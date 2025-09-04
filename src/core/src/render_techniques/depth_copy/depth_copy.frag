#include "depth_copy_shared.h"

Texture2D<float> g_Depth;

struct Pixel
{
    float depth : SV_Target0;
};

Pixel main(in VertexParams params)
{
    Pixel pixel;
    pixel.depth = g_Depth.Load(int3(params.screenPosition.xy, 0));
    return pixel;
}
