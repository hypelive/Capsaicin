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

#ifndef EOTF_HLSL
#define EOTF_HLSL

#include "math.hlsl"
#include "color.hlsl"

/**
 * Encode a color using sRGB EOTF.
 * @param color Input value to encode.
 * @return The converted value.
 */
float3 encodeEOTFSRGB(float3 color)
{
    return select(color < 0.03929337067685376f, color / 12.92f, pow((color + 0.055010718947587f) / 1.055010718947587f, 2.4f));
}

/**
 * Decode a color using sRGB EOTF.
 * @param color Input value to decode.
 * @return The converted value.
 */
float3 decodeEOTFSRGB(float3 color)
{
    return select(color < 0.003041282560128f, 12.92f * color, 1.055010718947587f * pow(color, 1.0f / 2.4f) - 0.055010718947587f);
}

/**
 * Encode a color using Rec470m (aka gamma 2.2) EOTF.
 * @param color Input value to encode.
 * @return The converted value.
 */
float3 encodeEOTFRec470m(float3 color)
{
    return pow(color, 2.2f);
}

/**
 * Decode a color using Rec470m (aka gamma 2.2) EOTF.
 * @param color Input value to decode.
 * @return The converted value.
 */
float3 decodeEOTFRec470m(float3 color)
{
    return pow(color, 1.0f / 2.2f);
}

/**
 * Encode a color using Rec1886 (aka gamma 2.4) EOTF.
 * @param color Input value to encode.
 * @return The converted value.
 */
float3 encodeEOTFRec1886(float3 color)
{
    return pow(color, 2.4f);
}

/**
 * Decode a color using Rec1886 (aka gamma 2.4) EOTF.
 * @param color Input value to decode.
 * @return The converted value.
 */
float3 decodeEOTFRec1886(float3 color)
{
    return pow(color, 1.0f / 2.4f);
}

/**
 * Encode a color using Rec709 EOTF.
 * @param color Input value to encode.
 * @return The converted value.
 */
float3 encodeEOTFRec709(float3 color)
{
    return select(color < 0.018053968510807f, 4.5f * color, 1.09929682680944f * pow(color, 0.45f) - 0.09929682680944f);
}

/**
 * Decode a color using Rec709 EOTF.
 * @param color Input value to decode.
 * @return The converted value.
 */
float3 decodeEOTFRec709(float3 color)
{
    return select(color < 7.311857246876835f, color / 4.5f, pow((color + 0.09929682680944f) / 1.09929682680944f, 1.0f / 0.45f));
}

/**
 * Encode a color using ST2084 EOTF.
 * @param color Input value to encode.
 * @return The converted value.
 */
float3 encodeEOTFST2048(float3 color)
{
    float3 powM2 = pow(color, 1.0f / 78.84375f);
    return pow(max(powM2 - 0.8359375f, 0) / max(18.8515625f - 18.6875f * powM2, FLT_MIN), 1.0f / 0.1593017578125f) * (10000.0f / 80.0f);
}

/**
 * Decode a color using ST2084 EOTF.
 * @param color Input value to decode.
 * @return The converted value.
 */
float3 decodeEOTFST2048(float3 color)
{
    float3 powM1 = pow(color * (80.0f / 10000.0f), 0.1593017578125f);
    return pow((0.8359375f + 18.8515625f * powM1) / (1.0f + 18.6875f * powM1), 78.84375f);
}

/**
 * Convert an RGB value to sRGB.
 * @note Requires input RGB using BT709 linear RGB values.
 * @param color Input RGB colour to convert.
 * @return The converted colour value.
 */
float3 convertToSRGB(float3 color)
{
    return decodeEOTFSRGB(color);
}

/**
 * Convert an sRGB value to RGB.
 * @note Outputs RGB using BT709 linear RGB values.
 * @param color Input sRGB colour to convert.
 * @return The converted colour value.
 */
float3 convertFromSRGB(float3 color)
{
    return encodeEOTFSRGB(color);
}

/**
 * Convert an RGB value to HDR10.
 * @note Requires input RGB using BT709 linear RGB values.
 * @param color Input RGB colour to convert.
 * @return The converted colour value.
 */
float3 convertToHDR10(float3 color)
{
    //Uses BT2020 color space with ST2048 PQ EOTF
    return decodeEOTFST2048(convertBT709ToBT2020(color));
}

/**
 * Convert an HDR10 value to RGB.
 * @note Outputs RGB using BT709 linear RGB values.
 * @param color Input sRGB colour to convert.
 * @return The converted colour value.
 */
float3 convertFromHDR10(float3 color)
{
    return convertBT2020ToBT709(encodeEOTFST2048(color));
}

#endif // EOTF_HLSL
