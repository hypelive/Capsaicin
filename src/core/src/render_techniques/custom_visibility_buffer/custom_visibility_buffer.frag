#include "custom_visibility_buffer_shared.h"
#include "lights/lights.hlsl"

StructuredBuffer<uint> g_LightsCountBuffer;
StructuredBuffer<Light> g_LightsBuffer;

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

Pixel main(in VertexParams params, in uint instanceID : INSTANCE_ID)
{
    Pixel pixel;

    const float3 normal = normalize(params.normal);

    float3 radiance = lambert(normal) + ambient(normal);

    float3 color = tonemap(radiance);
    color = applyInverseGamma(color);

    pixel.color = float4(color, 1.0f);
    return pixel;
}
