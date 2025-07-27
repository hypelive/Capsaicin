#include "custom_visibility_buffer_shared.h"

struct Pixel
{
    float4 color : SV_Target0;
};

Pixel main(in VertexParams params, in uint instanceID : INSTANCE_ID)
{
    Pixel pixel;

    float4 colors[8] = {
        float4(1.0f, 0.0f, 0.0f, 1.0f), // Red
        float4(0.0f, 1.0f, 0.0f, 1.0f), // Green
        float4(0.0f, 0.0f, 1.0f, 1.0f), // Blue
        float4(1.0f, 1.0f, 0.0f, 1.0f), // Yellow
        float4(1.0f, 0.0f, 1.0f, 1.0f), // Magenta
        float4(0.0f, 1.0f, 1.0f, 1.0f), // Cyan
        float4(0.5f, 0.5f, 0.5f, 1.0f), // Gray
        float4(1.0f, 0.5f, 0.0f, 1.0f)  // Orange
    };

    pixel.color = colors[instanceID % 8];
    return pixel;
}
