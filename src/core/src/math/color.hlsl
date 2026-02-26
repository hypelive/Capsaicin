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

#ifndef COLOR_HLSL
#define COLOR_HLSL

#include "math.hlsl"

/**
 * Calculate the luminance (Y) from an input colour.
 * @note Uses CIE 1931 assuming BT709 linear RGB input values.
 * @param color Input RGB colour to get value from.
 * @return The calculated luminance.
 */
float luminance(float3 color)
{
    return dot(color, float3(0.2126f, 0.7152f, 0.0722f));
}

/**
 * Convert a color from linear BT709 to BT2020 color space.
 * @param color Input value to convert.
 * @return The converted value.
 */
float3 convertBT709ToBT2020(float3 color)
{
    const float3x3 mat = float3x3(0.6274178028f, 0.3292815089f, 0.04330066592f,
        0.06909923255f, 0.919541657f, 0.01135913096f,
        0.01639600657f, 0.08803547174f, 0.89556849f);
    return mul(mat, color);
}

/**
 * Convert a color from linear BT2020 to BT709 color space.
 * @param color Input value to convert.
 * @return The converted value.
 */
float3 convertBT2020ToBT709(float3 color)
{
    const float3x3 mat = float3x3(1.660454154f, -0.5876246095f, -0.0728295669f,
        -0.1245510504f, 1.132898331f, -0.00834732037f,
        -0.01815596037f, -0.1006070971f, 1.118763089f);
    return mul(mat, color);
}

/**
 * Convert an RGB BT709 linear value to XYZ.
 * @param color Input RGB colour to convert.
 * @return The converted colour value.
 */
float3 convertBT709ToXYZ(float3 color)
{
    const float3x3 mat = float3x3(0.4123907983f, 0.3575843275f, 0.1804807931f,
        0.212639004f, 0.7151686549f, 0.07219231874f,
        0.01933081821f, 0.1191947833f, 0.9505321383f);
    return mul(mat, color);
}

/**
 * Convert an RGB BT709 linear value to XYZ with chromatic adaptation.
 * @param color Input RGB colour to convert.
 * @return The converted colour value.
 */
float3 convertBT709ToXYZAdaptation(float3 color)
{
    // Matrix includes conversion from D65 white-point to E using Bradfords
    const float3x3 mat = float3x3(0.4384595752f, 0.3921765387f, 0.1693638712f,
        0.2228332907f, 0.7086937428f, 0.06847295165f,
        0.01731903292f, 0.1104649305f, 0.8722160459f);
    return mul(mat, color);
}

/**
 * Convert an XYZ value to RGB BT709 linear color space.
 * @param color Input XYZ colour to convert.
 * @return The converted colour value.
 */
float3 convertXYZToBT709(float3 color)
{
    const float3x3 mat = float3x3(3.240835667f, -1.537319541f, -0.4985901117f,
        -0.9692294598f, 1.875940084f, 0.04155444726f,
        0.05564493686f, -0.2040314376f, 1.057253838f);
    return mul(mat, color);
}

/**
 * Convert an XYZ value to RGB BT709 linear color space with chromatic adaptation.
 * @param color Input XYZ colour to convert.
 * @return The converted colour value.
 */
float3 convertXYZToBT709Adaptation(float3 color)
{
    // Matrix includes conversion from E white-point to D65 using Bradfords
    const float3x3 mat = float3x3(3.14657712f, -1.666406274f, -0.480170846f,
        -0.9955176115f, 1.955746531f, 0.03977108747f,
        0.06360134482f, -0.2146037817f, 1.151002407f);
    return mul(mat, color);
}

/**
 * Convert an RGB BT709 linear value to DCIP3.
 * @param color Input RGB colour to convert.
 * @return The converted colour value.
 */
float3 convertBT709ToDCIP3(float3 color)
{
    const float3x3 mat = float3x3(0.8989887834f, 0.1940520406f, -1.110223025e-16f,
        0.0318220742f, 0.9268168211f, 1.387778781e-17f,
        0.01965498365f, 0.08329702914f, 1.047305584f);
    return mul(mat, color);
}

/**
 * Convert an RGB BT709 linear value to DCIP3 with chromatic adaptation.
 * @param color Input RGB colour to convert.
 * @return The converted colour value.
 */
float3 convertBT709ToDCIP3Adaptation(float3 color)
{
    // Matrix includes conversion from D65 white-point to DCIP3 using Bradfords
    const float3x3 mat = float3x3(0.8685989976f, 0.1289096773f, 0.002491327235f,
        0.03454159573f, 0.961815834f, 0.003642550204f,
        0.01677691936f, 0.07106060535f, 0.9121624827f);
    return mul(mat, color);
}

/**
 * Convert an DCIP3 value to RGB BT709 linear color space.
 * @param color Input DCIP3 colour to convert.
 * @return The converted colour value.
 */
float3 convertDCIP3ToBT709(float3 color)
{
    const float3x3 mat = float3x3(1.120666623f, -0.2346393019f, 0.0f,
        -0.03847786784f, 1.087018132f, -6.938893904e-18f,
        -0.01797144115f, -0.08205203712f, 0.954831183f);
    return mul(mat, color);
}

