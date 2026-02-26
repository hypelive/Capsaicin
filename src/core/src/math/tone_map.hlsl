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

#ifndef TONE_MAP_HLSL
#define TONE_MAP_HLSL

#include "color.hlsl"

/**
 * Tonemap an input colour using simple Reinhard.
 * @param color Input colour value to tonemap.
 * @return The tonemapped value.
 */
float3 tonemapSimpleReinhard(float3 color)
{
    return color / (color + 1.0f);
}

/**
 * Inverse Tonemap an input colour using simple Reinhard.
 * @param color Input colour value to inverse tonemap.
 * @return The inverse tonemapped value.
 */
float3 tonemapInverseSimpleReinhard(float3 color)
{
    return color / (1.0f - min(color, 0.99999995f));
}

/**
 * Tonemap an input colour using luminance based Reinhard.
 * @param color Input colour value to tonemap.
 * @return The tonemapped value.
 */
float3 tonemapReinhardLuminance(float3 color)
{
    return color / (1.0f + luminance(color));
}

/**
 * Inverse Tonemap an input colour using luminance based Reinhard.
 * @param color Input colour value to inverse tonemap.
 * @return The inverse tonemapped value.
 */
float3 tonemapInverseReinhardLuminance(float3 color)
{
    return color / (1.0f - luminance(color));
}

/**
 * Tonemap an input colour using extended Reinhard.
 * @param color    Input colour value to tonemap.
 * @param maxWhite Value used to map to 1 in the output (i.e. anything above this is mapped to white).
 * @return The tonemapped value.
 */
float3 tonemapReinhardExtended(float3 color, float maxWhite)
{
    return color * (1.0f + (color / (maxWhite * maxWhite))) / (color + 1.0f);
}

/**
 * Tonemap an input colour using luminance based extended Reinhard.
 * @param color        Input colour value to tonemap.
 * @param maxLuminance Max luminance value of the output range.
 * @return The tonemapped value.
 */
float3 tonemapReinhardExtendedLuminance(float3 color, float maxLuminance)
{
    float lum = luminance(color);
    maxLuminance /= 80.0F; //Scale from nits to scRGB range
    return color * ((lum * (1.0f + (lum / (maxLuminance * maxLuminance))) / (1.0f + lum)) / lum);
}

/**
 * Tonemap an input colour using approximated ACES.
 * @param color Input colour value to tonemap.
 * @return The tonemapped value.
 */
float3 tonemapACESFast(float3 color)
{
    // Based on curve fit by Krzysztof Narkowicz
    color *= 0.6f;
    return (color * (color * 2.51f + 0.03f)) / (color * (color * 2.43f + 0.59f) + 0.14f);
}

/**
 * Inverse Tonemap an input colour using approximated ACES.
 * @param color Input colour value to inverse tonemap.
 * @return The inverse tonemapped value.
 */
float3 tonemapInverseACESFast(float3 color)
{
    return 0.8333333333333333f * (0.59f * color - sqrt((color * -1.0127f + 1.3646f) * color + 0.0009f) - 0.03f) / (2.51f - color * 2.43f);
}

/**
 * Tonemap an input colour using approximated ACES to 0-1000nit HDR range.
 * @param color        Input colour value to tonemap.
 * @param maxLuminance Max luminance value of the output range.
 * @return The tonemapped value.
 */
float3 tonemapACESFast(float3 color, float maxLuminance)
{
    // Based on curve fit by Krzysztof Narkowicz using approximated ACES to 0-1000nit HDR range
    color *= (0.6f / 1000.0f) * maxLuminance;
    color = (color * (color * 15.8f + 2.12f)) / (color * (color * 1.2f + 5.92f) + 1.9f);
    return color;
}

/**
 * Tonemap an input colour using fitted ACES (more precise than approximated version).
 * @param color Input colour value to tonemap.
 * @return The tonemapped value.
 */
