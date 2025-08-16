#include "custom_visibility_buffer_shared.h"
#include "lights/lights.hlsl"

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

    const float3 upRadiance = 0.3f * float3(1.0f, 1.0f, 0.0f);
    const float3 downRadiance = 0.1f * float3(0.1f, 0.6f, 0.6f);

    float upFactor = max(0.0f, dot(normal, upVector));
    float downFactor = max(0.0f, dot(normal, downVector));

    // Turned off for now.
    return 0.0f;
    return upFactor * upRadiance + downFactor * downRadiance;
}

float3 lambert(float3 normal)
{
    const uint lightsCount = g_LightsCountBuffer[0];
    if (lightsCount == 0)
    {
        return float3(0.0f, 0.0f, 0.0f);
    }

    const LightDirectional directionalLight = MakeLightDirectional(g_LightsBuffer[0]);
    const float3 lightDirection = directionalLight.direction;
    const float3 irradiance = directionalLight.irradiance;

    return max(0.0f, dot(normal, lightDirection)) * irradiance;
}

float3 tonemap(float3 radiance)
{
    return radiance / (1.0f + radiance);
}

float3 applyInverseGamma(float3 color)
{
    return pow(color, 1.0f / 2.2f);
}

float3 calculateFresnelSchlick(float HdotV, float3 albedo, float metallic)
{
    const float3 F0 = 0.04f;
    const float3 effectiveF0 = lerp(F0, albedo, metallic);
    return effectiveF0 + (1.0f - effectiveF0) * pow(clamp(1 - HdotV, 0.0f, 1.0f), 5.0f);
}

float3 directionalPBR(Material material, float3 normal, float3 viewDirection)
{
    const float3 albedo = material.albedo.rgb;
    const float metallic = material.metallicity_roughness.x;
    const float roughness = material.metallicity_roughness.z;

    const uint lightsCount = g_LightsCountBuffer[0];
    if (lightsCount == 0)
    {
        return float3(0.0f, 0.0f, 0.0f);
    }

    const LightDirectional directionalLight = MakeLightDirectional(g_LightsBuffer[0]);
    const float3 lightDirection = directionalLight.direction;
    const float3 irradiance = directionalLight.irradiance;

    const float3 halfVector = normalize(viewDirection + lightDirection);
    const float3 Fresnel = calculateFresnelSchlick(dot(halfVector, viewDirection), albedo, metallic);

    return Fresnel;
}

Pixel main(in VertexParams params, in uint instanceID : INSTANCE_ID)
{
    Pixel pixel;

    const float3 normal = normalize(params.normal);

    Instance instance = g_InstanceBuffer[instanceID];
    // TODO perhaps the material index can be invalid. We need to handle that case.
    Material material = g_MaterialBuffer[instance.material_index];

    const float3 cameraPosition = g_DrawConstants[0].cameraPosition.xyz;
    const float3 viewDirection = normalize(cameraPosition - params.worldPosition);

    float3 radiance = directionalPBR(material, normal, viewDirection) + ambient(normal);

#if 0
    float3 color = radiance;
#else
    float3 color = tonemap(radiance);
    color = applyInverseGamma(color);
#endif

    pixel.color = float4(color, 1.0f);
    return pixel;
}
