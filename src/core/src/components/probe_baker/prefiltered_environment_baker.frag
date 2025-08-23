#include "prefiltered_environment_baker_shared.h"
#include "math/math.hlsl"
#include "math/sampling.hlsl"

StructuredBuffer<float4x4> g_DrawConstants;
TextureCube g_EnvironmentMap;
SamplerState g_LinearSampler;
float2 g_invScreenSize;
uint g_FaceIndex;
float g_Roughness;

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
    const float3 normal = viewDirection;

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

    const uint C_SAMPLE_COUNT = 4096u;
    const float a = g_Roughness * g_Roughness;

    float3 irradiance = 0.0f;
    float totalWeight = 0.0f;
    for (uint sampleIndex = 0u; sampleIndex < C_SAMPLE_COUNT; ++sampleIndex)
    {
        float2 xi = Hammersley2D(sampleIndex, C_SAMPLE_COUNT);
	
        // https://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_notes_v2.pdf
        float phi = 2.0 * PI * xi.x;
        float2 sinCosTheta;
        sinCosTheta.y = sqrt((1.0 - xi.y) / (1.0 + (a * a - 1.0) * xi.y));
        sinCosTheta.x = sqrt(1.0 - sinCosTheta.y * sinCosTheta.y);
        float2 sinCosPhi;
        sincos(phi, sinCosPhi.x, sinCosPhi.y);

        float3 microsurfaceNormalDirection = float3(sinCosTheta.x * sinCosPhi.y, sinCosTheta.x * sinCosPhi.x, sinCosTheta.y);
        microsurfaceNormalDirection = mul(tangentMatrix, microsurfaceNormalDirection);
        float3 sampleDirection = -reflect(viewDirection, microsurfaceNormalDirection);
        float NdotL = max(0.0f, dot(normal, sampleDirection));
        if (NdotL > 0.0f)
        {
            // TODO fix aliasing artifacts (requires generating EnvMap mips) https://chetanjags.wordpress.com/2015/08/26/image-based-lighting/#:~:text=time%20method%20(above).-,Aliasing%C2%A0Artifacts,-We%20are%20using
            // TODO use Monte Carlo instead of weighted sum.
            irradiance += g_EnvironmentMap.SampleLevel(g_LinearSampler, sampleDirection, 0).xyz * NdotL;
            totalWeight += NdotL;
        }
    }
    
    irradiance = irradiance / totalWeight;
    pixel.color = float4(irradiance, 1.0f);

    return pixel;
}