float3 tonemapACESFitted(float3 color)
{
    // Fitted by Stephen Hill
    const float3x3 RGBtoACES = float3x3(0.59719f, 0.35458f, 0.04823f,
        0.07600f, 0.90834f, 0.01566f,
        0.02840f, 0.13383f, 0.83777f);
    const float3x3 ACESToRGB = float3x3(1.60475f, -0.53108f, -0.07367f,
        -0.10208f, 1.10813f, -0.00605f,
        -0.00327f, -0.07276f, 1.07602f);
    color = mul(RGBtoACES, color);
    // RRT and ODT curve fitting
    float3 a = color * (color + 0.0245786f) - 0.000090537f;
    float3 b = color * (0.983729f * color + 0.4329510f) + 0.238081f;
    color = a / b;
    return mul(ACESToRGB, color);
}

/**
 * Tonemap an input colour using ACES 1.1 output transform.
 * @param color Input colour value to tonemap.
 * @return The tonemapped value.
 */
float3 tonemapACES(float3 color)
{
    // This is not the full ACES 1.1 output transform as we skip the glow and red hue passes for performance
    // Single matrix containing 709->XYZ->D65ToD60->AP1->RRT_SAT
    const float3x3 RGBToAPI1RRT = float3x3(0.5972001553f, 0.3545784056f, 0.04822144285f,
        0.07600115985f, 0.9083440304f, 0.01565481164f,
        0.02840936743f, 0.133846432f, 0.8377441764f);
    color = mul(RGBToAPI1RRT, color);

    // Apply SSTS curve
    // Uses pre-calculated values with default SDR minY=0.02, maxY=100, midY=10
    const float3x3 M1 = float3x3(0.5f, -1.0f, 0.5f, -1.0f, 1.0f, 0.0f, 0.5f, 0.5f, 0.0f);
    const float maxLuminance = 100.0f;
    const float minLuminance = 0.02f;
    float cLow[5] = {-1.69896996f, -1.69896996f, -0.8658960462f, 0.1757616997f, 1.186720729f};
    float cHigh[5] = {0.05282130092f, 1.30966115f, 1.856749415f, 2.0f, 2.0f};
    const float logMinX = -2.92438364f;
    const float logMidX = -0.9676886797f;
    const float logMaxX = 1.464904547f;
    float3 logx = log10(max(color, FLT_MIN));
    float3 logy;
    for (uint ind = 0; ind < 3; ++ind)
    {
        if (logx[ind] <= logMinX)
        {
            logy[ind] = -1.69896996f;
        }
        else if (logx[ind] < logMidX)
        {
            float knot = 3.0f * (logx[ind] - logMinX) / (logMidX - logMinX);
            uint j = (uint)knot;
            float t = knot - (float)j;
            float3 cf = float3(cLow[j], cLow[j + 1], cLow[j + 2]);
            float3 monomials = float3(t * t, t, 1.0f);
            logy[ind] = dot(monomials, mul(M1, cf));
        }
        else if (logx[ind] < logMaxX)
        {
            float knot = 3.0f * (logx[ind] - logMidX) / (logMaxX - logMidX);
            uint j = (uint)knot;
            float t = knot - (float)j;
            float3 cf = float3(cHigh[j], cHigh[j + 1], cHigh[j + 2]);
            float3 monomials = float3(t * t, t, 1.0f);
            logy[ind] = dot(monomials, mul(M1, cf));
        }
        else
        {
            logy[ind] = 2.0f;
        }
    }
    color = pow(10.0f, logy);

    // Apply linear luminance scale
    color = (color - minLuminance) / (maxLuminance - minLuminance);

    // Convert back to input color space
    // Single matrix containing AP1->ODT_SAT->D60ToD65->XYZ->709
    const float3x3 AP1ToRGBODT = float3x3(1.604716778f, -0.5310570002f, -0.07365974039f,
        -0.1020826399f, 1.108128428f, -0.006045801099f,
        -0.003273871494f, -0.07277934998f, 1.076053262f);
    color = mul(AP1ToRGBODT, color);
    return color;
}

