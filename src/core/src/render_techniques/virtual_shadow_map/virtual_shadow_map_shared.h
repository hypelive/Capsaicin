#ifndef VSM_SHARED_H
#define VSM_SHARED_H

#include "gpu_shared.h"

#define TILE_SIZE 8

struct VSMConstants
{
    float4x4 viewProjection;
    float4x4 invViewProjection;
    float4x4 lightViewProjection;
    float4   screenSize;
    float3   cameraPosition;
    uint     padding;
};

#endif
