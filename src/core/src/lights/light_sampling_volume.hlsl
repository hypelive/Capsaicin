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

#ifndef LIGHT_SAMPLING_VOLUME_HLSL
#define LIGHT_SAMPLING_VOLUME_HLSL

#include "light_evaluation.hlsl"
#include "geometry/geometry.hlsl"
#include "math/color.hlsl"

/*
// Requires the following data to be defined in any shader that uses this file
TextureCube g_EnvironmentBuffer;
Texture2D g_TextureMaps[] : register(space99);

SamplerState g_TextureSampler;

StructuredBuffer<Light> g_LightBuffer;
StructuredBuffer<uint> g_LightBufferSize;
*/

/*
 * Supports the following config values:
 * LIGHT_SAMPLE_VOLUME_CENTROID = Sample volumes only at single position at centroid of volume
 * THRESHOLD_RADIANCE = A threshold value used to cull area lights, if defined then additional checks
 *   are performed to cull lights based on the size of the sphere of influence defined by the radius at
 *   which the lights contribution drop below the threshold value
 */

/**
 * Calculate the combined luminance(Y) of a light taken within a bounding box.
 * @param selectedLight The light to sample.
 * @param minBB         Bounding box minimum values.
 * @param extent        Bounding box size.
 * @return The calculated combined luminance.
 */