/**
 * Tonemap an input colour using ACES 1.1 output transform.
 * @param color        Input colour value to tonemap.
 * @param maxLuminance Max luminance value of the output range.
 * @return The tonemapped value.
 */
float3 tonemapACES(float3 color, float maxLuminance)
{
    // This is not the full ACES 1.1 output transform as we skip the glow and red hue passes for performance
    // Single matrix containing 709->XYZ->D65ToD60->AP1->RRT_SAT
    const float3x3 RGBToAPI1RRT = float3x3(0.5972001553f, 0.3545784056f, 0.04822144285f,
        0.07600115985f, 0.9083440304f, 0.01565481164f,
        0.02840936743f, 0.133846432f, 0.8377441764f);
    color = mul(RGBToAPI1RRT, color);

    // Calculate SSTS curve values based on display luminance (Note: These can be pre-calculated)
    const float3x3 M1 = float3x3(0.5f, -1.0f, 0.5f, -1.0f, 1.0f, 0.0f, 0.5f, 0.5f, 0.0f);
    const float minLuminance = 0.0001f;
    float cLow[5] = {-4.0f, -4.0f, -3.157376528f, -0.4852499962f, 1.847732425f};
    float cHigh[5];
    const float logMaxLuminance = log10(maxLuminance);
    float acesMax = 0.18f * exp2(lerp(18.0f, 6.5f, (logMaxLuminance - 4.0f) / -2.318758726f));
    const float logAcesMax = log10(acesMax);
    float knotIncHigh = (logAcesMax + 0.7447274923f) / 6.0f;
    cHigh[0] = (1.55f * (-0.7447274923f - knotIncHigh)) + 1.835568905f;
    cHigh[1] = (1.55f * (-0.7447274923f + knotIncHigh)) + 1.835568905f;
    float p = log2(acesMax / 0.18f);
    float s = saturate((p - 6.5f) / (18.0f - 6.5f));
    float pctHigh = 0.89f * (1.0f - s) + 0.90f * s;
    cHigh[3] = logMaxLuminance;
    cHigh[4] = cHigh[3];
    cHigh[2] = 0.6812412143f + pctHigh * (cHigh[3] - 0.6812412143f);

    const float logMid = 1.176091313f;
    uint k = (logMid <= (cHigh[1] + cHigh[2]) / 2.0f) ? 0 : ((logMid <= (cHigh[2] + cHigh[3]) / 2.0f) ? 1 : 2);
    float3 tmp = mul(M1, float3(cHigh[k], cHigh[k + 1], cHigh[k + 2]));
    tmp.z -= logMid;
    const float tk = (2.0f * tmp.z) / (-sqrt(tmp.y * tmp.y - 4.0f * tmp.x * tmp.z) - tmp.y);
    const float knotHigh = (logAcesMax + 0.7447274923f) / 3.0f;
    float expShift = log2(pow(10.0f, -0.7447274923f + (tk + (float)k) * knotHigh)) + 2.473931074f;

    float acesMin = exp2(-17.47393036f - expShift);
    float acesMid = exp2(-2.473931074f - expShift);
    acesMax = exp2(log2(acesMax) - expShift);
    const float logMinX = log10(acesMin);
    const float logMidX = log10(acesMid);
    const float logMaxX = log10(acesMax);

    // Apply SSTS curve
    const float3 logx = log10(max(color, FLT_MIN));
    float3 logy;
    for (uint ind = 0; ind < 3; ++ind)
    {
        if (logx[ind] <= logMinX)
        {
            logy[ind] = -4.0f;
        }
        else if (logx[ind] < logMidX)
        {
            float knot = 3.0f * (logx[ind] - logMinX) / (logMidX - logMinX);
            uint j = (uint)knot;
            float t = knot - (float)j;
            float3 cf = float3(cLow[j], cLow[j + 1], cLow[j + 2]);
            float3 monomials = float3(t * t, t, 1.0f);
            logy[ind] = dot(monomials, mul(M1, cf));
        }
        else if (logx[ind] < logMaxX)
        {
            float knot = 3.0f * (logx[ind] - logMidX) / (logMaxX - logMidX);
            uint j = (uint)knot;
            float t = knot - (float)j;
            float3 cf = float3(cHigh[j], cHigh[j + 1], cHigh[j + 2]);
            float3 monomials = float3(t * t, t, 1.0f);
            logy[ind] = dot(monomials, mul(M1, cf));
        }
        else
        {
            logy[ind] = logMaxLuminance;
        }
    }
    color = pow(10.0f, logy);

    // Apply linear luminance scale
    color = (color - minLuminance) * (maxLuminance / 80.0f) / (maxLuminance - minLuminance);

    // Convert back to input color space
    // Single matrix containing AP1->ODT_SAT->D60ToD65->XYZ->709
    const float3x3 AP1ToRGBODT = float3x3(1.604716778f, -0.5310570002f, -0.07365974039f,
        -0.1020826399f, 1.108128428f, -0.006045801099f,
        -0.003273871494f, -0.07277934998f, 1.076053262f);
    color = mul(AP1ToRGBODT, color);
    return color;
}

