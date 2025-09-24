#ifndef SHADOWS_H
#define SHADOWS_H

#include "gpu_shared.h"

struct ShadowConstants
{
    float4x4 viewProjection;
};

static const float CASCADE_RESOLUTION = 8.0f * 1024.0f;
static const uint CASCADE_RESOLUTION_UINT = (uint)(8.0f * 1024.0f);
static const float PAGE_RESOLUTION = 128.0f;
static const uint PAGE_RESOLUTION_UINT = (uint)PAGE_RESOLUTION;
static const float PAGE_TABLE_RESOLUTION = CASCADE_RESOLUTION / PAGE_RESOLUTION;
static const uint PAGE_TABLE_RESOLUTION_UINT = (uint)PAGE_TABLE_RESOLUTION;
static const float PAGE_UV = PAGE_RESOLUTION / CASCADE_RESOLUTION;
static const float PAGE_NDC = 2.0f * PAGE_UV;
static const float CASCADE_SIZE_0 = 16.0f;
static const uint VPT_CLEAR_VALUE = 0xFFFFFFFFu;
static const float PP_CLEAR_VALUE = 1.0f;

#ifndef __cplusplus

// These parameters are set up through ShadowStructures component.
ConstantBuffer<ShadowConstants> g_ShadowConstants;
Texture2D<uint> g_VirtualPageTable;
Texture2D<uint> g_PhysicalPages;

bool isValid(uint data)
{
    return data != VPT_CLEAR_VALUE;
}

// Zero is invalid, so real pages start from one.
uint packVPTInfo(uint physicalPageIndex)
{
    return ((physicalPageIndex % PAGE_TABLE_RESOLUTION_UINT) << 16) | ((physicalPageIndex / PAGE_TABLE_RESOLUTION_UINT) & 0xFFFF);
}

uint2 unpackVPTInfo(uint packed)
{
    return uint2(packed >> 16, packed & 0xFFFF);
}

float3 calculateVirtualTextureUv(float3 worldPosition, float4x4 lightViewProjection)
{
    float4 lightNdc = mul(lightViewProjection, float4(worldPosition, 1.0f));
    // Translation to the Sample Light NDC.
    lightNdc.xy -= lightViewProjection._m03_m13;

    float2 lightSpaceUv = lightNdc.xy * float2(0.5f, -0.5f) + 0.5f;
    lightSpaceUv = frac(lightSpaceUv);

    return float3(lightSpaceUv, lightNdc.z);
}

float sampleShadowFactor(float3 worldPosition)
{
    float3 virtualTextureUv = calculateVirtualTextureUv(worldPosition, g_ShadowConstants.viewProjection);
    uint2 virtualTextureCoordinates = CASCADE_RESOLUTION * virtualTextureUv.xy;
    uint2 pageTableCoordinates = virtualTextureCoordinates / PAGE_RESOLUTION_UINT;
    uint2 textureCoordinatesInsidePage = virtualTextureCoordinates % PAGE_RESOLUTION_UINT;

    uint virtualPageData = g_VirtualPageTable[pageTableCoordinates];
    if (isValid(virtualPageData))
    {
        uint2 physicalTextureCoordinates = unpackVPTInfo(virtualPageData);
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
