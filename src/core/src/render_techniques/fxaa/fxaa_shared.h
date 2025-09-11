#ifndef FXAA_SHARED_H
#define FXAA_SHARED_H

#include "gpu_shared.h"

#define TILE_SIZE 8

struct FXAAConstants
{
    float4x4 viewProjection;
    float4x4 invViewProjection;
    float3   cameraPosition;
    float    radius;
    // .xy - screenSize, .zw - invScreenSize
    float4   screenSize;
};

#endif