/**
 * Tonemap an input colour using Hable Uncharted 2 tonemapper.
 * @param color Input colour value to tonemap.
 * @return The tonemapped value.
 */
float3 tonemapUncharted2(float3 color)
{
    const float A = 0.15f;
    const float B = 0.50f;
    const float C = 0.10f;
    const float D = 0.20f;
    const float E = 0.02f;
    const float F = 0.30f;
    const float white = 11.2f;

    float exposure_bias = 2.0f;
    color *= exposure_bias;
    color = ((color * (A * color + C * B) + D * E) / (color * (A * color + B) + D * F)) - E / F;

    float whiteScale = 1.0f / ((white * (A * white + C * B) + D * E) / (white * (A * white + B) + D * F)) - E / F;
    return color * whiteScale;
}

/**
 * Tonemap an input colour using Khronos PBR neutral.
 * @param color Input colour value to tonemap.
 * @return The tonemapped value.
 */
float3 tonemapPBRNeutral(float3 color)
{
    const float F90 = 0.04F;
    const float ks = 0.8F - F90;
    const float kd = 0.15F;

    float x = hmin(color);
    float offset = x < (2.0F * F90) ? x - (1.0F / (4.0F * F90)) * x * x : 0.04F;
    color -= offset;

    float p = hmax(color);
    if (p <= ks)
    {
        return color;
    }

    float d = 1.0F - ks;
    float pn = 1.0F - d * d / (p + d - ks);

    float g = 1.0F / (kd * (p - pn) + 1.0F);
    return lerp(pn.xxx, color * (pn / p), g);
}

/**
 * Tonemap an input colour using fitted Agx.
 * @param color Input colour value to tonemap.
 * @return The tonemapped value.
 */
float3 tonemapAgxFitted(float3 color)
{
    // Apply input transform
    const float3x3 RGBToAgx = float3x3(0.8566271663f, 0.09512124211f, 0.04825160652f,
        0.1373189688f, 0.7612419724f, 0.1014390364f,
        0.1118982136f, 0.07679941505f, 0.8113023639f);
    color = mul(RGBToAgx, color);

    // Convert to log2 space
    const float minEV = -10.0f;
    const float maxEV = 6.5f;
    color = log2(color);
    color = (color - minEV) / (maxEV - minEV);
    color = saturate(color);

    // Apply sigmoid curve fit
    // Use polynomial fit of original Agx LUT by Benjamin Wrensch
    float3 colorX2 = color * color;
    float3 colorX4 = colorX2 * colorX2;
    color = 15.5f * colorX4 * colorX2 - 40.14f * colorX4 * color + 31.96f * colorX4 - 6.868f
        * colorX2 * color + 0.4298f * colorX2 + 0.1191f * color - 0.00232f;

    // Apply inverse transform
    const float3x3 AgxToRGB = float3x3(1.127100587f, -0.1106066406f, -0.01649393886f,
        -0.1413297653f, 1.157823682f, -0.01649393886f,
        -0.1413297653f, -0.1106066406f, 1.251936436f);
    color = mul(AgxToRGB, color);

    // Linearise color
    color = pow(color, 2.2f);

    return color;
}

