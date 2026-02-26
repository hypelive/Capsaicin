/**********************************************************************
Copyright (c) 2025 Advanced Micro Devices, Inc. All rights reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
********************************************************************/

#include "math/math_constants.hlsl"

Texture2D g_InputBuffer;

int2 g_InputResolution;
float2 g_Scale; /**< Downscale amount as g_InputResolution/g_OutputResolution */

#define FILTER_NEAREST 0
#define FILTER_CATMULL_ROM 1
#define FILTER_MITCHELL 2
#define FILTER_LANCZOS2 3
#define FILTER_LANCZOS3 4

#ifndef SCALE_FILTER
#define SCALE_FILTER FILTER_MITCHELL
#endif


float filterBiCubic(float value, float B, float C)
{
    float ret = 0.0f;
    float value2 = value * value;
    float value3 = value * value * value;
    if (value < 1.0f)
        ret = (12.0f - 9.0f * B - 6.0f * C) * value3 + (-18.0f + 12.0f * B + 6.0f * C) * value2 + (6.0f - 2.0f * B);
    else if (value <= 2.0f)
        ret = (-B - 6.0f * C) * value3 + (6.0f * B + 30.0f * C) * value2 + (-12.0f * B - 48.0f * C) * value + (8.0f * B + 24.0f * C);

    return ret / 6.0f;
}

float filterCatmullRom(float value)
{
    // Catmull-Rom is just a bicubic filter with B=0 and C=1/2
    float ret = 0.0f;
    float value2 = value * value;
    float value3 = value * value * value;
    if (value < 1.0f)
        ret = 9.0f * value3 + -15.0f * value2 + 6.0f;
    else if (value <= 2.0f)
        ret = -3.0f * value3 + 15.0f * value2 + -24.0f * value + 12.0f;

    return ret / 6.0f;
}

float filterMitchellNetravali(float value)
{
    // Mitchell-Netravali filter is just a bicubic filter with B=1/3 and C=1/3
    float ret = 0.0f;
    float value2 = value * value;
    float value3 = value * value * value;
    if (value < 1.0f)
        ret = 7.0f * value3 - 12.0f * value2 + 5.33333333333333f;
    else if (value <= 2.0f)
        ret = -2.3333333333333f * value3 + 12.0f * value2 - 20.0f * value + 10.666666666666f;

    return ret / 6.0f;
}

float filterLanczos(float value, float kernelSize)
{
    float ret = 0.0f;
    if (value < FLT_EPSILON)
    {
        ret = 1.0f;
    }
    else if (value <= kernelSize)
    {
        float pix = PI * value;
        ret = (kernelSize * sin(pix) * sin(pix / kernelSize)) / (pix * pix);
    }
    return ret;
}

float4 main(in float4 pos : SV_POSITION, float2 texcoord : TEXCOORD) : SV_Target
{
    // Get input pixel coordinates
    float2 inputPixel = texcoord * (float2) g_InputResolution;

    if (all(g_Scale == 1.0f))
    {
        // Skip filtering if just a 1:1 blit
        return g_InputBuffer.Load(int3(inputPixel, 0));
    }

#if SCALE_FILTER == FILTER_NEAREST
    float3 colour = float3(1.0f, 0.0f, 0.0f);//g_InputBuffer.Load(int3(inputPixel, 0)).xyz;
#else
#   if SCALE_FILTER == FILTER_LANCZOS3
    const float filterSize = 3;
#   else
    const float filterSize = 2;
#   endif
    const float2 radius = g_Scale * filterSize;

    // Get filter box
    float4 box = float4(inputPixel - radius, inputPixel + radius);
    box = float4(floor(box.xy), ceil(box.zw));
    float3 colour = 0.0f;

    // Accumulate samples over filter window
    float totalWeight = 0.0f;
    uint4 boxPixels = int4(box);
    boxPixels = int4(max(boxPixels.xy, 0), min(boxPixels.zw, g_InputResolution));
    for (int x = boxPixels.x; x <= boxPixels.z; ++x)
    {
        for (int y = boxPixels.y; y <= boxPixels.w; ++y)
        {
            int2 samplePixel = int2(x, y);

            int2 sampleOffset = inputPixel - samplePixel;
            float sampleLength = length(float2(sampleOffset) / radius);
#   if SCALE_FILTER == FILTER_CATMULL_ROM
            float weigth = filterCatmullRom(sampleLength);
#   elif SCALE_FILTER == FILTER_MITCHELL
            float weigth = filterMitchellNetravali(sampleLength);
#   elif SCALE_FILTER == FILTER_LANCZOS2 || SCALE_FILTER == FILTER_LANCZOS3
            float weigth = filterLanczos(sampleLength, filterSize);
#   endif
            totalWeight += weigth;

            float3 value = g_InputBuffer.Load(int3(samplePixel, 0)).xyz;
            colour += value * weigth;
        }
    }
    colour /= totalWeight;
#endif
    return float4(colour, 1.0f);
}
