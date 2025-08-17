#ifndef VISIBILITY_BUFFER_SHARED_H
#define VISIBILITY_BUFFER_SHARED_H

#include "gpu_shared.h"

#ifndef __cplusplus

struct VertexParams
{
    float4 screenPosition : SV_Position;
    float3 normal : NORMAL;
    float2 uv : TEXCOORD;
    float3 worldPosition : POSITION0;
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
    float3   cameraPosition; 
    uint drawCount;
};

struct DrawData
{
    uint meshletIndex;
    uint instanceIndex;
};

#endif
