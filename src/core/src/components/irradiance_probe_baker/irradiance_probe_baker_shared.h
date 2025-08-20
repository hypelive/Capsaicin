#ifndef IRRADIANCE_PROBE_BAKER_SHARED_H
#define IRRADIANCE_PROBE_BAKER_SHARED_H

#include "gpu_shared.h"

#ifndef __cplusplus

struct VertexParams
{
    float4 screenPosition : SV_Position;
};

#endif

struct DrawConstants
{
    float4x4 invViewProjection;
    float2   invScreenSize;
    float2   padding;
};

#endif
