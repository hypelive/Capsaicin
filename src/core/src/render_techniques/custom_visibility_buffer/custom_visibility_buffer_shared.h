#ifndef VISIBILITY_BUFFER_SHARED_H
#define VISIBILITY_BUFFER_SHARED_H

#include "gpu_shared.h"

#ifndef __cplusplus

struct VertexParams
{
    float4 position : SV_Position;
    float3 normal : NORMAL;
};

struct PrimParams
{
    uint instanceID : INSTANCE_ID;
};

#define MESH_GROUP_SIZE 32
struct MeshPayload
{
    uint meshletIndex[MESH_GROUP_SIZE];
    uint instanceIndex[MESH_GROUP_SIZE];
};

#endif

struct DrawConstants
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
