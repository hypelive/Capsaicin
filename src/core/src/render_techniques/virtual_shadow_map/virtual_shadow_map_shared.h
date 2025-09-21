#ifndef VSM_SHARED_H
#define VSM_SHARED_H

#include "gpu_shared.h"

#define TILE_SIZE 8
#define TILE_SIZE_SQR 64

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

struct VertexParams
{
    float4 screenPosition : SV_Position;
    float2 uv : TEXCOORD;
    float3 worldPosition : WORLD_POSITION;
};

struct PrimParams
{
    uint2 packedInstancePrimitive : INSTANCE_PRIMITIVE_ID;
};

#define MESH_GROUP_SIZE 32
struct MeshPayload
{
    uint meshletIndex[MESH_GROUP_SIZE];
    uint instanceIndex[MESH_GROUP_SIZE];
};

uint packVPTInfo(uint physicalPageIndex)
{
    return ((physicalPageIndex % PAGE_TABLE_RESOLUTION_UINT) << 16) | ((physicalPageIndex / PAGE_TABLE_RESOLUTION_UINT) & 0xFFFF);
}

uint2 unpackVPTInfo(uint packed)
{
    return uint2(packed >> 16, packed & 0xFFFF);
}

float3 reconstructWorldPosition(float2 uv, float depth, float4x4 invViewProjection)
{
    float2 ndc = 2.0f * float2(uv.x, 1.0f - uv.y) - 1.0f;

    float4 worldPosition = mul(invViewProjection, float4(ndc, depth, 1.0f));
    worldPosition /= worldPosition.w;

    return worldPosition.xyz;
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

#endif

struct VSMConstants
{
    float4x4 viewProjection;
    float4x4 invViewProjection;
    float4x4 lightViewProjection;
    float4   screenSize;
    float3   cameraPosition;
    uint     padding;
    uint4    viewport;
};

struct AllocationsState
{
    // From 0 to PAGE_TABLE_RESOLUTION * PAGE_TABLE_RESOLUTION - 1.
    uint allocationIndex;
};

struct RenderingConstants
{
    float4x4 viewProjection;
    uint drawCount;
};

struct DrawData
{
    uint meshletIndex;
    uint instanceIndex;
};

#endif
