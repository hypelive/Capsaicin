#include "custom_visibility_buffer_shared.h"

struct Pixel
{
    float4 visibility : SV_Target0;
};

Pixel main(in VertexParams params)
{
    Pixel pixel;

    pixel.visibility = float4(0.5f, 0.3f, 0.0f, 1.0f);
    return pixel;
}
