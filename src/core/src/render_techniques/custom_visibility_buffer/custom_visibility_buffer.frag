#include "custom_visibility_buffer_shared.h"
#include "math/math.hlsl"
#include "math/pack.hlsl"

Texture2D g_TextureMaps[] : register(space99);
SamplerState g_LinearSampler;

StructuredBuffer<VisibilityBufferConstants> g_DrawConstants;
StructuredBuffer<Instance> g_InstanceBuffer;
StructuredBuffer<Material> g_MaterialBuffer;

struct Pixel
{
    uint2 packedInstancePrimitive : SV_Target0;
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
            alpha *= g_TextureMaps[NonUniformResourceIndex(alphaMapIndex)].SampleLevel(g_LinearSampler, uv, 0.0f).a;
        }
        if (alpha < C_APHA_CLIP_THRESHOLD)
        {
            discard;
        }
    }
}

Pixel main(in VertexParams params, in PrimParams primitiveParams)
{
    Pixel pixel;

    Instance instance = g_InstanceBuffer[unpackVisibilityBuffer(primitiveParams.packedInstancePrimitive).instanceId];
    Material material = g_MaterialBuffer[instance.material_index];

    handleAlpha(material, params.uv);

    pixel.packedInstancePrimitive = primitiveParams.packedInstancePrimitive;
    return pixel;
}
