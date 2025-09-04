#include "custom_shading_shared.h"
#include "math/math.hlsl"
#include "math/color.hlsl"
#include "math/pack.hlsl"
#include "math/transform.hlsl"
#include "lights/lights.hlsl"

Texture2D g_TextureMaps[] : register(space99);
SamplerState g_TextureSampler;

#include "materials/materials.hlsl"

ConstantBuffer<ShadingConstants> g_DrawConstants;
Texture2D<float4> g_GBuffer0;
Texture2D<float4> g_GBuffer1;
Texture2D<float4> g_GBuffer2;
Texture2D<float> g_DepthCopy;
ConstantBuffer<LightsBufferInfo> g_LightsBufferInfo;
StructuredBuffer<Light> g_LightsBuffer;
TextureCube<float4> g_IrradianceProbe;
TextureCube<float4> g_PrefilteredEnvironmentMap;
Texture2D<float2> g_BrdfLut;
SamplerState g_LinearSampler;

struct Pixel
{
    float4 color : SV_Target0;
};

float3 tonemap(float3 radiance)
{
    return radiance / (1.0f + radiance);
}

float3 applyInverseGamma(float3 color)
{
    return pow(color, 1.0f / 2.2f);
}

float3 calculateFresnelSchlick(float VdotH, float3 F0)
{
    return F0 + (1.0f - F0) * pow(saturate(1.0f - VdotH), 5.0f);
}

// https://seblagarde.wordpress.com/2011/08/17/hello-world/
float3 calculateFresnelSchlickWithRoughness(float VdotN, float3 F0, float roughness)
{
    return F0 + (max(1.0f - roughness, F0) - F0) * pow(saturate(1.0f - VdotN), 5.0f);
}

float calculateGGXNormalDistribution(float NdotH, float alpha)
{
    const float alpha2 = alpha * alpha;
    const float numerator = alpha2;
    const float temp = (NdotH * NdotH * (alpha2 - 1.0f) + 1.0f);
    const float denominator = PI * temp * temp;
    return numerator / (denominator + FLT_EPSILON);
}

float calculateSchlickGGXGeometry(float NdotV, float alpha)
{
    const float k = alpha / 2.0f;
    const float denominator = NdotV * (1.0f - k) + k;
    return rcp(denominator + FLT_EPSILON);
}

float calculateSmithGeometry(float NdotV, float NdotL, float alpha)
{
    return calculateSchlickGGXGeometry(saturate(NdotV), alpha) * calculateSchlickGGXGeometry(saturate(NdotL), alpha);
}

// https://graphicrants.blogspot.com/2013/08/specular-brdf-reference.html
float3 calculateDirectRadianceForLight(MaterialBRDF material, float3 normal, float3 viewDirection, float3 lightDirection, float3 radiance)
{
    const float3 halfVector = normalize(viewDirection + lightDirection);

    const float VdotH = dot(viewDirection, halfVector);
    const float NdotH = dot(normal, halfVector);
    const float NdotL = dot(normal, lightDirection);
    const float NdotV = dot(normal, viewDirection);

    const float3 Fresnel = calculateFresnelSchlick(VdotH, material.F0);
    const float NDF = calculateGGXNormalDistribution(NdotH, material.roughnessAlpha);
    const float G = calculateSmithGeometry(NdotV, NdotL, material.roughnessAlpha);

    const float3 specular = (Fresnel * NDF * G) / 4.0f;
    const float3 diffuse = (1.0f - Fresnel) * material.albedo / PI;

    return (specular + diffuse) * max(0.0f, NdotL) * radiance;
}

float3 calculateDirectLighting(MaterialBRDF material, float3 normal, float3 viewDirection, float3 cameraPosition, float3 worldPosition)
{
    float3 radiance = 0.0f;

    // Evaluate directional lights.
    for (uint lightIndex = 0u; lightIndex < g_LightsBufferInfo.directionalLightsCount; ++lightIndex)
    {
        const LightDirectional directionalLight = MakeLightDirectional(g_LightsBuffer[lightIndex]);
        radiance += calculateDirectRadianceForLight(material, normal, viewDirection,
            directionalLight.direction, directionalLight.irradiance);
    }

    // Evaluate point lights.
    for (uint lightIndex = 0u; lightIndex < g_LightsBufferInfo.pointLightsCount; ++lightIndex)
    {
        const LightPoint pointLight = MakeLightPoint(g_LightsBuffer[g_LightsBufferInfo.pointLightsOffset + lightIndex]);
        const float3 lightOffset = pointLight.position - worldPosition;
        const float sqrDistance = dot(lightOffset, lightOffset);
        if (sqrDistance > pointLight.range * pointLight.range)
        {
            continue;
        }

        const float attenuation = rcp(PI * sqrDistance);

        radiance += attenuation * calculateDirectRadianceForLight(material, normal, viewDirection,
            lightOffset / sqrt(sqrDistance), pointLight.intensity);
    }
    
    // Evaluate spot lights.
    for (uint lightIndex = 0u; lightIndex < g_LightsBufferInfo.spotLightsCount; ++lightIndex)
    {
        const LightSpot spotLight = MakeLightSpot(g_LightsBuffer[g_LightsBufferInfo.spotLightsOffset + lightIndex]);
        const float3 lightOffset = spotLight.position - worldPosition;
        const float sqrDistance = dot(lightOffset, lightOffset);
        if (sqrDistance > spotLight.range * spotLight.range)
        {
            continue;
        }

        const float3 lightDirection = lightOffset / sqrt(sqrDistance);
        const float cosTheta = dot(lightDirection, spotLight.direction);

        const float distanceAttenuation = rcp(PI * sqrDistance);
        float angleAttenuation = saturate(cosTheta * spotLight.angleCutoffScale + spotLight.angleCutoffOffset);
        angleAttenuation *= angleAttenuation;
        if (angleAttenuation <= 0.0f)
        {
            continue;
        }

        radiance += distanceAttenuation * angleAttenuation * calculateDirectRadianceForLight(material, normal, viewDirection,
            lightDirection, spotLight.intensity);
    }

    // TODO Evaluate area lights.

    return radiance;
}

