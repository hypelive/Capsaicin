#include "virtual_shadow_map_shared.h"
#include "math/math.hlsl"
#include "math/pack.hlsl"

Texture2D g_TextureMaps[] : register(space99);
SamplerState g_LinearSampler;

StructuredBuffer<RenderingConstants> g_DrawConstants;
StructuredBuffer<Instance> g_InstanceBuffer;
StructuredBuffer<Material> g_MaterialBuffer;
RWTexture2D<uint> g_PhysicalPagesUav;
uint g_ClipmapIndex;

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
    float3 lightNdc = calculateLightNdc(worldPosition, g_DrawConstants[0].viewProjection, g_ClipmapIndex);
    float3 virtualTextureUv = calculateVirtualTextureUv(lightNdc, g_DrawConstants[0].viewProjection, g_ClipmapIndex);
    uint2 virtualTextureCoordinates = CASCADE_RESOLUTION * virtualTextureUv.xy;
    uint2 pageTableCoordinates = virtualTextureCoordinates / PAGE_RESOLUTION_UINT;
    uint2 textureCoordinatesInsidePage = virtualTextureCoordinates % PAGE_RESOLUTION_UINT;

    uint virtualPageData = g_VirtualPageTable[uint3(pageTableCoordinates, g_ClipmapIndex)];
    if (!isVisible(virtualPageData) || !isBacked(virtualPageData))
    {
        return float4(0, 0, 0, 0);
    }

    uint2 physicalTextureCoordinates = unpackVPTData(virtualPageData).physicalCoordinates;
    uint oldValue;
    // TODO add slope bias
    const float BIAS = 1e-3f;
    InterlockedMin(g_PhysicalPagesUav[PAGE_RESOLUTION_UINT * physicalTextureCoordinates + textureCoordinatesInsidePage], asuint(params.screenPosition.z + BIAS), oldValue);

    return float4(params.screenPosition.zzz, 1);
}