float sampleLightVolume(Light selectedLight, float3 minBB, float3 extent)
{
#if defined(DISABLE_AREA_LIGHTS) && defined(DISABLE_DELTA_LIGHTS) && defined(DISABLE_ENVIRONMENT_LIGHTS)
    return 0.0f;
#else
    float3 radiance;
#   ifdef HAS_MULTIPLE_LIGHT_TYPES
    LightType lightType = selectedLight.get_light_type();
#   endif
#   ifndef DISABLE_AREA_LIGHTS
#       if !defined(DISABLE_DELTA_LIGHTS) || !defined(DISABLE_ENVIRONMENT_LIGHTS)
    if (lightType == kLight_Area)
#       endif
    {
        // Get the area light
        LightArea light = MakeLightArea(selectedLight);

        // Get light position at approximate midpoint
        float3 lightPosition = interpolate(light.v0, light.v1, light.v2, 0.3333333333333f.xx);

        float3 emissivity = light.emissivity.xyz;
#       ifdef THRESHOLD_RADIANCE
        // Quick cull based on range of sphere falloff
        float3 extentCentre = extent * 0.5f;
        float3 centre = minBB + extentCentre;
        float radiusSqr = dot(extentCentre, extentCentre);
        float radius = sqrt(radiusSqr);
        const float range = sqrt(max(emissivity.x, max(emissivity.y, emissivity.z)) / THRESHOLD_RADIANCE);
        float3 lightDirection = centre - lightPosition;
        if (length(lightDirection) > (radius + range))
        {
            return 0.0f;
        }
#       endif // THRESHOLD_RADIANCE

        uint emissivityTex = asuint(light.emissivity.w);
        if (emissivityTex != uint(-1))
        {
            float2 edgeUV0 = light.uv1 - light.uv0;
            float2 edgeUV1 = light.uv2 - light.uv0;
            // Get texture dimensions in order to determine LOD of visible solid angle
            float2 size;
            g_TextureMaps[NonUniformResourceIndex(emissivityTex)].GetDimensions(size.x, size.y);
            float areaUV = size.x * size.y * abs(edgeUV0.x * edgeUV1.y - edgeUV1.x * edgeUV0.y);
            float lod = 0.5f * log2(areaUV);

            float2 uv = interpolate(light.uv0, light.uv1, light.uv2, 0.3333333333333f.xx);
            float4 textureValue = g_TextureMaps[NonUniformResourceIndex(emissivityTex)].SampleLevel(g_TextureSampler, uv, lod);
            emissivity *= textureValue.xyz;
            emissivity *= textureValue.w;
        }

        // Calculate lights surface normal vector
        float3 edge1 = light.v1 - light.v0;
        float3 edge2 = light.v2 - light.v0;
        float3 lightCross = cross(edge1, edge2);
        // Calculate surface area of triangle
        float lightNormalLength = length(lightCross);
        float3 lightNormal = lightCross / lightNormalLength;
        float lightArea = 0.5f * lightNormalLength;

#       ifdef LIGHT_SAMPLE_VOLUME_CENTROID
        // Evaluate radiance at cell centre
#           ifndef THRESHOLD_RADIANCE
        float3 centre = minBB + (extent * 0.5f);
#           endif
        float3 lightVector = centre - lightPosition;
        float lightLengthSqr = lengthSqr(lightVector);
        float recipLengthSqr = (lightLengthSqr != 0.0F) ? rcp(lightLengthSqr) : 0.0F;
        float pdf = saturate(abs(dot(lightNormal, lightVector * rsqrt(lightLengthSqr)))) * recipLengthSqr;
        radiance = emissivity * (lightArea * pdf);
#       else
        // Contribution is emission scaled by surface area converted to solid angle
        // The light is sampled at all 8 corners of the AABB and then interpolated to fill in the internal volume
        float3 maxBB = minBB + extent;
        float3 lightVector = minBB - lightPosition;
        float lightLengthSqr = lengthSqr(lightVector);
        float recipLengthSqr = (lightLengthSqr != 0.0F) ? rcp(lightLengthSqr) : 0.0F;
        float pdf = saturate(abs(dot(lightNormal, lightVector * rsqrt(lightLengthSqr)))) * recipLengthSqr;
        lightVector = float3(minBB.x, minBB.y, maxBB.z) - lightPosition;
        lightLengthSqr = lengthSqr(lightVector);
        recipLengthSqr = (lightLengthSqr != 0.0F) ? rcp(lightLengthSqr) : 0.0F;
        pdf += saturate(abs(dot(lightNormal, lightVector * rsqrt(lightLengthSqr)))) * recipLengthSqr;
        lightVector = float3(minBB.x, maxBB.y, minBB.z) - lightPosition;
        lightLengthSqr = lengthSqr(lightVector);
        recipLengthSqr = (lightLengthSqr != 0.0F) ? rcp(lightLengthSqr) : 0.0F;
        pdf += saturate(abs(dot(lightNormal, lightVector * rsqrt(lightLengthSqr)))) * recipLengthSqr;
        lightVector = float3(minBB.x, maxBB.y, maxBB.z) - lightPosition;
        lightLengthSqr = lengthSqr(lightVector);
        recipLengthSqr = (lightLengthSqr != 0.0F) ? rcp(lightLengthSqr) : 0.0F;
        pdf += saturate(abs(dot(lightNormal, lightVector * rsqrt(lightLengthSqr)))) * recipLengthSqr;
        lightVector = float3(maxBB.x, minBB.y, minBB.z) - lightPosition;
        lightLengthSqr = lengthSqr(lightVector);
        recipLengthSqr = (lightLengthSqr != 0.0F) ? rcp(lightLengthSqr) : 0.0F;
        pdf += saturate(abs(dot(lightNormal, lightVector * rsqrt(lightLengthSqr)))) * recipLengthSqr;
        lightVector = float3(maxBB.x, minBB.y, maxBB.z) - lightPosition;
        lightLengthSqr = lengthSqr(lightVector);
        recipLengthSqr = (lightLengthSqr != 0.0F) ? rcp(lightLengthSqr) : 0.0F;
        pdf += saturate(abs(dot(lightNormal, lightVector * rsqrt(lightLengthSqr)))) * recipLengthSqr;
        lightVector = float3(maxBB.x, maxBB.y, minBB.z) - lightPosition;
        lightLengthSqr = lengthSqr(lightVector);
        recipLengthSqr = (lightLengthSqr != 0.0F) ? rcp(lightLengthSqr) : 0.0F;
        pdf += saturate(abs(dot(lightNormal, lightVector * rsqrt(lightLengthSqr)))) * recipLengthSqr;
        lightVector = maxBB - lightPosition;
        lightLengthSqr = lengthSqr(lightVector);
        recipLengthSqr = (lightLengthSqr != 0.0F) ? rcp(lightLengthSqr) : 0.0F;
        pdf += saturate(abs(dot(lightNormal, lightVector * rsqrt(lightLengthSqr)))) * recipLengthSqr;
        radiance = emissivity * (lightArea * 0.125f * pdf);
#       endif // LIGHT_SAMPLE_VOLUME_CENTROID
    }
#       if !defined(DISABLE_DELTA_LIGHTS) || !defined(DISABLE_ENVIRONMENT_LIGHTS)
    else
#       endif
#   endif // DISABLE_AREA_LIGHTS
#   ifndef DISABLE_DELTA_LIGHTS
    if (lightType == kLight_Point || lightType == kLight_Spot)
    {
        // Get the point light
        LightPoint light = MakeLightPoint(selectedLight);

        // Quick cull based on range of sphere
        float3 extentCentre = extent * 0.5f;
        float3 centre = minBB + extentCentre;
        float radiusSqr = lengthSqr(extentCentre);
        float radius = sqrt(radiusSqr);
        float3 lightDirection = centre - light.position;
        float dirLengthSqr = lengthSqr(lightDirection);
        float combinedRadius = radius + light.range;
        if (dirLengthSqr > (combinedRadius * combinedRadius))
        {
            return 0.0f;
        }

#       ifdef LIGHT_SAMPLE_VOLUME_OVERLAP
        // Get sphere values for overlap test
        float radius2 = light.range;
        float dirDistSqr = dirLengthSqr;
#       endif // LIGHT_SAMPLE_VOLUME_OVERLAP

        if (lightType == kLight_Spot)
        {
            // Check if spot cone intersects current cell
            // Uses fast cone-sphere test (Hale)
            bool intersect = false;
            const float3 coneNegNormal = selectedLight.v2.xyz;
            const float sinAngle = selectedLight.v2.w;
            const float tanAngle = selectedLight.v3.z;
            const float tanAngleSqPlusOne = squared(tanAngle) + 1.0f;
            float offset = radius * sinAngle;
            if (dot(lightDirection + (coneNegNormal * offset), coneNegNormal) < 0.0f)
            {
                float3 c = (lightDirection * sinAngle) - (coneNegNormal * radius);
                float lenA = dot(c, coneNegNormal);
                intersect = (lengthSqr(c) <= squared(lenA) * tanAngleSqPlusOne);
                offset = dot(lightDirection, coneNegNormal);
            }
            else
            {
                intersect = (dirLengthSqr <= radiusSqr);
            }
            if (!intersect)
            {
                return 0.0f;
            }
#       ifdef LIGHT_SAMPLE_VOLUME_OVERLAP
            // Get new sphere values for approximate overlap test
            radius2 = squared(offset) * tanAngleSqPlusOne;
            float3 compareDir = lightDirection + (coneNegNormal * offset);
            dirDistSqr = lengthSqr(compareDir);
#       endif // LIGHT_SAMPLE_VOLUME_OVERLAP
        }

#       ifdef LIGHT_SAMPLE_VOLUME_OVERLAP
        // Check the approximate overlap of the light and the cell
        float overlap = 1.0f;
        float radiusDiff = radius - radius2;
        float radiusDiffSqr = squared(radiusDiff);
        if (dirDistSqr > radiusDiffSqr)
        {
            // Here we treat the cell as a sphere and check the proportion of the cell sphere that overlaps
            //  with the light sphere. In the case of a spot-light we create a sphere centered on the spots cone
            //  direction perpendicular to the shortest path between the cell sphere center and the cones light direction vector.
            float dist = sqrt(dirDistSqr);
            float radiusCombined = radius + radius2;
            overlap = squared(radiusCombined - dist) * (dirDistSqr + (2.0f * dist * radiusCombined) - (3.0f * radiusDiffSqr));
            overlap /= (16.0f * dist * radiusSqr * radius);
        }
        else if (radius > radius2)
        {
            // This is the case where the light is entirely within the sphere. The overlap is then the volume of the
            //  light with respect to the volume of the cell.
            float radius3 = rcp(radiusSqr * radius);
            if (lightType == kLight_Spot)
            {
                // The volume of the cone must be clipped against the bounds of the cell. We approximate this by
                //  capping the light cone roughly where it intersects the cell sphere. We then subtract the volume
                //  of the cone near the apex that is not contained within the cell (again this is roughly clipped
                //  against the cell sphere)
                float3 coneNegNormal = selectedLight.v2.xyz;
                float const tanAngle = selectedLight.v3.z;
                float tc = dot(lightDirection, coneNegNormal);
                float d2 = dirLengthSqr - squared(tc);
                float th = sqrt(radiusSqr - d2);
                overlap = tanAngle * abs(tc) * th * radius3;
            }
            else
            {
                overlap = radius3 * (squared(radius2) * radius2);
            }
        }
#       endif // LIGHT_SAMPLE_VOLUME_OVERLAP

#       ifdef LIGHT_SAMPLE_VOLUME_CENTROID
        // Evaluate radiance at cell centre
        float distSqr = distanceSqr(light.position, centre);
        float rad = saturate(1.0f - (squared(distSqr) / squared(squared(light.range)))) / (0.0001f + distSqr);
        radiance = light.intensity * rad;
#       else // LIGHT_SAMPLE_VOLUME_CENTROID
        // For each corner of the cell evaluate the radiance
        float3 maxBB = minBB + extent;
        float recipRange4 = rcp(squared(squared(light.range)));
        float distSqr = distanceSqr(light.position, minBB);
        float rad = saturate(1.0f - (squared(distSqr) * recipRange4)) / (0.0001f + distSqr);
        distSqr = distanceSqr(light.position, float3(minBB.x, minBB.y, maxBB.z));
        rad += saturate(1.0f - (squared(distSqr) * recipRange4)) / (0.0001f + distSqr);
        distSqr = distanceSqr(light.position, float3(minBB.x, maxBB.y, minBB.z));
        rad += saturate(1.0f - (squared(distSqr) * recipRange4)) / (0.0001f + distSqr);
        distSqr = distanceSqr(light.position, float3(minBB.x, maxBB.y, maxBB.z));
        rad += saturate(1.0f - (squared(distSqr) * recipRange4)) / (0.0001f + distSqr);
        distSqr = distanceSqr(light.position, float3(maxBB.x, minBB.y, minBB.z));
        rad += saturate(1.0f - (squared(distSqr) * recipRange4)) / (0.0001f + distSqr);
        distSqr = distanceSqr(light.position, float3(maxBB.x, minBB.y, maxBB.z));
        rad += saturate(1.0f - (squared(distSqr) * recipRange4)) / (0.0001f + distSqr);
        distSqr = distanceSqr(light.position, float3(maxBB.x, maxBB.y, minBB.z));
        rad += saturate(1.0f - (squared(distSqr) * recipRange4)) / (0.0001f + distSqr);
        distSqr = distanceSqr(light.position, maxBB);
        rad += saturate(1.0f - (squared(distSqr) * recipRange4)) / (0.0001f + distSqr);
        radiance = light.intensity * (rad * 0.125f);
#       endif // LIGHT_SAMPLE_VOLUME_CENTROID
    }
    else
#       ifndef DISABLE_ENVIRONMENT_LIGHTS
    if (lightType == kLight_Direction)
#       endif
    {
        // Get the directional light
        LightDirectional light = MakeLightDirectional(selectedLight);

        // Directional light is constant at all points
        radiance = light.irradiance;
    }
#       ifndef DISABLE_ENVIRONMENT_LIGHTS
    else
#       endif
#   endif // DISABLE_DELTA_LIGHTS
#   ifndef DISABLE_ENVIRONMENT_LIGHTS
    /*lightType == kLight_Environment*/
    {
        // Get the environment light
        LightEnvironment light = MakeLightEnvironment(selectedLight);

        // Environment light is constant at all points so just sample the environment map at
        //   lower mip levels to get combined contribution
        // Due to use of cube map all 6 sides must be individually sampled
        radiance = g_EnvironmentBuffer.SampleLevel(g_TextureSampler, float3(0.0f, 0.0f, 1.0f), light.mips).xyz;
        radiance += g_EnvironmentBuffer.SampleLevel(g_TextureSampler, float3(0.0f, 0.0f, -1.0f), light.mips).xyz;
        radiance += g_EnvironmentBuffer.SampleLevel(g_TextureSampler, float3(0.0f, 1.0f, 0.0f), light.mips).xyz;
        radiance += g_EnvironmentBuffer.SampleLevel(g_TextureSampler, float3(0.0f, -1.0f, 0.0f), light.mips).xyz;
        radiance += g_EnvironmentBuffer.SampleLevel(g_TextureSampler, float3(1.0f, 0.0f, 0.0f), light.mips).xyz;
        radiance += g_EnvironmentBuffer.SampleLevel(g_TextureSampler, float3(-1.0f, 0.0f, 0.0f), light.mips).xyz;
        radiance *= FOUR_PI / 6.0f;
    }
#   endif // DISABLE_ENVIRONMENT_LIGHTS
    return luminance(radiance);
#endif
}


