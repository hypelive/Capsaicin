#include "custom_shading_shared.h"
#include "math/math.hlsl"
#include "math/color.hlsl"
#include "math/pack.hlsl"

ConstantBuffer<ShadingConstants> g_DrawConstants;
Texture2D<uint> g_VisibilityBuffer;

struct Pixel
{
    float4 color : SV_Target0;
};

Pixel main(in VertexParams params)
{
    Pixel pixel;

    uint2 visibilityBufferData = unpackVisibilityBuffer(g_VisibilityBuffer.Load(int3(params.screenPosition.xy, 0)));
    uint instanceId = visibilityBufferData.x;
    uint primitiveId = visibilityBufferData.y;

    // Shade vertices.
    // Calc radiance.

    float3 lutColor[8] = {
        float3(1.0f, 0.0f, 0.0f),
        float3(0.0f, 1.0f, 0.0f),
        float3(0.0f, 0.0f, 1.0f),
        float3(1.0f, 1.0f, 0.0f),
        float3(1.0f, 0.0f, 1.0f),
        float3(0.0f, 1.0f, 1.0f),
        float3(1.0f, 1.0f, 1.0f),
        float3(0.5f, 0.5f, 0.5f)
    };

    pixel.color = float4(lutColor[instanceId % 8], 1.0f);

    return pixel;
}
