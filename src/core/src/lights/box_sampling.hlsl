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

#ifndef BOX_SAMPLING_HLSL
#define BOX_SAMPLING_HLSL

#include "math/color.hlsl"

static const float3 WEST_FACE = float3(-1.0, 0.0, 0.0);
static const float3 EAST_FACE = float3(1.0, 0.0, 0.0);
static const float3 TOP_FACE = float3(0.0, 1.0, 0.0);
static const float3 BOTTOM_FACE = float3(0.0, -1.0, 0.0);
static const float3 SOUTH_FACE = float3(0.0, 0.0, 1.0);
static const float3 NORTH_FACE = float3(0.0, 0.0, -1.0);

static const float3 FACES[] = {
    EAST_FACE,
    WEST_FACE,
    TOP_FACE,
    BOTTOM_FACE,
    SOUTH_FACE,
    NORTH_FACE
};

/**
 * Cast a vector onto a cube map with (-1, 1) range.
 * @param v          The input 3D vector to be projected onto the cube.
 * @param faceIndex  (Out) The index of the face onto which the vector is projected (see `FACES` array).
 * @return The UV coordinates on the cube map.
 */
float2 castOntoCubeUV(
    const float3 v,
    out int faceIndex)
{
    float3 vAbs = abs(v);
    float ma;
    float2 uv;
    if(vAbs.z >= vAbs.x && vAbs.z >= vAbs.y)
    {
        faceIndex = v.z < 0 ? 5 : 4;
        ma = 0.5 / vAbs.z;
        uv = float2(v.z > 0 ? -v.x : v.x, -v.y);
    }
    else if(vAbs.y >= vAbs.x)
    {
        faceIndex = v.y < 0 ? 3 : 2;
        ma = 0.5 / vAbs.y;
        uv = float2(v.x, v.y > 0 ? -v.z : v.z);
    }
    else
    {
        faceIndex = v.x < 0 ? 1 : 0;
        ma = 0.5 / vAbs.x;
        uv = float2(v.x > 0 ? v.z : -v.z, -v.y);
    }
    return uv * ma + 0.5;
}

/**
 * Cast a vector onto a cube map with (-1, 1) range.
 * @param v          The input 3D vector to be projected onto the cube.
 * @param faceIndex  (Out) The index of the face onto which the vector is projected (see `FACES` array).
 * @return A 3D coordinates of the projected vector.
 */
float3 castOntoCube(
    const float3 v,
    out float faceIndex)
{
    float3 vAbs = abs(v);
    if(vAbs.z >= vAbs.x && vAbs.z >= vAbs.y)
    {
        faceIndex = v.z < 0.0 ? 5.0 : 4.0;
        return v / vAbs.z;
    }
    else if(vAbs.y >= vAbs.x)
    {
        faceIndex = v.y < 0.0 ? 3.0 : 2.0;
        return v / vAbs.y;
    }
    else
    {
        faceIndex = v.x < 0.0 ? 1.0 : 0.0;
        return v / vAbs.x;
    }
}

/**
 * Converts UV coordinates to a position on a box face.
 * @param uv          The UV coordinates to convert.
 * @param faceNormal  The normal vector of the box face.
 * @return The position on the box face corresponding to the UV coordinates.
 */
float3 uvToBoxPosition(in float2 uv, in float3 faceNormal)
{
    float2 xy = float2(uv.x, 1.0 - uv.y);
    xy = xy * 2.0 - 1.0;

    float3 up = float3(0.0, 1.0, 0.0);
    bool mainDirectionAlreadyVertical = abs(dot(faceNormal, up)) > 0.999;
    if (mainDirectionAlreadyVertical)
    {
        up = float3(0.0, 0.0, faceNormal.y);
    }
    float3 right = -cross(up, faceNormal);

    return faceNormal + right * xy.x + up * xy.y;
}

/**
 * Sample cube with face normal and UV coordinates.
 * @param cube        The texture cube to sample from.
 * @param sampler     The sampler state to use for sampling.
 * @param uv          The UV coordinates to convert.
 * @param faceNormal  The normal vector of the box face.
 * @param mipLevel    The mip level to sample from.
 * @return RGB sampled color.
 */
