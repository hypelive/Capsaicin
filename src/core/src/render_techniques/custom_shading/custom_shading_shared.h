#ifndef CUSTOM_SHADING_SHARED_H
#define CUSTOM_SHADING_SHARED_H

#include "gpu_shared.h"

#ifndef __cplusplus

struct VertexParams
{
    float4 screenPosition : SV_Position;
};

#endif

struct ShadingConstants
{
    float4x4 viewProjection;
    float3   cameraPosition;
    uint     padding;
    float2   invScreenSize;
};

#endif
