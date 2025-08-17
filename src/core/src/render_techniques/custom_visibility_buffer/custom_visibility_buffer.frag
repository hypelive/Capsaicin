#include "custom_visibility_buffer_shared.h"
#include "lights/lights.hlsl"
#include "math/math_constants.hlsl"

StructuredBuffer<DrawConstants> g_DrawConstants;
StructuredBuffer<uint> g_LightsCountBuffer;
StructuredBuffer<Light> g_LightsBuffer;
StructuredBuffer<Instance> g_InstanceBuffer;
StructuredBuffer<Material> g_MaterialBuffer;

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

float3 calculateFresnelSchlick(float VdotH, float3 albedo, float metallic)
{
    const float3 F0 = 0.04f;
    const float3 effectiveF0 = lerp(F0, albedo, metallic);
    return effectiveF0 + (1.0f - effectiveF0) * pow(clamp(1 - VdotH, 0.0f, 1.0f), 5.0f);
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
float3 directionalPBR(Material material, float3 normal, float3 viewDirection)
{
    const float3 albedo = material.albedo.rgb;
    const float metallic = material.metallicity_roughness.x;
    const float roughness = material.metallicity_roughness.z;
    const float alpha = roughness * roughness;

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

    const float3 Fresnel = calculateFresnelSchlick(VdotH, albedo, metallic);
    const float NDF = calculateGGXNormalDistribution(NdotH, alpha);
    const float G = calculateSmithGeometry(NdotV, NdotL, alpha);

    const float3 specular = (Fresnel * NDF * G) / 4.0f;
    const float3 diffuse = (1.0f - Fresnel) * albedo / PI;

    return (specular + diffuse) * NdotL * irradiance;
}

Pixel main(in VertexParams params, in uint instanceID : INSTANCE_ID)
{
    Pixel pixel;

    const float3 normal = normalize(params.normal);

    Instance instance = g_InstanceBuffer[instanceID];
    Material material = g_MaterialBuffer[instance.material_index];

    const float3 cameraPosition = g_DrawConstants[0].cameraPosition.xyz;
    const float3 viewDirection = normalize(cameraPosition - params.worldPosition);

    float3 radiance = directionalPBR(material, normal, viewDirection) + ambient(normal);

    float3 color = tonemap(radiance);
    color = applyInverseGamma(color);

    pixel.color = float4(color, 1.0f);
    return pixel;
}