float3 sampleBox(in TextureCube cube, in SamplerState sampler, in float2 uv, in float3 faceNormal, in int mipLevel)
{
    float3 pos = uvToBoxPosition(uv, faceNormal);
    return cube.SampleLevel(sampler, pos, mipLevel).rgb;
}

/**
 * Sample cube with face normal and UV coordinates by snapping to the nearst pixel centers.
 * This should be the same as sampleBox, with a nearest neighbor sampler.
 * @param cube        The texture cube to sample from.
 * @param width       The width of the cube map.
 * @param sampler     The sampler state to use for sampling.
 * @param uv          The UV coordinates to convert.
 * @param faceNormal  The normal vector of the box face.
 * @param mipLevel    The mip level to sample from.
 * @return RGB sampled color.
 */
float3 nearestSampleBox(in TextureCube cube, in float width, in SamplerState sampler, in float2 uv, in float3 faceNormal, in int mipLevel)
{
    float2 snappedUV = (ceil(uv * width) - 0.5) / width;
    return sampleBox(cube, sampler, snappedUV, faceNormal, mipLevel);
}

/**
 * Sample 4 nearby pixel luminance with integer indices.
 * @param cube        The texture cube to sample from.
 * @param width       The width of the cube map.
 * @param sampler     The sampler state to use for sampling.
 * @param faceNormal  The normal vector of the box face.
 * @param mipLevel    The mip level to sample from.
 * @param offset      The integer index of the top-left pixel from the 4 pixels to sample.
 * @return 2x2 matrix of luminance values.
 */
float2x2 get2x2Luminance(in TextureCube cube, in float width, in SamplerState sampler, in float3 faceNormal, in int mipLevel, in float2 offset)
{
    // This UV coordinate is directly in between 4 pixels
    float2 uv = ceil(offset + 0.5) / width;
    float dx = 0.5 / width;
    float dy = dx;  // We use squares

    // Reach into the centers of 4 nearby pixels
    return float2x2(
        luminance(sampleBox(cube, sampler, uv + float2(-dx, -dy), faceNormal, mipLevel)),
        luminance(sampleBox(cube, sampler, uv + float2(dx, -dy), faceNormal, mipLevel)),
        luminance(sampleBox(cube, sampler, uv + float2(-dx, dy), faceNormal, mipLevel)),
        luminance(sampleBox(cube, sampler, uv + float2(dx, dy), faceNormal, mipLevel))
    );
}

/**
 * Sample UV coordinates of a box face proportionally to pixel luminance.
 * @param cube        The texture cube to sample from.
 * @param sampler     The sampler state to use for sampling.
 * @param faceNormal  The normal vector of the box face.
 * @param r           Random 2D vector in [0, 1] to sample the luminance distribution.
 * @param topLevel    The lowest MIP level to sample from (to limit the max resolution). Use a value >0 for debugging.
 * @param levels      The number of MIP levels in the cube map.
 * @param pdf         (Out) The probability density function of the sampled UV coordinates.
 * @return The sampled UV coordinates.
 */
float2 importanceSampleByLuminance(in TextureCube cube, in SamplerState sampler, in float3 faceNormal, in float2 r, in int topLevel, in int levels, out float pdf)
{
    // Exclude the level with 1x1 pixels
    int maxLevel = int(levels) - 2;

    int2 offset = int2(0, 0);
    pdf = 1.0;
    float eps = 0.0000001;
    float width = 1.0;

    for (int level = maxLevel; level >= topLevel; --level)
    {
        offset *= 2;
        width *= 2;
        float2x2 view = get2x2Luminance(cube, width, sampler, faceNormal, level, offset);
        view = max(view, eps);
        float2 rowsMass = float2(view._m00 + view._m01, view._m10 + view._m11);
        float totalMass = rowsMass.x + rowsMass.y;

        float py0 = rowsMass.x / totalMass;
        int y = (r.y > py0) ? 1 : 0;

        float px0 = view[y][0] / rowsMass[y];
        int x = (r.x > px0) ? 1 : 0;

        if (y == 0)
        {
            r.y /= py0;
            pdf *= py0;
        }
        else
        {
            float py1 = max(1.0 - py0, eps);
            r.y = (r.y - py0) / py1;
            pdf *= py1;
        }

        if (x == 0)
        {
            r.x /= px0;
            pdf *= px0;
        }
        else
        {
            float px1 = max(1.0 - px0, eps);
            r.x = (r.x - px0) / px1;
            pdf *= px1;
        }

        offset += int2(x, y);
    }

    float2 sample = float2(offset) + r;
    // Scale the sample from [0, <texture-size>] to [0, 1]
    sample = sample / width;
    float invPixelArea = width * width;
    pdf *= invPixelArea;

    return sample;
}

