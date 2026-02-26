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

#ifndef INTERSECTION_HLSL
#define INTERSECTION_HLSL

/*
// Requires the following data to be defined in any shader that uses this file
StructuredBuffer<Instance> g_InstanceBuffer;
StructuredBuffer<float3x4> g_TransformBuffer;
StructuredBuffer<uint> g_IndexBuffer;
StructuredBuffer<Vertex> g_VertexBuffer;
StructuredBuffer<Material> g_MaterialBuffer;
RaytracingAccelerationStructure g_Scene;
uint g_FrameIndex;
*/

#include "geometry/geometry.hlsl"
#include "lights/lights.hlsl"
#include "geometry/mesh.hlsl"
#include "materials/materials.hlsl"
#include "math/hash.hlsl"
#include "ray_tracing.hlsl"

/**
 * Stochastic alpha test the hit surface.
 * @param hitInfo The hit information.
 * @return True if the surface is hit/opaque, false otherwise.
 */
bool AlphaTest(HitInfo hitInfo)
{
    // Get instance information for current object
    Instance instance = g_InstanceBuffer[hitInfo.instanceIndex];

    // Get material
    Material material = g_MaterialBuffer[instance.material_index];

    // Check back facing
    //  We currently only check back facing on alpha flagged surfaces as a performance optimisation. For normal
    //  geometry we should never intersect the back side of any opaque objects due to visibility being occluded
    //  by the front of the object (situations where camera is inside an object is ignored).
    if (!hitInfo.frontFace && asuint(material.normal_alpha_side.z) == 0)
    {
        return false;
    }

    if (asuint(material.normal_alpha_side.w) != 0)
    {
        // Get vertices
        TriangleUV vertices = fetchVerticesUV(instance, hitInfo.primitiveIndex);

        // Calculate UV coordinates
        float2 uv = interpolate(vertices.uv0, vertices.uv1, vertices.uv2, hitInfo.barycentrics);
        MaterialAlpha mask = MakeMaterialAlpha(material, uv);

        // Check the alpha mask
        float alpha = 0.5F;
        if (asuint(material.normal_alpha_side.w) == 2)
        {
            // Approximates alpha blending using hashed alpha
            alpha = saturate(hashToFloat(xxHash(asuint(float4(interpolate(vertices.v0, vertices.v1, vertices.v2, hitInfo.barycentrics), g_FrameIndex)))));
        }
        return mask.alpha > alpha;
    }

    return true;
}

/**
 * Create a ray for closest hit traversal.
 * @param position       Current position on surface.
 * @param geometryNormal Surface normal vector at current position.
 * @param direction      The direction of the new ray.
 * @return The new ray.
 */
RayInfo MakeRayInfoClosest(float3 position, float3 geometryNormal, float3 direction)
{
    RayInfo ray;
    ray.origin = offsetPosition(position, geometryNormal);
    ray.direction = direction;
    ray.range = float2(0.0f, FLT_MAX);
    return ray;
}

/**
 * Create a ray for closest hit traversal.
 * @param position       Current position on surface.
 * @param geometryNormal Surface normal vector at current position.
 * @param direction      The direction of the new ray.
 * @param maxLength      The maximum length of the new ray.
 * @return The new ray.
 */
RayInfo MakeRayInfoClosest(float3 position, float3 geometryNormal, float3 direction, float maxLength)
{
    RayInfo ray;
    ray.origin = offsetPosition(position, geometryNormal);
    ray.direction = direction;
    ray.range = float2(0.0f, maxLength);
    return ray;
}

// A small value to avoid intersection with a light for shadow rays. This value is obtained empirically.
#define SHADOW_RAY_EPSILON (1.0f / (1 << 14))

/**
 * Create a ray for shadow ray traversal.
 * @param position       Current position on surface.
 * @param geometryNormal Surface normal vector at current position.
 * @param lightPosition  The position of the light.
 * @return The new ray.
 */
RayInfo MakeRayInfoShadow(float3 position, float3 geometryNormal, float3 lightPosition)
{
    RayInfo ray;
    ray.origin = offsetPosition(position, geometryNormal);
    ray.direction = lightPosition - ray.origin;
    ray.range = float2(0.0f, 1.0f - SHADOW_RAY_EPSILON);
    return ray;
}

/**
 * Create a ray for shadow ray traversal using light direction.
 * @param position       Current position on surface.
 * @param geometryNormal Surface normal vector at current position.
 * @param lightDirection The direction to the light.
 * @return The new ray.
 */
RayInfo MakeRayInfoShadowDir(float3 position, float3 geometryNormal, float3 lightDirection)
{
    return MakeRayInfoClosest(position, geometryNormal, lightDirection);
}

