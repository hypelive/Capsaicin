#include "custom_visibility_buffer_shared.h"
#include "lights/lights.hlsl"
#include "math/math_constants.hlsl"

Texture2D g_TextureMaps[] : register(space99);
SamplerState g_TextureSampler;

#include "materials/materials.hlsl"

StructuredBuffer<DrawConstants> g_DrawConstants;
ConstantBuffer<LightsBufferInfo> g_LightsBufferInfo;
StructuredBuffer<Light> g_LightsBuffer;
StructuredBuffer<Instance> g_InstanceBuffer;
StructuredBuffer<Material> g_MaterialBuffer;

TextureCube<float4> g_IrradianceProbe;
TextureCube<float4> g_PrefilteredEnvironmentMap;
Texture2D<float2> g_BrdfLut;
SamplerState g_LinearSampler;

struct Pixel
{
    float4 color : SV_Target0;
};

void handleAlpha(Material material, float2 uv)
{
    // 0 - opaque, 1 - clip, 2 - blend.
    // Don't support alpha blending here, we'll have dedicated transparency pass.
    uint alphaBlendMode = asuint(material.normal_alpha_side.w);
    if (alphaBlendMode != 0)
    {
        const float C_APHA_CLIP_THRESHOLD = 0.5f;

        float alpha = material.normal_alpha_side.y;
        uint alphaMapIndex = asuint(material.albedo.w);
        if (alphaMapIndex != uint(-1))
        {
            alpha *= g_TextureMaps[NonUniformResourceIndex(alphaMapIndex)].Sample(g_LinearSampler, uv).a;
        }
        if (alpha < C_APHA_CLIP_THRESHOLD)
        {
            discard;
        }
    }
}

float3 calculateNormal(VertexParams params, PrimParams primitiveParams, Material material)
{
    float3 normal = normalize(params.normal);
    uint normalMap = asuint(material.normal_alpha_side.x);
    if (normalMap != uint(-1))
    {
        float3 tangent = normalize(primitiveParams.tangent);
        tangent = normalize(tangent - dot(tangent, normal) * normal);
        float3 bitangent = cross(normal, tangent);
        bitangent = dot(bitangent, primitiveParams.bitangent) < 0.0f ? -bitangent : bitangent;
        float3x3 TBN = float3x3(tangent.x, bitangent.x, normal.x,
                                tangent.y, bitangent.y, normal.y,
                                tangent.z, bitangent.z, normal.z);
        float3 textureNormal = 2.0f * g_TextureMaps[NonUniformResourceIndex(normalMap)].Sample(g_LinearSampler, params.uv).xyz - 1.0f;
        normal = normalize(mul(TBN, textureNormal));
    }

    return normal;
}

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

Pixel main(in VertexParams params, in PrimParams primitiveParams)
{
    Pixel pixel;

    Instance instance = g_InstanceBuffer[primitiveParams.instanceID];
    Material material = g_MaterialBuffer[instance.material_index];
    MaterialEvaluated materialEvaluated = MakeMaterialEvaluated(material, params.uv);
    MaterialBRDF materialBrdf = MakeMaterialBRDF(materialEvaluated);

    handleAlpha(material, params.uv);

    float3 normal = calculateNormal(params, primitiveParams, material);

    const float3 cameraPosition = g_DrawConstants[0].cameraPosition.xyz;
    const float3 viewDirection = normalize(cameraPosition - params.worldPosition);

    float3 radiance = calculateDirectLighting(materialBrdf, normal, viewDirection, cameraPosition, params.worldPosition) +
        calculateIndirectLighting(materialBrdf, normal, viewDirection);

#if 0
    float3 color = radiance;
#else
    float3 color = tonemap(radiance);
    color = applyInverseGamma(color);
#endif

    pixel.color = float4(color, 1.0f);
    return pixel;
}
