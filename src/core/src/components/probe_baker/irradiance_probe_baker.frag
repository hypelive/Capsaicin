#include "irradiance_probe_baker_shared.h"
#include "math/math.hlsl"

StructuredBuffer<float4x4> g_DrawConstants;
TextureCube g_EnvironmentMap;
SamplerState g_LinearSampler;
float2 g_invScreenSize;
uint g_FaceIndex;

struct Pixel
{
    float4 color : SV_Target0;
};

Pixel main(in VertexParams params)
{
    Pixel pixel;

    float2 uv = params.screenPosition.xy * g_invScreenSize;
    float2 ndc = 2.0f * uv - 1.0f;

    // Translate point from near plane (we have inversed depth) to the world space.
    float4 worldPosition = mul(g_DrawConstants[g_FaceIndex], float4(ndc, 1.0f, 1.0f));
    worldPosition.xyz /= worldPosition.w;
    const float3 viewDirection = normalize(worldPosition.xyz);

    // Should be in sync with the .cpp.
    float3 upVectors[6] = {
        float3(0.0f, 1.0f, 0.0f),
        float3(0.0f, 1.0f, 0.0f),
        float3(0.0f, 0.0f, -1.0f),
        float3(0.0f, 0.0f, 1.0f),
        float3(0.0f, 1.0f, 0.0f),
        float3(0.0f, 1.0f, 0.0f)
    };

    float3 tangentDirection = upVectors[g_FaceIndex];
    tangentDirection -= dot(tangentDirection, viewDirection) * viewDirection;
    tangentDirection = normalize(tangentDirection);
    float3 bitangentDirection = cross(viewDirection, tangentDirection);

    float3x3 tangentMatrix = float3x3(
        tangentDirection.x, bitangentDirection.x, viewDirection.x, // row 1
        tangentDirection.y, bitangentDirection.y, viewDirection.y, // row 2
        tangentDirection.z, bitangentDirection.z, viewDirection.z  // row 3
    );

    const float thetaStep = HALF_PI / 64.0f;
    const float phiStep = TWO_PI / 64.0f;

    float3 irradiance = 0.0f;
    for (float theta = thetaStep * 0.5f; theta < HALF_PI; theta += thetaStep)
    {
        for (float phi = phiStep * 0.5f; phi < TWO_PI; phi += phiStep)
        {
            float2 sinCosTheta;
            sincos(theta, sinCosTheta.x, sinCosTheta.y);
            float2 sinCosPhi;
            sincos(phi, sinCosPhi.x, sinCosPhi.y);

            float3 sampleDirection = float3(sinCosTheta.x * sinCosPhi.y, sinCosTheta.x * sinCosPhi.x, sinCosTheta.y);
            sampleDirection = mul(tangentMatrix, sampleDirection);

            float3 environmentSample = g_EnvironmentMap.SampleLevel(g_LinearSampler, sampleDirection, 0).xyz;
            // Riemann sum.
            irradiance += (environmentSample * sinCosTheta.y) * (sinCosTheta.x * phiStep * thetaStep);
        }
    }
    
    // Sample the environment map
    pixel.color = float4(irradiance, 1.0f);

    return pixel;
}
