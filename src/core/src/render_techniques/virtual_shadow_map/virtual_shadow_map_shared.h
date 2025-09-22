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
