#ifndef CUSTOM_TAA_SHARED_H
#define CUSTOM_TAA_SHARED_H

#include "gpu_shared.h"

#define TILE_SIZE 8

struct TAAConstants
{
    // .xy - screenSize, .zw - invScreenSize
    float4 screenSize;

    float historyWeight;
};

#endif
