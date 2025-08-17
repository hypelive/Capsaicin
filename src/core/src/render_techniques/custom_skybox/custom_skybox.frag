#include "custom_skybox_shared.h"
#include "math/math.hlsl"

StructuredBuffer<DrawConstants> g_DrawConstants;
TextureCube g_EnvironmentMap;
SamplerState g_LinearSampler;

struct Pixel
{
    float4 color : SV_Target0;
};

Pixel main(in VertexParams params)
{
    Pixel pixel;

    float2 uv = params.screenPosition.xy * g_DrawConstants[0].invScreenSize;
    float2 ndc = 2.0f * float2(uv.x, 1.0f - uv.y) - 1.0f;

    // Translate point from near plane (we have inversed depth) to the world space.
    float4 worldPosition = mul(g_DrawConstants[0].invViewProjection, float4(ndc, 1.0f, 1.0f));
    worldPosition.xyz /= worldPosition.w;
    const float3 viewDirection = normalize(worldPosition.xyz - g_DrawConstants[0].cameraPosition);
    
    // Sample the environment map
    pixel.color = float4(g_EnvironmentMap.SampleLevel(g_LinearSampler, viewDirection, 0).xyz, 1.0f);

    return pixel;
}