/**
 * Tonemap an input colour using Agx.
 * @param color Input colour value to tonemap.
 * @return The tonemapped value.
 */
float3 tonemapAgx(float3 color)
{
    // Apply input transform
    const float3x3 RGBToAgx = float3x3(0.8566271663f, 0.09512124211f, 0.04825160652f,
        0.1373189688f, 0.7612419724f, 0.1014390364f,
        0.1118982136f, 0.07679941505f, 0.8113023639f);
    color = mul(RGBToAgx, color);

    // Convert to log2 space
    const float minEV = -10.0f;
    const float maxEV = 6.5f;
    color = log2(color);
    color = (color - minEV) / (maxEV - minEV);
    color = saturate(color);

    // Apply sigmoid curve fit
    // Uses factored version of original Agx curve
    for (uint ind = 0; ind < 3; ++ind)
    {
        float numerator = 2.0f * (-0.6060606241f + color[ind]);
        if (color[ind] >= 0.6060606241f)
        {
            color[ind] = numerator / pow(1.0f + 69.86278914f * pow(color[ind] - 0.6060606241f, 3.25f), 0.3076923192f);
        }
        else
        {
            color[ind] = numerator / pow(1.0 - 59.507875f * pow(color[ind] - 0.6060606241f, 3.0f), 0.3333333433f);
        }
        color[ind] += 0.5f;
    }

    // Apply inverse transform
    const float3x3 AgxToRGB = float3x3(1.127100587f, -0.1106066406f, -0.01649393886f,
        -0.1413297653f, 1.157823682f, -0.01649393886f,
        -0.1413297653f, -0.1106066406f, 1.251936436f);
    color = mul(AgxToRGB, color);

    // Linearise color
    color = pow(color, 2.2f);

    return color;
}

/**
 * Tonemap an input colour using Agx.
 * @param color        Input colour value to tonemap.
 * @param maxLuminance Max luminance value of the output range.
 * @return The tonemapped value.
 */
float3 tonemapAgx(float3 color, float maxLuminance)
{
    // Apply input transform
    const float3x3 RGBToAgx = float3x3(0.8566271663f, 0.09512124211f, 0.04825160652f,
        0.1373189688f, 0.7612419724f, 0.1014390364f,
        0.1118982136f, 0.07679941505f, 0.8113023639f);
    color = mul(RGBToAgx, color);

    // Convert to log2 space
    const float minEV = -10.0f;
    const float maxEV = log2(maxLuminance);
    color = log2(color);
    color = (color - minEV) / (maxEV - minEV);
    color = saturate(color);

    // Apply sigmoid curve fit
    // Uses factored version of original Agx curve
    for (uint ind = 0; ind < 3; ++ind)
    {
        float numerator = 2.0f * (-0.6060606241f + color[ind]);
        if (color[ind] >= 0.6060606241f)
        {
            color[ind] = numerator / pow(1.0f + 69.86278914f * pow(color[ind] - 0.6060606241f, 3.25f), 0.3076923192f);
        }
        else
        {
            color[ind] = numerator / pow(1.0 - 59.507875f * pow(color[ind] - 0.6060606241f, 3.0f), 0.3333333433f);
        }
        color[ind] += 0.5f;
    }

    // Apply inverse transform
    const float3x3 AgxToRGB = float3x3(1.127100587f, -0.1106066406f, -0.01649393886f,
        -0.1413297653f, 1.157823682f, -0.01649393886f,
        -0.1413297653f, -0.1106066406f, 1.251936436f);
    color = mul(AgxToRGB, color);

    // Linearise color
    color = pow(color, 2.2f);

    // Adjust to scRGB range
    color *= maxLuminance / 80.0f;

    return color;
}

#endif // TONE_MAP_HLSL
