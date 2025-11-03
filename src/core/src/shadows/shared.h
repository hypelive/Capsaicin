#ifndef SHADOWS_H
#define SHADOWS_H

#include "gpu_shared.h"

struct ShadowConstants
{
    float4x4 viewProjection;
};

static const float CASCADE_RESOLUTION = 64.0f * 128.0f;
static const uint CASCADE_RESOLUTION_UINT = (uint)CASCADE_RESOLUTION;
static const float PAGE_RESOLUTION = 128.0f;
static const uint PAGE_RESOLUTION_UINT = (uint)PAGE_RESOLUTION;
static const float PAGE_TABLE_RESOLUTION = CASCADE_RESOLUTION / PAGE_RESOLUTION;
static const uint PAGE_TABLE_RESOLUTION_UINT = (uint)PAGE_TABLE_RESOLUTION;
static const float PAGE_UV = PAGE_RESOLUTION / CASCADE_RESOLUTION;
static const float PAGE_NDC = 2.0f * PAGE_UV;
static const float CASCADE_SIZE_0 = 2.0f;
static const float CASCADES_NUM = 8.0f;
static const uint CASCADES_NUM_UINT = (uint)CASCADES_NUM;
static const uint VPT_CLEAR_VALUE = 0x00FFFFFFu;
static const float PP_CLEAR_VALUE = 1024.0f * 1024.0f;

// TODO remove this variable, allocate dynamically
static const uint PHYSICAL_TEXTURE_RESOLUTION = CASCADE_RESOLUTION_UINT << 1u;
static const uint PHYSICAL_PAGES_BUFFER_RESOLUTION = PHYSICAL_TEXTURE_RESOLUTION / PAGE_RESOLUTION_UINT;
static const uint PHYSICAL_PAGES_BUFFER_RESOLUTION_SQR = PHYSICAL_PAGES_BUFFER_RESOLUTION * PHYSICAL_PAGES_BUFFER_RESOLUTION;

#ifndef __cplusplus

// These parameters are set up through ShadowStructures component.
ConstantBuffer<ShadowConstants> g_ShadowConstants;
Texture2DArray<uint> g_VirtualPageTable;
Texture2D<uint> g_PhysicalPages;

struct VPTData 
{
    uint2 physicalCoordinates;
    uint frameCounter;
    uint isVisible;
    uint isValid;
};

bool isBacked(uint data)
{
    return (data & 0xFFFFFF) != 0xFFFFFF;
}

bool isValid(uint data)
{
    return data >> 31;
}

bool isValid(VPTData data)
{
    return data.isValid;
}

static const uint VISIBLE_BIT_MASK = 1u << 30u;
static const uint FRAME_COUNTER_MASK = 0xFu << 26u;

bool isVisible(uint data)
{
    return data & VISIBLE_BIT_MASK;
}

// 1 valid 1 visible 4 frameCounter 2 reserved 12 physical y 12 physical x
uint packVPTData(VPTData data)
{
    return ((data.isValid & 0x1) << 31) | ((data.isVisible & 0x1) << 30) | ((data.frameCounter & 0xF) << 26) | ((data.physicalCoordinates.y & 0xFFF) << 12) | (data.physicalCoordinates.x & 0xFFF);
}

VPTData unpackVPTData(uint packed)
{
    VPTData result;
    result.isValid = isValid(packed);
    result.isVisible = isVisible(packed);
    result.frameCounter = (packed >> 26) & 0xF;
    result.physicalCoordinates = uint2(packed & 0xFFF, (packed >> 12) & 0xFFF);

    return result;
}

uint resetVisible(uint data)
{
    VPTData unpackedData = unpackVPTData(data);
    if (unpackedData.frameCounter > 0)
    {
        unpackedData.frameCounter -= 1;
    }
    unpackedData.isVisible = 0;
    return packVPTData(unpackedData);
}

float3 calculateLightNdc(float3 worldPosition, float4x4 lightViewProjection, uint clipmapIndex)
{
    float3 lightNdc = mul(lightViewProjection, float4(worldPosition, 1.0f)).xyz;
    lightNdc.xy /= (float)(1u << clipmapIndex);
    return lightNdc;
}

float3 calculateVirtualTextureUv(float3 lightNdc, float4x4 lightViewProjection, uint clipmapIndex)
{
    // Translation to the Sample Light NDC.
    lightNdc.xy -= lightViewProjection._m03_m13 / (1u << clipmapIndex);

    float2 lightSpaceUv = lightNdc.xy * float2(0.5f, -0.5f) + 0.5f;
    lightSpaceUv = frac(lightSpaceUv);

    return float3(lightSpaceUv, lightNdc.z);
}

uint calculateClipmapIndexUnbound(float3 lightNdc)
{
    return max(
        ceil(log2(max(abs(lightNdc.x), 1.0f))),
        ceil(log2(max(abs(lightNdc.y), 1.0f)))
    );
}

uint calculateClipmapIndex(float3 lightNdc)
{
    uint clipmapIndex = calculateClipmapIndexUnbound(lightNdc);
    clipmapIndex = min(CASCADES_NUM_UINT - 1, clipmapIndex);

    return clipmapIndex;
}

float sampleShadowFactor(float3 worldPosition)
{
    float3 lightNdc0 = calculateLightNdc(worldPosition, g_ShadowConstants.viewProjection, 0);
    uint clipmapIndex = calculateClipmapIndexUnbound(lightNdc0);

    if (clipmapIndex >= CASCADES_NUM_UINT)
    {
        return 1.0f;
    }

    float3 lightNdc = lightNdc0;
    lightNdc.xy /= (1u << clipmapIndex);

    float3 virtualTextureUv = calculateVirtualTextureUv(lightNdc, g_ShadowConstants.viewProjection, clipmapIndex);
    uint2 virtualTextureCoordinates = CASCADE_RESOLUTION * virtualTextureUv.xy;
    uint2 pageTableCoordinates = virtualTextureCoordinates / PAGE_RESOLUTION_UINT;
    uint2 textureCoordinatesInsidePage = virtualTextureCoordinates % PAGE_RESOLUTION_UINT;

    uint virtualPageData = g_VirtualPageTable[uint3(pageTableCoordinates, clipmapIndex)];
    if (isValid(virtualPageData))
    {
        uint2 physicalTextureCoordinates = unpackVPTData(virtualPageData).physicalCoordinates;
        float shadowMapDepth = asfloat(g_PhysicalPages.Load(int3(PAGE_RESOLUTION_UINT * physicalTextureCoordinates + textureCoordinatesInsidePage, 0)));
        if (virtualTextureUv.z > shadowMapDepth)
        {
            return 0.0f;
        }
    }
    
    return 1.0f;
}

#endif

#endif
