#ifndef CUSTOM_VISIBILITY_TO_GBUFFER_SHARED_H
#define CUSTOM_VISIBILITY_TO_GBUFFER_SHARED_H

#include "gpu_shared.h"

#ifndef __cplusplus

struct VertexParams
{
    float4 screenPosition : SV_Position;
};

#endif

struct VisibilityToGbufferConstants
{
    float4x4 viewProjection;
    float4x4 invViewProjection;
    float3   cameraPosition;
    uint     padding;
    float2   invScreenSize;
};

#endif
