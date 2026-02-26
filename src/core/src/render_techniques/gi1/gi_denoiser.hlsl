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

#ifndef GI_DENOISER_HLSL
#define GI_DENOISER_HLSL

#define kGIDenoiser_MaxBlurMask   8.0f

//!
//! GI-1 denoiser shader bindings.
//!

RWTexture2D<float>  g_GIDenoiser_BlurMask;
RWTexture2D<float4> g_GIDenoiser_ColorBuffer;
RWTexture2D<float>  g_GIDenoiser_ColorDeltaBuffer;
Texture2D           g_GIDenoiser_PreviousColorBuffer;
Texture2D           g_GIDenoiser_PreviousColorDeltaBuffer;

//!
//! GI-1 denoiser helper functions.
//!

// Retrieves the radius for the blur kernel.
int GIDenoiser_GetBlurRadius(in uint2 pos)
{
    int   blur_radius = 0;
    float blur_mask = g_GIDenoiser_BlurMask[pos] * kGIDenoiser_MaxBlurMask;

    if (blur_mask > 0.0f)
    {
        blur_radius = int(max(blur_mask, 1.0f) + 0.5f);
    }

    return blur_radius;
}

// Gets the linearized value of the sample depth in the previous frame.
float GIDenoiser_GetPreviousDepth(in float2 uv, in float depth)
{
    float4 clip_space = mul(g_Reprojection, float4(2.0f * float2(uv.x, 1.0f - uv.y) - 1.0f, depth, 1.0f));

    return toLinearDepth(clip_space.z / clip_space.w, g_NearFar);
}

// Removes NaNs from the color values.
float GIDenoiser_RemoveNaNs(in float color)
{
    color /= (1.0f + color);
    color  = saturate(color);
    color /= max(1.0f - color, 1e-4f);

    return color;
}

// Removes NaNs from the color values.
float3 GIDenoiser_RemoveNaNs(in float3 color)
{
    color /= (1.0f + color);
    color  = saturate(color);
    color /= max(1.0f - color, 1e-4f);

    return color;
}

// Removes NaNs from the color values.
float4 GIDenoiser_RemoveNaNs(in float4 color)
{
    color /= (1.0f + color);
    color  = saturate(color);
    color /= max(1.0f - color, 1e-4f);

    return color;
}

#endif // GI_DENOISER_HLSL
