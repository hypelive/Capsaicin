#ifndef CUSTOM_TONE_MAPPING_SHARED_H
#define CUSTOM_TONE_MAPPING_SHARED_H

#include "gpu_shared.h"

#define TILE_SIZE 8

struct ToneMapConstants
{
    // .xy - screenSize, .zw - invScreenSize
    float4   screenSize;
};

#endif
