#ifndef CUSTOM_VISIBILITY_BUFFER_SHARED_H
#define CUSTOM_VISIBILITY_BUFFER_SHARED_H

#include "gpu_shared.h"

#ifndef __cplusplus

struct VertexParams
{
    float4 screenPosition : SV_Position;
    float2 uv : TEXCOORD;
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

struct VisibilityBufferConstants
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
