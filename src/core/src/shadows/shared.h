#ifndef SHADOWS_H
#define SHADOWS_H

// Deduplicate these
struct ShadowConstants
{
    float4x4 viewProjection;
    uint drawCount;
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

#ifndef __cplusplus

uint packVPTInfo(uint physicalPageIndex)
{
    return ((physicalPageIndex % PAGE_TABLE_RESOLUTION_UINT) << 16) | ((physicalPageIndex / PAGE_TABLE_RESOLUTION_UINT) & 0xFFFF);
}

uint2 unpackVPTInfo(uint packed)
{
    return uint2(packed >> 16, packed & 0xFFFF);
}

float2 calculateVirtualTextureUv(float3 worldPosition, float4x4 lightViewProjection)
{
    float4 lightNdc = mul(lightViewProjection, float4(worldPosition, 1.0f));
    // Translation to the Sample Light NDC.
    lightNdc.xy -= lightViewProjection._m03_m13;

    float2 lightSpaceUv = lightNdc.xy * float2(0.5f, -0.5f) + 0.5f;
    lightSpaceUv = frac(lightSpaceUv);

    return lightSpaceUv;
}

float3 calculateVirtualTextureCoordinates(float3 worldPosition, float4x4 lightViewProjection)
{
    float4 lightNdc = mul(lightViewProjection, float4(worldPosition, 1.0f));
    // Translation to the Sample Light NDC.
    lightNdc.xy -= lightViewProjection._m03_m13;

    float2 lightSpaceUv = lightNdc.xy * float2(0.5f, -0.5f) + 0.5f;
    lightSpaceUv = frac(lightSpaceUv);

    return float3(lightSpaceUv, lightNdc.z);
}

#endif

#endif