/**
 * Create a ray for shadow ray traversal.
 * @param position       Current position on surface.
 * @param geometryNormal Surface normal vector at current position.
 * @param lightPosition  The position of the light (will be ignored if selectedLight has no position).
 * @param lightDirection The direction to the light (will be ignored if selectedLight has a position).
 * @param selectedLight  The light the ray is being traced toward.
 * @return The new ray.
 */
RayInfo MakeRayInfoShadow(float3 position, float3 geometryNormal, float3 lightPosition, float3 lightDirection, Light selectedLight)
{
    if (hasLightPosition(selectedLight))
    {
        return MakeRayInfoShadow(position, geometryNormal, lightPosition);
    }
    else
    {
        return MakeRayInfoShadowDir(position, geometryNormal, lightDirection);
    }
}

#ifdef DISABLE_ALPHA_TESTING
#   define CLOSEST_RAY_FLAGS RAY_FLAG_FORCE_OPAQUE
#   define SHADOW_RAY_FLAGS  RAY_FLAG_FORCE_OPAQUE | RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH
typedef RayQuery<CLOSEST_RAY_FLAGS> ClosestRayQuery;
typedef RayQuery<SHADOW_RAY_FLAGS> ShadowRayQuery;

template<typename RayQueryType>
RayQueryType TraceRay(RayDesc ray)
{
    RayQueryType ray_query;
    ray_query.TraceRayInline(g_Scene, RAY_FLAG_NONE, 0xFFu, ray);
    while (ray_query.Proceed())
    {
    }

    return ray_query;
}
#else // DISABLE_ALPHA_TESTING
#   define CLOSEST_RAY_FLAGS RAY_FLAG_NONE
#   define SHADOW_RAY_FLAGS  RAY_FLAG_ACCEPT_FIRST_HIT_AND_END_SEARCH
typedef RayQuery<CLOSEST_RAY_FLAGS> ClosestRayQuery;
typedef RayQuery<SHADOW_RAY_FLAGS> ShadowRayQuery;

template<typename RayQueryType>
RayQueryType TraceRay(RayDesc ray)
{
    RayQueryType ray_query;
    ray_query.TraceRayInline(g_Scene, RAY_FLAG_NONE, 0xFFu, ray);
    while (ray_query.Proceed())
    {
        if (ray_query.CandidateType() == CANDIDATE_NON_OPAQUE_TRIANGLE)
        {
            if (AlphaTest(GetHitInfoRtInlineCandidate(ray_query)))
            {
                ray_query.CommitNonOpaqueTriangleHit();
            }
        }
        else
        {
            // Should never get here as we don't support non-triangle geometry
            // However if this conditional is removed the driver crashes
            ray_query.Abort();
        }
    }

    return ray_query;
}
#endif // DISABLE_ALPHA_TESTING

template<typename RayQueryType>
RayQueryType TraceRay(RayInfo ray)
{
    return TraceRay<RayQueryType>(GetRayDesc(ray));
}

template<typename PayloadType>
void TraceRay(RaytracingAccelerationStructure accelerationStructure, uint rayFlags, uint rayContributionToHitGroupIndex,
              uint missShaderIndex, RayInfo ray, inout PayloadType payload)
{
    TraceRay(accelerationStructure, rayFlags, 0xFFu, rayContributionToHitGroupIndex, 0, missShaderIndex, GetRayDesc(ray), payload);
}

template<typename PayloadType>
void TraceRayClosest(RayInfo ray, inout PayloadType payload)
{
    TraceRay(g_Scene, CLOSEST_RAY_FLAGS, 0xFFu, 0, 0, 0, GetRayDesc(ray), payload);
}

template<typename PayloadType>
void TraceRayClosest(RayInfo ray, uint shaderIndex, inout PayloadType payload)
{
    TraceRay(g_Scene, CLOSEST_RAY_FLAGS, 0xFFu, shaderIndex, 0, shaderIndex, GetRayDesc(ray), payload);
}

template<typename PayloadType>
void TraceRayShadow(RayInfo ray, inout PayloadType payload)
{
    TraceRay(g_Scene, SHADOW_RAY_FLAGS, 0xFFu, 1, 0, 1, GetRayDesc(ray), payload);
}

template<typename PayloadType>
void TraceRayShadow(RayInfo ray, uint shaderIndex, inout PayloadType payload)
{
    TraceRay(g_Scene, SHADOW_RAY_FLAGS, 0xFFu, shaderIndex, 0, shaderIndex, GetRayDesc(ray), payload);
}

#endif // INTERSECTION_HLSL