/**
 * Sample direction proportionally to pixel luminance of a cube map.
 * @param cube        The texture cube to sample from.
 * @param sampler     The sampler state to use for sampling.
 * @param r           Random 2D vector in [0, 1] to sample the luminance distribution.
 * @param topLevel    The lowest MIP level to sample from (to limit the max resolution). Use a value >0 for debugging.
 * @param levels      The number of MIP levels in the cube map.
 * @param pdf         (Out) The probability density function of the sampled UV coordinates.
 * @return The sampled direction.
 */
float3 importanceSampleBoxByLuminance(in TextureCube cube, in SamplerState sampler, in float2 r, in int topLevel, in int levels, out float pdf)
{
    float sum = 0.0;
    float faceCDF[6];
    int maxLevel = levels - 1;
    for (int i = 0; i < 6; ++i)
    {
        float value = luminance(cube.SampleLevel(sampler, FACES[i], maxLevel).rgb);
        faceCDF[i] = sum + value;
        sum += value;
    }
    for (int i = 0; i < 6; ++i)
    {
        faceCDF[i] /= sum;
    }

    int faceId = 0;
    while (faceId < 6 && r.y > faceCDF[faceId])
    {
        ++faceId;
    }
    float prevFaceCDF = faceId == 0 ? 0.0 : faceCDF[faceId - 1];
    float facePDF = faceCDF[faceId] - prevFaceCDF;
    r.y = (r.y - prevFaceCDF) / facePDF;

    float3 faceNormal = FACES[faceId];
    float2 uv = importanceSampleByLuminance(cube, sampler, faceNormal, r, topLevel, levels, pdf);
    float3 direction = uvToBoxPosition(uv, faceNormal);
    direction = normalize(direction);
    pdf /= 4;  // Going from 1x1 plane to 2x2 plane (each axis from -1 to 1)
    pdf /= pow(dot(faceNormal, direction), 3);
    pdf *= facePDF;
    return direction;
}

/**
 * Compute PDF of sampling a direction proportionally to pixel luminance of a cube map.
 * @param cube        The texture cube to sample from.
 * @param sampler     The sampler state to use for sampling.
 * @param direction   The sample direction.
 * @param width       The dimension of the cube map. We assume each face is a square.
 * @return PDF value.
 */
float boxLuminancePDF(in TextureCube cube, in SamplerState sampler, in float3 direction, in float width)
{
    int faceIndex;
    float2 uv = castOntoCubeUV(direction, faceIndex);
    float pixelIntensity = luminance(nearestSampleBox(cube, width, sampler, uv, FACES[faceIndex], 0));

    float sum = 0.0;
    const int maxLevel = (1 << 31) - 1;
    for (int i = 0; i < 6; ++i)
    {
        // The largest mip level already contain the integral value over pixels for a unit face
        sum += luminance(cube.SampleLevel(sampler, FACES[i], maxLevel).rgb);
    }

    float pdf = pixelIntensity / sum;
    pdf /= 4;  // Going from 1x1 plane to 2x2 plane (each axis from -1 to 1)
    pdf /= pow(dot(FACES[faceIndex], direction), 3);
    return pdf;
}

#endif // BOX_SAMPLING_HLSL