/**
 * Calculate the combined luminance(Y) of a light taken within a bounding box visible from a surface orientation.
 * @param selectedLight The light to sample.
 * @param minBB         Bounding box minimum values.
 * @param extent        Bounding box size.
 * @param normal        The face normal of the bounding box region.
 * @return The calculated combined luminance.
 */
float sampleLightVolumeNormal(Light selectedLight, float3 minBB, float3 extent, float3 normal)
{
#if defined(DISABLE_AREA_LIGHTS) && defined(DISABLE_DELTA_LIGHTS) && defined(DISABLE_ENVIRONMENT_LIGHTS)
    return 0.0f;
#else
    float3 radiance;
#   ifdef HAS_MULTIPLE_LIGHT_TYPES
    LightType lightType = selectedLight.get_light_type();
#   endif
#   ifndef DISABLE_AREA_LIGHTS
#       if !defined(DISABLE_DELTA_LIGHTS) || !defined(DISABLE_ENVIRONMENT_LIGHTS)
    if (lightType == kLight_Area)
#       endif
    {
        // Get the area light
        LightArea light = MakeLightArea(selectedLight);

        // Get light position at approximate midpoint
        float3 lightPosition = interpolate(light.v0, light.v1, light.v2, 0.3333333333333f.xx);

        // Check if inside AABB
        float3 maxBB = minBB + extent;
        float3 extentCentre = extent * 0.5f;
        float3 centre = minBB + extentCentre;
        bool insideAABB = all(lightPosition >= minBB) && all(lightPosition <= maxBB);

        if (!insideAABB)
        {
            // Cull by visibility by checking if triangle is above plane
            if (dot(light.v0 - centre, normal) <= -0.7071f && dot(light.v1 - centre, normal) <= -0.7071f && dot(light.v2 - centre, normal) <= -0.7071f)
            {
                return 0.0f;
            }
        }

        float3 emissivity = light.emissivity.xyz;
#       ifdef THRESHOLD_RADIANCE
        // Quick cull based on range of sphere falloff
        float radiusSqr = dot(extentCentre, extentCentre);
        float radius = sqrt(radiusSqr);
        const float range = sqrt(max(emissivity.x, max(emissivity.y, emissivity.z)) / THRESHOLD_RADIANCE);
        float3 lightDirection = centre - lightPosition;
        if (length(lightDirection) > (radius + range))
        {
            return 0.0f;
        }
#       endif // THRESHOLD_RADIANCE

        uint emissivityTex = asuint(light.emissivity.w);
        if (emissivityTex != uint(-1))
        {
            float2 edgeUV0 = light.uv1 - light.uv0;
            float2 edgeUV1 = light.uv2 - light.uv0;
            // Get texture dimensions in order to determine LOD of visible solid angle
            float2 size;
            g_TextureMaps[NonUniformResourceIndex(emissivityTex)].GetDimensions(size.x, size.y);
            float areaUV = size.x * size.y * abs(edgeUV0.x * edgeUV1.y - edgeUV1.x * edgeUV0.y);
            float lod = 0.5f * log2(areaUV);

            float2 uv = interpolate(light.uv0, light.uv1, light.uv2, 0.3333333333333f.xx);
            emissivity *= g_TextureMaps[NonUniformResourceIndex(emissivityTex)].SampleLevel(g_TextureSampler, uv, lod).xyz;
        }

        // Calculate lights surface normal vector
        float3 edge1 = light.v1 - light.v0;
        float3 edge2 = light.v2 - light.v0;
        float3 lightCross = cross(edge1, edge2);
        // Calculate surface area of triangle
        float lightNormalLength = length(lightCross);
        float3 lightNormal = lightCross / lightNormalLength;
        float lightArea = 0.5f * lightNormalLength;

#       ifdef LIGHT_SAMPLE_VOLUME_CENTROID
        // Evaluate radiance at cell centre
        float3 lightVector = centre - lightPosition;
        float lightLengthSqr = lengthSqr(lightVector);
        float recipLengthSqr = (lightLengthSqr != 0.0F) ? rcp(lightLengthSqr) : 0.0F;
        float pdf = saturate(abs(dot(lightNormal, lightVector * rsqrt(lightLengthSqr)))) * recipLengthSqr;
        radiance = emissivity * (lightArea * pdf);
#       else // LIGHT_SAMPLE_VOLUME_CENTROID
        // Contribution is emission scaled by surface area converted to solid angle
        // The light is sampled at all 8 corners of the AABB and then interpolated to fill in the internal volume
        float3 lightVector = minBB - lightPosition;
        float lightLengthSqr = lengthSqr(lightVector);
        float recipLengthSqr = (lightLengthSqr != 0.0F) ? rcp(lightLengthSqr) : 0.0F;
        lightVector *= rsqrt(lightLengthSqr);
        float pdf = saturate(abs(dot(lightNormal, lightVector))) * recipLengthSqr * (!insideAABB && dot(lightVector, normal) >= 0.7071f ? 0.0f : 1.0f);
        lightVector = float3(minBB.x, minBB.y, maxBB.z) - lightPosition;
        lightLengthSqr = lengthSqr(lightVector);
        recipLengthSqr = (lightLengthSqr != 0.0F) ? rcp(lightLengthSqr) : 0.0F;
        lightVector *= rsqrt(lightLengthSqr);
        pdf += saturate(abs(dot(lightNormal, lightVector))) * recipLengthSqr * (!insideAABB && dot(lightVector, normal) >= 0.7071f ? 0.0f : 1.0f);
        lightVector = float3(minBB.x, maxBB.y, minBB.z) - lightPosition;
        lightLengthSqr = lengthSqr(lightVector);
        recipLengthSqr = (lightLengthSqr != 0.0F) ? rcp(lightLengthSqr) : 0.0F;
        lightVector *= rsqrt(lightLengthSqr);
        pdf += saturate(abs(dot(lightNormal, lightVector))) * recipLengthSqr * (!insideAABB && dot(lightVector, normal) >= 0.7071f ? 0.0f : 1.0f);
        lightVector = float3(minBB.x, maxBB.y, maxBB.z) - lightPosition;
        lightLengthSqr = lengthSqr(lightVector);
        recipLengthSqr = (lightLengthSqr != 0.0F) ? rcp(lightLengthSqr) : 0.0F;
        lightVector *= rsqrt(lightLengthSqr);
        pdf += saturate(abs(dot(lightNormal, lightVector))) * recipLengthSqr * (!insideAABB && dot(lightVector, normal) >= 0.7071f ? 0.0f : 1.0f);
        lightVector = float3(maxBB.x, minBB.y, minBB.z) - lightPosition;
        lightLengthSqr = lengthSqr(lightVector);
        recipLengthSqr = (lightLengthSqr != 0.0F) ? rcp(lightLengthSqr) : 0.0F;
        lightVector *= rsqrt(lightLengthSqr);
        pdf += saturate(abs(dot(lightNormal, lightVector))) * recipLengthSqr * (!insideAABB && dot(lightVector, normal) >= 0.7071f ? 0.0f : 1.0f);
        lightVector = float3(maxBB.x, minBB.y, maxBB.z) - lightPosition;
        lightLengthSqr = lengthSqr(lightVector);
        recipLengthSqr = (lightLengthSqr != 0.0F) ? rcp(lightLengthSqr) : 0.0F;
        lightVector *= rsqrt(lightLengthSqr);
        pdf += saturate(abs(dot(lightNormal, lightVector))) * recipLengthSqr * (!insideAABB && dot(lightVector, normal) >= 0.7071f ? 0.0f : 1.0f);
        lightVector = float3(maxBB.x, maxBB.y, minBB.z) - lightPosition;
        lightLengthSqr = lengthSqr(lightVector);
        recipLengthSqr = (lightLengthSqr != 0.0F) ? rcp(lightLengthSqr) : 0.0F;
        lightVector *= rsqrt(lightLengthSqr);
        pdf += saturate(abs(dot(lightNormal, lightVector))) * recipLengthSqr * (!insideAABB && dot(lightVector, normal) >= 0.7071f ? 0.0f : 1.0f);
        lightVector = maxBB - lightPosition;
        lightLengthSqr = lengthSqr(lightVector);
        recipLengthSqr = (lightLengthSqr != 0.0F) ? rcp(lightLengthSqr) : 0.0F;
        lightVector *= rsqrt(lightLengthSqr);
        pdf += saturate(abs(dot(lightNormal, lightVector))) * recipLengthSqr * (!insideAABB && dot(lightVector, normal) >= 0.7071f ? 0.0f : 1.0f);
        radiance = emissivity * (lightArea * 0.125f * pdf);
#       endif // LIGHT_SAMPLE_VOLUME_CENTROID
    }
#       if !defined(DISABLE_DELTA_LIGHTS) || !defined(DISABLE_ENVIRONMENT_LIGHTS)
    else
#       endif
#   endif // DISABLE_AREA_LIGHTS
#   ifndef DISABLE_DELTA_LIGHTS
    if (lightType == kLight_Point || lightType == kLight_Spot)
    {
        // Get the point light
        LightPoint light = MakeLightPoint(selectedLight);

        // Check if inside AABB
        float3 maxBB = minBB + extent;
        float3 extentCentre = extent * 0.5f;
        float3 centre = minBB + extentCentre;
        const bool insideAABB = all(light.position >= minBB) && all(light.position <= maxBB);

        // Cull by visibility by checking if light is above plane
        if (!insideAABB && dot(light.position - centre, normal) <= -0.7071f)
        {
            return 0.0f;
        }

        // Quick cull based on range of sphere
        float radiusSqr = lengthSqr(extentCentre);
        float radius = sqrt(radiusSqr);
        float3 lightDirection = centre - light.position;
        float dirLengthSqr = lengthSqr(lightDirection);
        float combinedRadius = radius + light.range;
        if (dirLengthSqr > (combinedRadius * combinedRadius))
        {
            return 0.0f;
        }

#        ifdef LIGHT_SAMPLE_VOLUME_OVERLAP
        // Get sphere values for overlap test
        float radius2 = light.range;
        float dirDistSqr = dirLengthSqr;
#       endif // LIGHT_SAMPLE_VOLUME_OVERLAP

        if (lightType == kLight_Spot)
        {
            // Check if spot cone intersects current cell
            // Uses fast cone-sphere test (Hale)
            bool intersect = false;
            const float3 coneNegNormal = selectedLight.v2.xyz;
            const float sinAngle = selectedLight.v2.w;
            const float tanAngle = selectedLight.v3.z;
            const float tanAngleSqPlusOne = squared(tanAngle) + 1.0f;
            float offset = radius * sinAngle;
            if (dot(lightDirection + (coneNegNormal * offset), coneNegNormal) < 0.0f)
            {
                float3 c = (lightDirection * sinAngle) - (coneNegNormal * radius);
                float lenA = dot(c, coneNegNormal);
                intersect = (lengthSqr(c) <= squared(lenA) * tanAngleSqPlusOne);
                offset = dot(lightDirection, coneNegNormal);
            }
            else
            {
                intersect = (dirLengthSqr <= radiusSqr);
            }
            if (!intersect)
            {
                return 0.0f;
            }
#       ifdef LIGHT_SAMPLE_VOLUME_OVERLAP
            // Get new sphere values for approximate overlap test
            radius2 = squared(offset) * tanAngleSqPlusOne;
            float3 compareDir = lightDirection + (coneNegNormal * offset);
            dirDistSqr = lengthSqr(compareDir);
#       endif // LIGHT_SAMPLE_VOLUME_OVERLAP
        }

#       ifdef LIGHT_SAMPLE_VOLUME_OVERLAP
        // Check the approximate overlap of the light and the cell
        float overlap = 1.0f;
        float radiusDiff = radius - radius2;
        float radiusDiffSqr = squared(radiusDiff);
        if (dirDistSqr > radiusDiffSqr)
        {
            // Here we treat the cell as a sphere and check the proportion of the cell sphere that overlaps
            //  with the light sphere. In the case of a spot-light we create a sphere centered on the spots cone
            //  direction perpendicular to the shortest path between the cell sphere center and the cones light direction vector.
            float dist = sqrt(dirDistSqr);
            float radiusCombined = radius + radius2;
            overlap = squared(radiusCombined - dist) * (dirDistSqr + (2.0f * dist * radiusCombined) - (3.0f * radiusDiffSqr));
            overlap /= (16.0f * dist * radiusSqr * radius);
        }
        else if (radius > radius2)
        {
            // This is the case where the light is entirely within the sphere. The overlap is then the volume of the
            //  light with respect to the volume of the cell.
            float radius3 = rcp(radiusSqr * radius);
            if (lightType == kLight_Spot)
            {
                // The volume of the cone must be clipped against the bounds of the cell. We approximate this by
                //  capping the light cone roughly where it intersects the cell sphere. We then subtract the volume
                //  of the cone near the apex that is not contained within the cell (again this is roughly clipped
                //  against the cell sphere)
                float3 coneNegNormal = selectedLight.v2.xyz;
                float const tanAngle = selectedLight.v3.z;
                float tc = dot(lightDirection, coneNegNormal);
                float d2 = dirLengthSqr - squared(tc);
                float th = sqrt(radiusSqr - d2);
                overlap = tanAngle * abs(tc) * th * radius3;
            }
            else
            {
                overlap = radius3 * (squared(radius2) * radius2);
            }
        }
#       endif // LIGHT_SAMPLE_VOLUME_OVERLAP

#       ifdef LIGHT_SAMPLE_VOLUME_CENTROID
        // Evaluate radiance at cell centre
        float distSqr = distanceSqr(light.position, centre);
        float rad = saturate(1.0f - (squared(distSqr) / squared(squared(light.range)))) / (0.0001f + distSqr);
        radiance = light.intensity * rad;
#       else // LIGHT_SAMPLE_VOLUME_CENTROID
        // For each corner of the cell evaluate the radiance
        float recipRange4 = rcp(squared(squared(light.range)));
        float3 lightVector = minBB - light.position;
        float distSqr = lengthSqr(lightVector);
        lightVector *= rsqrt(distSqr);
        float pdf = saturate(1.0f - (squared(distSqr) * recipRange4)) / (0.0001f + distSqr) * (!insideAABB && dot(lightVector, normal) >= 0.7071f ? 0.0f : 1.0f);
        lightVector = float3(minBB.x, minBB.y, maxBB.z) - light.position;
        distSqr = lengthSqr(lightVector);
        lightVector *= rsqrt(distSqr);
        pdf += saturate(1.0f - (squared(distSqr) * recipRange4)) / (0.0001f + distSqr) * (!insideAABB && dot(lightVector, normal) >= 0.7071f ? 0.0f : 1.0f);
        lightVector = float3(minBB.x, maxBB.y, minBB.z) - light.position;
        distSqr = lengthSqr(lightVector);
        lightVector *= rsqrt(distSqr);
        pdf += saturate(1.0f - (squared(distSqr) * recipRange4)) / (0.0001f + distSqr) * (!insideAABB && dot(lightVector, normal) >= 0.7071f ? 0.0f : 1.0f);
        lightVector = float3(minBB.x, maxBB.y, maxBB.z) - light.position;
        distSqr = lengthSqr(lightVector);
        lightVector *= rsqrt(distSqr);
        pdf += saturate(1.0f - (squared(distSqr) * recipRange4)) / (0.0001f + distSqr) * (!insideAABB && dot(lightVector, normal) >= 0.7071f ? 0.0f : 1.0f);
        lightVector = float3(maxBB.x, minBB.y, minBB.z) - light.position;
        distSqr = lengthSqr(lightVector);
        lightVector *= rsqrt(distSqr);
        pdf += saturate(1.0f - (squared(distSqr) * recipRange4)) / (0.0001f + distSqr) * (!insideAABB && dot(lightVector, normal) >= 0.7071f ? 0.0f : 1.0f);
        lightVector = float3(maxBB.x, minBB.y, maxBB.z) - light.position;
        distSqr = lengthSqr(lightVector);
        lightVector *= rsqrt(distSqr);
        pdf += saturate(1.0f - (squared(distSqr) * recipRange4)) / (0.0001f + distSqr) * (!insideAABB && dot(lightVector, normal) >= 0.7071f ? 0.0f : 1.0f);
        lightVector = float3(maxBB.x, maxBB.y, minBB.z) - light.position;
        distSqr = lengthSqr(lightVector);
        lightVector *= rsqrt(distSqr);
        pdf += saturate(1.0f - (squared(distSqr) * recipRange4)) / (0.0001f + distSqr) * (!insideAABB && dot(lightVector, normal) >= 0.7071f ? 0.0f : 1.0f);
        lightVector = maxBB - light.position;
        distSqr = lengthSqr(lightVector);
        lightVector *= rsqrt(distSqr);
        pdf += saturate(1.0f - (squared(distSqr) * recipRange4)) / (0.0001f + distSqr) * (!insideAABB && dot(lightVector, normal) >= 0.7071f ? 0.0f : 1.0f);
        radiance = light.intensity * (0.125f * pdf);
#       endif // LIGHT_SAMPLE_VOLUME_CENTROID
    }
    else
#       ifndef DISABLE_ENVIRONMENT_LIGHTS
    if (lightType == kLight_Direction)
#       endif
    {
        // Get the directional light
        LightDirectional light = MakeLightDirectional(selectedLight);

        // Fast check to cull lights based on cell normal
        if (dot(light.direction, normal) <= -0.7071f)
        {
            return 0.0f;
        }

        // Directional light is constant at all points
        radiance = light.irradiance;
    }
#       ifndef DISABLE_ENVIRONMENT_LIGHTS
    else
#       endif
#   endif // DISABLE_DELTA_LIGHTS
#   ifndef DISABLE_ENVIRONMENT_LIGHTS
    /*lightType == kLight_Environment*/
    {
        // Get the environment light
        LightEnvironment light = MakeLightEnvironment(selectedLight);

        // Environment light is constant at all points so just sample the environment map at
        //   lower mip levels to get combined contribution
        // Due to normal based sampling the directions straddle multiple cube faces
        radiance = 0.0f;
        float count = 0.0f;
        if (normal.z != 0)
        {
            radiance += g_EnvironmentBuffer.SampleLevel(g_TextureSampler, float3(0.0f, 0.0f, normal.z), light.mips).xyz;
            ++count;
        }
        if (normal.y != 0)
        {
            radiance += g_EnvironmentBuffer.SampleLevel(g_TextureSampler, float3(0.0f, normal.y, 0.0f), light.mips).xyz;
            ++count;
        }
        if (normal.x != 0)
        {
            radiance += g_EnvironmentBuffer.SampleLevel(g_TextureSampler, float3(normal.x, 0.0f, 0.0f), light.mips).xyz;
            ++count;
        }
        radiance *= FOUR_PI / count;
    }
#   endif // DISABLE_ENVIRONMENT_LIGHTS
    return luminance(radiance);
#endif
}

