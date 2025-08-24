#include "custom_visibility_buffer_shared.h"
#include "lights/lights.hlsl"
#include "math/math_constants.hlsl"

Texture2D g_TextureMaps[] : register(space99);
SamplerState g_TextureSampler;

#include "materials/materials.hlsl"

StructuredBuffer<DrawConstants> g_DrawConstants;
StructuredBuffer<uint> g_LightsCountBuffer;
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

float3 ambient(float3 normal)
{
    const float3 upVector = float3(0.0f, 1.0f, 0.0f);
    const float3 downVector = float3(-0.0f, -1.0f, -0.0f);

    const float3 upRadiance = 0.3f * float3(0.1f, 0.1f, 0.0f);
    const float3 downRadiance = 0.1f * float3(0.01f, 0.06f, 0.06f);

    float upFactor = max(0.0f, dot(normal, upVector));
    float downFactor = max(0.0f, dot(normal, downVector));

    return upFactor * upRadiance + downFactor * downRadiance;
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

// TODO support multiple lights.
// https://graphicrants.blogspot.com/2013/08/specular-brdf-reference.html
float3 calculateDirectLighting(MaterialEvaluated material, float3 normal, float3 viewDirection)
{
    const float3 albedo = material.albedo;
    const float metallic = material.metallicity;
    const float roughness = material.roughness;
    const float alpha = roughness * roughness;
    const float3 dielectricF0 = 0.04f;
    const float3 F0 = lerp(dielectricF0, albedo, metallic);

    const uint lightsCount = g_LightsCountBuffer[0];
    if (lightsCount == 0)
    {
        return float3(0.0f, 0.0f, 0.0f);
    }

    const LightDirectional directionalLight = MakeLightDirectional(g_LightsBuffer[0]);
    const float3 lightDirection = directionalLight.direction;
    const float3 irradiance = directionalLight.irradiance;

    const float3 halfVector = normalize(viewDirection + lightDirection);

    const float VdotH = dot(viewDirection, halfVector);
    const float NdotH = dot(normal, halfVector);
    const float NdotL = dot(normal, lightDirection);
    const float NdotV = dot(normal, viewDirection);

    const float3 Fresnel = calculateFresnelSchlick(VdotH, F0);
    const float NDF = calculateGGXNormalDistribution(NdotH, alpha);
    const float G = calculateSmithGeometry(NdotV, NdotL, alpha);

    const float3 specular = (Fresnel * NDF * G) / 4.0f;
    const float3 diffuse = (1.0f - Fresnel) * albedo / PI;

    return (specular + diffuse) * max(0.0f, NdotL) * irradiance;
}

float3 calculateIndirectLighting(MaterialEvaluated material, float3 normal, float3 viewDirection)
{
    const float3 albedo = material.albedo;
    const float metallic = material.metallicity;
    const float roughness = material.roughness;
    const float alpha = roughness * roughness;
    const float3 dielectricF0 = 0.04f;
    const float3 F0 = lerp(dielectricF0, albedo, metallic);

    const float VdotN = max(0.0f, dot(viewDirection, normal));
    const float3 Fresnel = calculateFresnelSchlickWithRoughness(VdotN, F0, alpha);
    const float3 diffuse = (1.0f - Fresnel) * albedo / PI * g_IrradianceProbe.SampleLevel(g_LinearSampler, normal, 0).rgb;

    const float C_PREFILTERED_MAP_MAX_MIP = 4.0f;
    const float prefilteredMapMip = roughness * C_PREFILTERED_MAP_MAX_MIP;
    const float3 reflectionVector = reflect(-viewDirection, normal);
    const float3 prefilteredColor = g_PrefilteredEnvironmentMap.SampleLevel(g_LinearSampler, reflectionVector, prefilteredMapMip).rgb;
    const float2 brdf = g_BrdfLut.SampleLevel(g_LinearSampler, float2(VdotN, roughness), 0.0f).xy;
    const float3 specular = Fresnel * prefilteredColor * (F0 * brdf.x + brdf.y);

    // TODO add AO.
    return diffuse + specular;
}

Pixel main(in VertexParams params, in uint instanceID : INSTANCE_ID)
{
    Pixel pixel;

    const float3 normal = normalize(params.normal);

    Instance instance = g_InstanceBuffer[instanceID];
    Material material = g_MaterialBuffer[instance.material_index];
    MaterialEvaluated materialEvaluated = MakeMaterialEvaluated(material, params.uv);

    const float3 cameraPosition = g_DrawConstants[0].cameraPosition.xyz;
    const float3 viewDirection = normalize(cameraPosition - params.worldPosition);

    // TODO add indirect specular.
    float3 radiance = calculateDirectLighting(materialEvaluated, normal, viewDirection) +
        calculateIndirectLighting(materialEvaluated, normal, viewDirection);

#if 0
    float3 color = radiance;
#else
    float3 color = tonemap(radiance);
    color = applyInverseGamma(color);
#endif

    pixel.color = float4(color, 1.0f);
    return pixel;
}
