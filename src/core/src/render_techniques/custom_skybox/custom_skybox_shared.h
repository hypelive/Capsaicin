#ifndef CUSTOM_SKYBOX_SHARED_H
#define CUSTOM_SKYBOX_SHARED_H

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
    float3   cameraPosition;
    float    padding;
    float2   invScreenSize;
};

#endif
