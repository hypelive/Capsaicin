#ifndef SSAO_SHARED_H
#define SSAO_SHARED_H

#include "gpu_shared.h"

#define TILE_SIZE 32

struct SSAOConstants
{
    float4x4 viewProjection;
    float4x4 invViewProjection;
    float3   cameraPosition;
    uint     padding;
    // .xy - screenSize, .zw - invScreenSize
    float4   screenSize;
};

#endif
