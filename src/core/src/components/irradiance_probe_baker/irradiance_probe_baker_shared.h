#ifndef IRRADIANCE_PROBE_BAKER_SHARED_H
#define IRRADIANCE_PROBE_BAKER_SHARED_H

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
    float    padding; // Padding to align to 16 bytes
    float2   invScreenSize;
};

#endif
