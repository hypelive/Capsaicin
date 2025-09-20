#ifndef VSM_SHARED_H
#define VSM_SHARED_H

#include "gpu_shared.h"

#define TILE_SIZE 8

static const float CASCADE_RESOLUTION = 8.0f * 1024.0f;
static const float PAGE_RESOLUTION = 128.0f;
static const float PAGE_TABLE_RESOLUTION = CASCADE_RESOLUTION / PAGE_RESOLUTION;
static const float PAGE_UV = PAGE_RESOLUTION / CASCADE_RESOLUTION;
static const float PAGE_NDC = 2.0f * PAGE_UV;
static const float CASCADE_SIZE_0 = 16.0f;

struct VSMConstants
{
    float4x4 viewProjection;
    float4x4 invViewProjection;
    float4x4 lightViewProjection;
    float4   screenSize;
    float3   cameraPosition;
    uint     padding;
    float4   alignedLightPositionNDC;
    float4   cameraPageOffset;
};

#endif
