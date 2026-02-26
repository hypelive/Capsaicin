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

#ifndef TRANSFORM_HLSL
#define TRANSFORM_HLSL

#include "math.hlsl"

/**
 * Determine a transformation matrix to correctly transform normal vectors.
 * @param transform The original transform matrix.
 * @return The new transform matrix.
 */
float3x3 getNormalTransform(float3x3 transform)
{
    // The transform for a normal is [1/det(M)]transpose(adj(M))
    // This simplifies down to [1/det(M)]*C where C is the cofactor matrix of M
    float3x3 result = float3x3(
        cross(transform[1].xyz, transform[2].xyz),
        cross(transform[2].xyz, transform[0].xyz),
        cross(transform[0].xyz, transform[1].xyz)
    );
    // Use values already calculated in 'result' to get determinant
    const float3 det3 = transform[0] * result[0];
    const float det = hadd(det3);
    // det(M) is used to correct for inverse scale (mirroring) so it's only needed
    //  to flip normal directions to the correct orientation and so just using the
    //  determinants sign will suffice as this saves a division
    return result * sign(det);
}

/**
 * Transform a normal vector.
 * @note This correctly handles converting the transform to operate correctly on a surface normal.
 * @param normal    The normal vector.
 * @param transform The transform matrix.
 * @return The transformed normal.
 */
float3 transformNormal(const float3 normal, const float3x4 transform)
{
    const float3x3 normalTransform = getNormalTransform((float3x3)transform);
    return mul(normalTransform, normal);
}

/**
 * Transform a 3D direction vector.
 * @param values    The direction vector.
 * @param transform The transform matrix.
 * @return The new transform matrix.
 */
float3 transformVector(const float3 values, const float3x4 transform)
{
    return mul((float3x3)transform, values);
}

/**
 * Transform a 3D point by an affine matrix.
 * @param values    The position.
 * @param transform The transform matrix.
 * @return The new transform matrix.
 */
float3 transformPoint(const float3 values, const float3x4 transform)
{
    return mul(transform, float4(values, 1.0f));
}

/**
 * Transform a 3D point.
 * @note This version of transforming a point assumes a non-affine matrix and will handle
 *  normalisation of the result by the 'w' component.
 * @param values    The position.
 * @param transform The transform matrix.
 * @return The new transform matrix.
 */
float3 transformPointProjection(const float3 values, const float4x4 transform)
{
    float4 ret = mul(transform, float4(values, 1.0f));
    ret.xyz /= ret.w; // perspective divide
    return ret.xyz;
}

/**
 * Transform a 3D point created from a UV pair and depth.
 * @note This version of transforming a point assumes a non-affine matrix and will handle
 *  normalisation of the result by the 'w' component.
 * @param uv        The UV coordinates to build point from.
 * @param depth     The depth value to build point from.
 * @param transform The transform matrix.
 * @return The new transform matrix.
 */
float3 transformPointProjection(float2 uv, float depth, float4x4 transform)
{
    return transformPointProjection(float3(2.0f * float2(uv.x, 1.0f - uv.y) - 1.0f, depth), transform);
}

/**
 * Linearise a perspective depth/z value created using standard depth comparisons.
 * @param depth   The perspective depth value.
 * @param nearFar The near/far values used when creating the perspective depth (i.e. camera near/far).
 * @return The new linearised depth.
 */
float toLinearDepthForward(float depth, float2 nearFar)
{
    return -nearFar.x * nearFar.y / (depth * (nearFar.y - nearFar.x) - nearFar.y);
}

/**
 * Linearise a perspective depth/z value created using reverse depth comparisons.
 * @param depth   The perspective depth value.
 * @param nearFar The near/far values used when creating the perspective depth (i.e. camera near/far).
 * @return The new linearised depth.
 */
float toLinearDepthInverse(float depth, float2 nearFar)
{
    return (nearFar.x * nearFar.y) / (nearFar.x + (depth * (nearFar.y - nearFar.x)));
}

/**
 * Linearise a perspective depth/z value.
 * @note This uses an reverse-Z convention.
 * @param depth   The perspective depth value.
 * @param nearFar The near/far values used when creating the perspective depth (i.e. camera near/far).
 * @return The new linearised depth.
 */
float toLinearDepth(float depth, float2 nearFar)
{
    // Uses reverse-Z convention
    return (nearFar.x * nearFar.y) / (nearFar.x + (depth * (nearFar.y - nearFar.x)));
}
float4 toLinearDepth(float4 depth, float2 nearFar)
{
    // Uses reverse-Z convention
    return (nearFar.x * nearFar.y) / (nearFar.x + (depth * (nearFar.y - nearFar.x)));
}

float3 reconstructWorldPosition(float2 uv, float depth, float4x4 invViewProjection)
{
    float2 ndc = 2.0f * float2(uv.x, 1.0f - uv.y) - 1.0f;

    float4 worldPosition = mul(invViewProjection, float4(ndc, depth, 1.0f));
    worldPosition /= worldPosition.w;

    return worldPosition.xyz;
}

#endif // MATH_HLSL