/**
 * Calculate the combined luminance(Y) of a light taken at a specific location.
 * @param selectedLight The light to sample.
 * @param position      Current position on surface.
 * @return The calculated combined luminance.
 */
float sampleLightPoint(Light selectedLight, float3 position)
{
#if defined(DISABLE_AREA_LIGHTS) && defined(DISABLE_DELTA_LIGHTS) && defined(DISABLE_ENVIRONMENT_LIGHTS)
    return 0.0f;
#else
    float3 radiance;
#   ifdef HAS_MULTIPLE_LIGHT_TYPES
    LightType lightType = selectedLight.get_light_type();
#   endif
#   ifndef DISABLE_AREA_LIGHTS
#       if !defined(DISABLE_DELTA_LIGHTS) || !defined(DISABLE_ENVIRONMENT_LIGHTS)
    if (lightType == kLight_Area)
#       endif
    {
        // Get the area light
        LightArea light = MakeLightArea(selectedLight);

        // Get light position at approximate midpoint
        float3 lightPosition = interpolate(light.v0, light.v1, light.v2, 0.3333333333333f.xx);

        float3 emissivity = light.emissivity.xyz;
        uint emissivityTex = asuint(light.emissivity.w);
        if (emissivityTex != uint(-1))
        {
            float2 edgeUV0 = light.uv1 - light.uv0;
            float2 edgeUV1 = light.uv2 - light.uv0;
            // Get texture dimensions in order to determine LOD of visible solid angle
            float2 size;
            g_TextureMaps[NonUniformResourceIndex(emissivityTex)].GetDimensions(size.x, size.y);
            float areaUV = size.x * size.y * abs(edgeUV0.x * edgeUV1.y - edgeUV1.x * edgeUV0.y);
            float lod = 0.5f * log2(areaUV);

            float2 uv = interpolate(light.uv0, light.uv1, light.uv2, 0.3333333333333f.xx);
            emissivity *= g_TextureMaps[NonUniformResourceIndex(emissivityTex)].SampleLevel(g_TextureSampler, uv, lod).xyz;
        }

        // Calculate lights surface normal vector
        float3 edge1 = light.v1 - light.v0;
        float3 edge2 = light.v2 - light.v0;
        float3 lightCross = cross(edge1, edge2);
        // Calculate surface area of triangle
        float lightNormalLength = length(lightCross);
        float3 lightNormal = lightCross / lightNormalLength;
        float lightArea = 0.5f * lightNormalLength;

        // Evaluate radiance at specified point
        float3 lightVector = position - lightPosition;
        float lightLengthSqr = lengthSqr(lightVector);
        lightVector *= rsqrt(lightLengthSqr);
        float pdf = saturate(abs(dot(lightNormal, lightVector)));
        pdf = (lightLengthSqr != 0.0F) ? pdf / lightLengthSqr : 0.0f;
        radiance = emissivity * (lightArea * pdf);
    }
#       if !defined(DISABLE_DELTA_LIGHTS) || !defined(DISABLE_ENVIRONMENT_LIGHTS)
    else
#       endif
#   endif // DISABLE_AREA_LIGHTS
#   ifndef DISABLE_DELTA_LIGHTS
    if (lightType == kLight_Point || lightType == kLight_Spot)
    {
        // Get the point light
        LightPoint light = MakeLightPoint(selectedLight);

        // Evaluate radiance at specified point
        float distSqr = distanceSqr(light.position, position);
        float rad = saturate(1.0f - (squared(distSqr) / squared(squared(light.range)))) / (0.0001f + distSqr);
        radiance = light.intensity * rad;
    }
    else
#       ifndef DISABLE_ENVIRONMENT_LIGHTS
    if (lightType == kLight_Direction)
#       endif
    {
        // Get the directional light
        LightDirectional light = MakeLightDirectional(selectedLight);

        // Directional light is constant at all points
        radiance = light.irradiance;
    }
#       ifndef DISABLE_ENVIRONMENT_LIGHTS
    else
#       endif
#   endif // DISABLE_DELTA_LIGHTS
#   ifndef DISABLE_ENVIRONMENT_LIGHTS
    /*lightType == kLight_Environment*/
    {
        // Get the environment light
        LightEnvironment light = MakeLightEnvironment(selectedLight);

        // Environment light is constant at all points so just sample the environment map at
        //   lower mip levels to get combined contribution
        // Due to use of cube map all 6 sides must be individually sampled
        radiance = g_EnvironmentBuffer.SampleLevel(g_TextureSampler, float3(0.0f, 0.0f, 1.0f), light.mips).xyz;
        radiance += g_EnvironmentBuffer.SampleLevel(g_TextureSampler, float3(0.0f, 0.0f, -1.0f), light.mips).xyz;
        radiance += g_EnvironmentBuffer.SampleLevel(g_TextureSampler, float3(0.0f, 1.0f, 0.0f), light.mips).xyz;
        radiance += g_EnvironmentBuffer.SampleLevel(g_TextureSampler, float3(0.0f, -1.0f, 0.0f), light.mips).xyz;
        radiance += g_EnvironmentBuffer.SampleLevel(g_TextureSampler, float3(1.0f, 0.0f, 0.0f), light.mips).xyz;
        radiance += g_EnvironmentBuffer.SampleLevel(g_TextureSampler, float3(-1.0f, 0.0f, 0.0f), light.mips).xyz;
        radiance *= FOUR_PI / 6.0f;
    }
#   endif // DISABLE_ENVIRONMENT_LIGHTS
    return luminance(radiance);
#endif
}

