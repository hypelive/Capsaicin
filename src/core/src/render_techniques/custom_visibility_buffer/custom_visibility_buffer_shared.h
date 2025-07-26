#ifndef VISIBILITY_BUFFER_SHARED_H
#define VISIBILITY_BUFFER_SHARED_H

#include "gpu_shared.h"

#ifndef __cplusplus

struct VertexParams
{
    float4 position : SV_Position;
};

struct MeshPayload
{
    uint trash;
};

#endif

struct DrawData
{
    uint meshletIndex;
    uint instanceIndex;
};

#endif
