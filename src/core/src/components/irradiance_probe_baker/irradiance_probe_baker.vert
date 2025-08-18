#include "irradiance_probe_baker_shared.h"
#include "math/math_constants.hlsl"

VertexParams main(in uint idx : SV_VertexID)
{
    VertexParams params;
    params.screenPosition = float4(1.0f - 4.0f * (idx & 1), 1.0f - 4.0f * (idx >> 1), FLT_MIN, 1.0f);

    return params;
}