/**
 * Calculate the combined luminance(Y) of a light taken at a specific location visible from a surface orientation.
 * @param selectedLight The light to sample.
 * @param position      Current position on surface.
 * @param normal        Shading normal vector at current position.
 * @return The calculated combined luminance.
 */
float sampleLightPointNormal(Light selectedLight, float3 position, float3 normal)
{
#if defined(DISABLE_AREA_LIGHTS) && defined(DISABLE_DELTA_LIGHTS) && defined(DISABLE_ENVIRONMENT_LIGHTS)
    return 0.0f;
#else
    float3 radiance;
#   ifdef HAS_MULTIPLE_LIGHT_TYPES
    LightType lightType = selectedLight.get_light_type();
#   endif
#   ifndef DISABLE_AREA_LIGHTS
#       if !defined(DISABLE_DELTA_LIGHTS) || !defined(DISABLE_ENVIRONMENT_LIGHTS)
    if (lightType == kLight_Area)
#       endif
    {
        // Get the area light
        LightArea light = MakeLightArea(selectedLight);

        // Get light position at approximate midpoint
        float3 lightPosition = interpolate(light.v0, light.v1, light.v2, 0.3333333333333f.xx);

        float3 emissivity = light.emissivity.xyz;
        uint emissivityTex = asuint(light.emissivity.w);
        if (emissivityTex != uint(-1))
        {
            float2 edgeUV0 = light.uv1 - light.uv0;
            float2 edgeUV1 = light.uv2 - light.uv0;
            // Get texture dimensions in order to determine LOD of visible solid angle
            float2 size;
            g_TextureMaps[NonUniformResourceIndex(emissivityTex)].GetDimensions(size.x, size.y);
            float areaUV = size.x * size.y * abs(edgeUV0.x * edgeUV1.y - edgeUV1.x * edgeUV0.y);
            float lod = 0.5f * log2(areaUV);

            float2 uv = interpolate(light.uv0, light.uv1, light.uv2, 0.3333333333333f.xx);
            emissivity *= g_TextureMaps[NonUniformResourceIndex(emissivityTex)].SampleLevel(g_TextureSampler, uv, lod).xyz;
        }

        // Calculate lights surface normal vector
        float3 edge1 = light.v1 - light.v0;
        float3 edge2 = light.v2 - light.v0;
        float3 lightCross = cross(edge1, edge2);
        // Calculate surface area of triangle
        float lightNormalLength = length(lightCross);
        float3 lightNormal = lightCross / lightNormalLength;
        float lightArea = 0.5f * lightNormalLength;

        // Evaluate radiance at specified point
        float3 lightVector = position - lightPosition;
        float lightLengthSqr = lengthSqr(lightVector);
        lightVector *= rsqrt(lightLengthSqr);
        float pdf = saturate(abs(dot(lightNormal, lightVector))) * lightArea;
        pdf = (lightLengthSqr != 0.0F) ? pdf / lightLengthSqr : 0.0f;
        radiance = emissivity * pdf;

        // Evaluate the angle to the light, as the light may be partially behind the current surface
        //   we need to check multiple points to ensure we don't incorrectly cull the light.
        float3 lightVector0 = normalize(light.v0 - position);
        float3 lightVector1 = normalize(light.v1 - position);
        float3 lightVector2 = normalize(light.v2 - position);

        // Evaluate light angle at specified points
        float angles = saturate(dot(lightVector0, normal)) + saturate(dot(lightVector1, normal)) + saturate(dot(lightVector2, normal));
        radiance *= angles / 3.0f;
    }
#       if !defined(DISABLE_DELTA_LIGHTS) || !defined(DISABLE_ENVIRONMENT_LIGHTS)
    else
#       endif
#   endif // DISABLE_AREA_LIGHTS
#   ifndef DISABLE_DELTA_LIGHTS
    if (lightType == kLight_Point || lightType == kLight_Spot)
    {
        // Get the point light
        LightPoint light = MakeLightPoint(selectedLight);

        // Evaluate radiance at specified point
        float3 lightVector = light.position - position;
        float distSqr = lengthSqr(lightVector);
        float rad = saturate(1.0f - (squared(distSqr) / squared(squared(light.range)))) / (0.0001f + distSqr);
        lightVector *= rsqrt(distSqr);
        radiance = light.intensity * rad * saturate(dot(lightVector, normal));
    }
    else
#       ifndef DISABLE_ENVIRONMENT_LIGHTS
    if (lightType == kLight_Direction)
#       endif
    {
        // Get the directional light
        LightDirectional light = MakeLightDirectional(selectedLight);

        // Directional light is constant at all points
        radiance = light.irradiance * saturate(dot(light.direction, normal));
    }
#       ifndef DISABLE_ENVIRONMENT_LIGHTS
    else
#       endif
#   endif // DISABLE_DELTA_LIGHTS
#   ifndef DISABLE_ENVIRONMENT_LIGHTS
    /*lightType == kLight_Environment*/
    {
        // Get the environment light
        LightEnvironment light = MakeLightEnvironment(selectedLight);

        // Environment light is constant at all points so just sample the environment map at
        //   lower mip levels to get combined contribution
        // Due to normal based sampling the directions straddle multiple cube faces
        radiance = 0.0f;
        float count = 0.0f;
        if (normal.z != 0)
        {
            radiance += g_EnvironmentBuffer.SampleLevel(g_TextureSampler, float3(0.0f, 0.0f, normal.z), light.mips).xyz;
            ++count;
        }
        if (normal.y != 0)
        {
            radiance += g_EnvironmentBuffer.SampleLevel(g_TextureSampler, float3(0.0f, normal.y, 0.0f), light.mips).xyz;
            ++count;
        }
        if (normal.x != 0)
        {
            radiance += g_EnvironmentBuffer.SampleLevel(g_TextureSampler, float3(normal.x, 0.0f, 0.0f), light.mips).xyz;
            ++count;
        }
        radiance /= count;
    }
#   endif // DISABLE_ENVIRONMENT_LIGHTS
    return luminance(radiance);
#endif
}


