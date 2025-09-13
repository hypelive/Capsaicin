#ifndef FXAA_SHARED_H
#define FXAA_SHARED_H

#include "gpu_shared.h"

#define TILE_SIZE 8

struct FXAAConstants
{
    // .xy - screenSize, .zw - invScreenSize
    float4   screenSize;
};

#endif