/**
 * Convert an DCIP3 value to RGB BT709 linear color space with chromatic adaptation.
 * @param color Input DCIP3 colour to convert.
 * @return The converted colour value.
 */
float3 convertDCIP3ToBT709Adaptation(float3 color)
{
    // Matrix includes conversion from DCIP3 white-point to D65 using Bradfords
    const float3x3 mat = float3x3(1.157490134f, -0.1549475342f, -0.002542620059f,
        -0.04150044546f, 1.045562387f, -0.004061910324f,
        -0.01805607416f, -0.0786030516f, 1.096659064f);
    return mul(mat, color);
}

/**
 * Convert an RGB BT709 linear value to ACEScg.
 * @param color Input RGB colour to convert.
 * @return The converted colour value.
 */
float3 convertBT709ToACEScg(float3 color)
{
    const float3x3 mat = float3x3(0.6031317711f, 0.3263393044f, 0.04798280075f,
        0.07012086362f, 0.919929862f, 0.01276017074f,
        0.0221798066f, 0.116080001f, 0.9407673478f);
    return mul(mat, color);
}

/**
 * Convert an RGB BT709 linear value to ACEScg with chromatic adaptation.
 * @param color Input RGB colour to convert.
 * @return The converted colour value.
 */
float3 convertBT709ToACEScgAdaptation(float3 color)
{
    // Matrix includes conversion from D65 white-point to ACEScg using Bradfords
    const float3x3 mat = float3x3(0.6131113768f, 0.3395187259f, 0.04736991972f,
        0.0701957345f, 0.9163579345f, 0.01344634499f,
        0.0206209477f, 0.1095895767f, 0.8697894812f);
    return mul(mat, color);
}

/**
 * Convert an ACEScg value to RGB BT709 linear color space.
 * @param color Input ACEScg colour to convert.
 * @return The converted colour value.
 */
float3 convertACEScgToBT709(float3 color)
{
    const float3x3 mat = float3x3(1.731182098f, -0.6040180922f, -0.0801043883f,
        -0.1316169947f, 1.134824872f, -0.008679305203f,
        -0.02457481436f, -0.1257839948f, 1.065921545f);
    return mul(mat, color);
}

/**
 * Convert an ACEScg value to RGB BT709 linear color space with chromatic adaptation.
 * @param color Input ACEScg colour to convert.
 * @return The converted colour value.
 */
float3 convertACEScgToBT709Adaptation(float3 color)
{
    // Matrix includes conversion from ACEScg white-point to D65 using Bradfords
    const float3x3 mat = float3x3(1.705011487f, -0.6217663884f, -0.0832451731f,
        -0.1302566081f, 1.140798569f, -0.01054200716f,
        -0.02401062287f, -0.1289946884f, 1.153005362f);
    return mul(mat, color);
}

/**
 * Convert an RGB BT709 linear value to YCoCg.
 * @param color Input RGB colour to convert.
 * @return The converted colour value.
 */
float3 convertBT709ToYCoCg(float3 color)
{
    return float3(color.r * float3(0.25f, 0.5f, -0.25f)
        + color.g * float3(0.5f, 0.0f, 0.5f)
        + color.b * float3(0.25f, -0.5f, -0.25f));
}

/**
 * Convert an YCoCg value to RGB BT709 linear color space.
 * @param color Input YCoCG colour to convert.
 * @return The converted colour value.
 */
float3 convertYCoCgToBT709(float3 color)
{
    return float3(color.r
        + color.g * float3(1.0f, 0.0f, -1.0f)
        + color.b * float3(-1.0f, 1.0f, -1.0f));
}

/**
 * Convert an XYZ value to Lab.
 * @param color Input XYZ colour to convert.
 * @return The converted colour value.
 */
float3 convertXYZToLab(float3 color)
{
    const float3 labWhitePointInverse = float3(1.0f / 0.95047f, 1.0f, 1.0f / 1.08883f);
    float3 lab = color * labWhitePointInverse;
    lab = select(lab > (216.0f / 24389.0f), pow(color, 1.0f / 3.0f), ((24389.0f / 27.0f) * lab + 16.0f) / 116.0f);
    return float3(116.0f * lab.y, 500.0f * (lab.x - lab.y), 200.0f * (lab.y - lab.z));
}

/**
 * Convert an XYZ value to xyY.
 * @param color Input XYZ colour to convert.
 * @return The converted colour value.
 */
float3 convertXYZToXYY(float3 color)
{
    float divisor = max(color.x + color.y + color.z, FLT_EPSILON);
    return float3(color.xy / divisor.x, color.z);
}

/**
 * Convert an xyY value to XYZ.
 * @param color Input xyY colour to convert.
 * @return The converted colour value.
 */
float3 convertXYYToXYZ(float3 color)
{
    float yDiv = max(color.y, FLT_EPSILON);
    return float3((color.x * color.z) / yDiv, color.z, ((1.0f - color.x - color.y) * color.z) / yDiv);
}

/**
 * Convert an xy value to XYZ.
 * @param color Input xy colour to convert.
 * @return The converted colour value.
 */
float3 convertXYToXYZ(float2 color)
{
    float yDiv = max(color.y, FLT_EPSILON);
    return float3(color.x / yDiv, 1.0f, (1.0f - color.x - color.y) / yDiv);
}

#endif // COLOR_HLSL