float3 calculateIndirectLighting(MaterialBRDF material, float3 normal, float3 viewDirection)
{
    const float VdotN = max(0.0f, dot(viewDirection, normal));
    const float3 Fresnel = calculateFresnelSchlickWithRoughness(VdotN, material.F0, material.roughnessAlpha);
    const float3 diffuse = (1.0f - Fresnel) * material.albedo / PI * g_IrradianceProbe.SampleLevel(g_LinearSampler, normal, 0).rgb;

    const float C_PREFILTERED_MAP_MAX_MIP = 4.0f;
    const float prefilteredMapMip = material.roughnessAlpha * C_PREFILTERED_MAP_MAX_MIP;
    const float3 reflectionVector = reflect(-viewDirection, normal);
    const float3 prefilteredColor = g_PrefilteredEnvironmentMap.SampleLevel(g_LinearSampler, reflectionVector, prefilteredMapMip).rgb;
    const float2 brdf = g_BrdfLut.SampleLevel(g_LinearSampler, float2(VdotN, material.roughnessAlpha), 0.0f).xy;
    const float3 specular = Fresnel * prefilteredColor * (material.F0 * brdf.x + brdf.y);

    // TODO add AO.
    return diffuse + specular;
}

struct GBufferData
{
    float4 albedo;
    float4 normalRoughnessMetallicity;
    float4 emission;
};

GBufferData readGBuffer(float2 texelCoordinates)
{
    GBufferData data;
    data.albedo = g_GBuffer0.Load(int3(texelCoordinates, 0));
    data.normalRoughnessMetallicity = g_GBuffer1.Load(int3(texelCoordinates, 0));
    data.emission = g_GBuffer2.Load(int3(texelCoordinates, 0));

    return data;
}

Pixel main(in VertexParams params)
{
    float2 uv = params.screenPosition.xy * g_DrawConstants.invScreenSize;
    float2 ndc = 2.0f * float2(uv.x, 1.0f - uv.y) - 1.0f;

    GBufferData gBuffer = readGBuffer(params.screenPosition.xy);

    MaterialEvaluated materialEvaluated;
    materialEvaluated.albedo = gBuffer.albedo.xyz;
    materialEvaluated.metallicity = gBuffer.normalRoughnessMetallicity.w;
    materialEvaluated.roughness = gBuffer.normalRoughnessMetallicity.z;
    MaterialBRDF materialBrdf = MakeMaterialBRDF(materialEvaluated);

    float3 normal;
    normal.xy = gBuffer.normalRoughnessMetallicity.xy * 2.0f - 1.0f;
    normal.z = sqrt(1 - normal.x * normal.x - normal.y * normal.y);

    float pixelDepth = g_DepthCopy.Load(int3(params.screenPosition.xy, 0));
    float4 screenSpacePosition = float4(ndc, pixelDepth, 1.0f);
    float4 worldPosition = mul(g_DrawConstants.invViewProjection, screenSpacePosition);
    worldPosition.xyz /= worldPosition.w;

    const float3 cameraPosition = g_DrawConstants.cameraPosition.xyz;
    const float3 viewDirection = normalize(cameraPosition - worldPosition.xyz);

    float3 radiance = gBuffer.emission.xyz +
        calculateDirectLighting(materialBrdf, normal, viewDirection, cameraPosition, worldPosition.xyz) +
        calculateIndirectLighting(materialBrdf, normal, viewDirection);

    float3 color = tonemap(radiance);
    color = applyInverseGamma(color);

    Pixel pixel;
    pixel.color = float4(color, 1.0f);

    return pixel;
}