/**
 * Calculate a quick weighting based on the cosine light angle.
 * @param selectedLight The light to sample.
 * @param position      Current position on surface.
 * @param normal        Shading normal vector at current position.
 * @return The calculated angle weight.
 */
float sampleLightPointNormalFast(Light selectedLight, float3 position, float3 normal)
{
#if defined(DISABLE_AREA_LIGHTS) && defined(DISABLE_DELTA_LIGHTS) && defined(DISABLE_ENVIRONMENT_LIGHTS)
    return 0.0f;
#else
#   ifdef HAS_MULTIPLE_LIGHT_TYPES
    LightType lightType = selectedLight.get_light_type();
#   endif
#   ifndef DISABLE_AREA_LIGHTS
#       if !defined(DISABLE_DELTA_LIGHTS) || !defined(DISABLE_ENVIRONMENT_LIGHTS)
    if (lightType == kLight_Area)
#       endif
    {
        // Get the area light
        LightArea light = MakeLightArea(selectedLight);

        // Evaluate the angle to the light, as the light may be partially behind the current surface
        //   we need to check multiple points to ensure we don't incorrectly cull the light.
        float3 lightVector0 = normalize(light.v0 - position);
        float3 lightVector1 = normalize(light.v1 - position);
        float3 lightVector2 = normalize(light.v2 - position);

        // Evaluate light angle at specified points
        float angles = saturate(dot(lightVector0, normal)) + saturate(dot(lightVector1, normal)) + saturate(dot(lightVector2, normal));
        return angles / 3.0f;
    }
#       if !defined(DISABLE_DELTA_LIGHTS) || !defined(DISABLE_ENVIRONMENT_LIGHTS)
    else
#       endif
#   endif // DISABLE_AREA_LIGHTS
#   ifndef DISABLE_DELTA_LIGHTS
    if (lightType == kLight_Point || lightType == kLight_Spot)
    {
        // Get the point light
        LightPoint light = MakeLightPoint(selectedLight);

        // Evaluate radiance at specified point
        float3 lightVector = light.position - position;
        return saturate(dot(lightVector, normal));
    }
    else
#       ifndef DISABLE_ENVIRONMENT_LIGHTS
    if (lightType == kLight_Direction)
#       endif
    {
        // Get the directional light
        LightDirectional light = MakeLightDirectional(selectedLight);

        // Directional light is constant at all points
        return saturate(dot(light.direction, normal));
    }
#       ifndef DISABLE_ENVIRONMENT_LIGHTS
    else
#       endif
#   endif // DISABLE_DELTA_LIGHTS
#   ifndef DISABLE_ENVIRONMENT_LIGHTS
    /*lightType == kLight_Environment*/
    {
        // Get the environment light
        LightEnvironment light = MakeLightEnvironment(selectedLight);

        return INV_PI;
    }
#   endif // DISABLE_ENVIRONMENT_LIGHTS
#endif
}

#endif // LIGHT_SAMPLING_VOLUME_HLSL
