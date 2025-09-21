#include "virtual_shadow_map_shared.h"
#include "math/math.hlsl"
#include "math/pack.hlsl"

Texture2D g_TextureMaps[] : register(space99);
SamplerState g_LinearSampler;

StructuredBuffer<RenderingConstants> g_DrawConstants;
StructuredBuffer<Instance> g_InstanceBuffer;
StructuredBuffer<Material> g_MaterialBuffer;
Texture2D<uint> g_VirtualPageTable;
RWTexture2D<uint> g_PhysicalPages;

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

float4 main(in VertexParams params, in PrimParams primitiveParams) : SV_Target0
{
    Instance instance = g_InstanceBuffer[unpackVisibilityBuffer(primitiveParams.packedInstancePrimitive).instanceId];
    Material material = g_MaterialBuffer[instance.material_index];

    handleAlpha(material, params.uv);

    // TODO move culling to the earlier steps.
    float3 worldPosition = params.worldPosition;
    float2 virtualTextureUv = calculateVirtualTextureUv(worldPosition, g_DrawConstants[0].viewProjection);
    uint2 virtualTextureCoordinates = CASCADE_RESOLUTION * virtualTextureUv;
    uint2 pageTableCoordinates = virtualTextureCoordinates / PAGE_RESOLUTION_UINT;
    uint2 textureCoordinatesInsidePage = virtualTextureCoordinates % PAGE_RESOLUTION_UINT;

    uint virtualPageData = g_VirtualPageTable[pageTableCoordinates];
    if (!virtualPageData)
    {
        // Page isn't visible.
        return float4(0,0,0,0);
    }

    uint2 physicalTextureCoordinates = unpackVPTInfo(virtualPageData);
    uint oldValue;
    // TODO add slope bias
    const float BIAS = 1e-3f;
    InterlockedMin(g_PhysicalPages[PAGE_RESOLUTION_UINT * physicalTextureCoordinates + textureCoordinatesInsidePage], asuint(params.screenPosition.z + BIAS), oldValue);

    return float4(params.screenPosition.zzz,1);
}
