#ifndef VSM_SHARED_H
#define VSM_SHARED_H

#include "gpu_shared.h"
#include "shadows/shared.h"

#define TILE_SIZE 8
#define TILE_SIZE_SQR 64

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

struct PageData 
{
    uint2 coordinates;
    uint clipmapIndex;
};

// 8 clipmap 12 y 12 x
uint packPageData(uint2 coordinates, uint clipmapIndex)
{
    return (clipmapIndex << 24) | ((coordinates.y & 0xFFF) << 12) | (coordinates.x & 0xFFF);
}

PageData unpackPageData(uint packed)
{
    PageData result;
    result.coordinates = uint2(packed & 0xFFF, (packed >> 12) & 0xFFF);
    result.clipmapIndex = packed >> 24;

    return result;
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
    // Ring buffers.
    uint pendingVisibleCount;
    uint pendingVisibleHead;

    uint unusedCount;
    uint unusedHead;
    
    uint invalidCount;
    uint invalidHead;
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
