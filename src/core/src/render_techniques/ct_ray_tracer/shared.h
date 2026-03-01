#ifndef CT_RT_SHARED_H
#define CT_RT_SHARED_H

#include "gpu_shared.h"

#ifndef __cplusplus

#    define ShadedVertex Vertex

#endif

// Per draw call data
struct InstanceData
{
    uint instanceId;
    uint vertexOffset;
    uint numVertices;
};

struct Constants
{
    float4x4 view;
    uint     numVertices;
};

#endif
