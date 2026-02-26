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

#ifndef PATH_TRACING_HLSL
#define PATH_TRACING_HLSL

#include "path_tracing_shared.h"

#include "trace_ray.hlsl"
#include "intersect_data.hlsl"
#include "components/light_builder/light_builder.hlsl"
#include "components/random_number_generator/random_number_generator.hlsl"
#include "components/stratified_sampler/stratified_sampler.hlsl"
#include "geometry/mis.hlsl"
#include "lights/light_sampling.hlsl"
#include "materials/material_sampling.hlsl"
#include "math/transform.hlsl"

#ifndef USE_INLINE_RT
#   define USE_INLINE_RT 1
#endif

/**
 * The default payload for shading functions is just standard float3 radiance.
 * However, if USE_CUSTOM_HIT_PAYLOAD is defined by any code including this header then instead
 * the payload will be the user supplied CustomPayLoad struct.
 */
#ifdef USE_CUSTOM_HIT_PAYLOAD
typedef CustomPayLoad pathPayload;
#else
typedef float3 pathPayload;
#endif

struct [raypayload] ShadowRayPayload
{
    bool visible : read(caller) : write(caller, miss);
};

struct [raypayload] PathData
{
    Random randomNG                    : read(closesthit, miss, caller) : write(caller, closesthit, miss); /**< Random number generator */
    StratifiedSampler randomStratified : read(closesthit, caller)       : write(caller, closesthit); /**< Stratified random number generator instance */
    float3 throughput                  : read(closesthit, miss, caller) : write(caller, closesthit); /**< Accumulated ray throughput for current path segment */
    pathPayload radiance               : read(closesthit, miss, caller) : write(caller, closesthit, miss); /**< Accumulated radiance for the current path segment */
    float samplePDF                    : read(closesthit, miss, caller) : write(caller, closesthit); /**< The PDF of the last sampled BRDF */
    float3 normal                      : read(closesthit, miss, caller) : write(caller, closesthit); /**< The surface normal at the location the current path originated from */
    uint bounce                        : read(closesthit, miss, caller) : write(caller);     /**< Bounce depth of current path segment */
    float3 origin                      : read(caller)                   : write(closesthit); /**< Return value for new path segment start location */
    float3 direction                   : read(caller)                   : write(closesthit); /**< Return value for new path segment direction */
    bool terminated                    : read(caller)                   : write(closesthit, miss); /**< Return value to indicated current paths terminates */
};

/**
 * Add new radiance to the existing radiance.
 * @param [in,out] radiance The existing radiance.
 * @param newRadiance       The new radiance to add.
 * @param bounce            The current bounce depth.
 */
void addRadiance(inout float3 radiance, float3 newRadiance, uint bounce)
{
    radiance += newRadiance;
}

/**
 * Set the hit distance for a shaded path hit.
 * @param [in,out] radiance The existing radiance.
 * @param rayOrigin         The origin of the ray.
 * @param position          The position of the surface the ray hit.
 */
void addHitDistance(inout float3 radiance, float3 rayOrigin, float3 position)
{
    // Nothing to do
}

/**
 * Calculate any radiance from a missed path segment.
 * @param ray               The traced ray that missed any surfaces.
 * @param currentBounce     The current number of bounces along path for current segment.
 * @param randomNG          Random number generator.
 * @param normal            Shading normal vector at start of path segment.
 * @param samplePDF         The PDF of sampling the current paths direction.
 * @param throughput        The current paths combined throughput.
 * @param [in,out] radiance The combined radiance. Any new radiance is added to the existing value and returned.
 */
template<typename RadianceT>
void shadePathMiss(RayInfo ray, uint currentBounce, inout Random randomNG, float3 normal, float samplePDF,
    float3 throughput, inout RadianceT radiance)
{
#if !defined(DISABLE_NON_NEE) && !defined(DISABLE_ENVIRONMENT_LIGHTS)
#   if defined(DISABLE_DIRECT_LIGHTING)
    if (currentBounce == 1) return;
#   endif
    if (hasEnvironmentLight())
    {
        // If nothing was hit then load the environment map
        LightEnvironment light = getEnvironmentLight();
        float3 lightRadiance = evaluateEnvironmentLight(light, ray.direction);
#   if !defined(DISABLE_NEE)
        if (currentBounce != 0)
        {
            // Account for light contribution along sampled direction
            float lightPDF = sampleEnvironmentLightPDF(light, ray.direction, normal);
            lightPDF *= sampleLightsPDF(randomNG, 0, ray.origin, normal);
            if (lightPDF != 0.0f)
            {
                float weight = heuristicMIS(samplePDF, lightPDF);
                addRadiance(radiance, throughput * lightRadiance * weight, currentBounce);
            }
        }
        else
#   endif // !DISABLE_NON_NEE
        {
            addRadiance(radiance, throughput * lightRadiance, currentBounce);
        }
    }
#endif // !DISABLE_NON_NEE && !DISABLE_ENVIRONMENT_LIGHTS
}

/**
 * Calculate any radiance from a hit path segment.
 * @param ray               The traced ray that hit a surface.
 * @param hitData           Data associated with the hit surface.
 * @param iData             Retrieved data associated with the hit surface.
 * @param randomNG          Random number generator.
 * @param currentBounce     The current number of bounces along path for current segment.
 * @param normal            Shading normal vector at start of path segment (Only valid if bounce > 0).
 * @param samplePDF         The PDF of sampling the current paths direction.
 * @param throughput        The current paths combined throughput.
 * @param [in,out] radiance The combined radiance. Any new radiance is added to the existing value and returned.
 */
template<typename RadianceT>
void shadePathHit(RayInfo ray, HitInfo hitData, IntersectData iData, inout Random randomNG,
    uint currentBounce, float3 normal, float samplePDF, float3 throughput, inout RadianceT radiance)
{
    if (currentBounce == 1)
    {
        addHitDistance(radiance, ray.origin, iData.position);
#if defined(DISABLE_DIRECT_LIGHTING)
        return;
#endif
    }
#if !defined(DISABLE_NON_NEE) && !defined(DISABLE_AREA_LIGHTS)
    // Get material emissive values
    if (any(iData.material.emissivity.xyz > 0.0f))
    {
        // Get material properties at intersection
        float4 areaEmissivity = emissiveAlphaScaled(iData.material, iData.uv);
        // Get light contribution
        LightArea emissiveLight = MakeLightArea(iData.vertex0, iData.vertex1, iData.vertex2,
            areaEmissivity, iData.uv, iData.uv, iData.uv);
        float3 lightRadiance = evaluateAreaLight(emissiveLight, 0.0f.xx/*Use bogus barycentrics as correct UV is already stored*/);
#   if !defined(DISABLE_NEE)
        if (currentBounce != 0)
        {
            // Account for light contribution along sampled direction
            float lightPDF = sampleAreaLightPDF(emissiveLight, ray.origin, iData.position);
            lightPDF *= sampleLightsPDF(randomNG, getAreaLightIndex(hitData.instanceIndex, hitData.primitiveIndex), ray.origin, normal);
            if (lightPDF != 0.0f)
            {
                float weight = heuristicMIS(samplePDF, lightPDF);
                addRadiance(radiance, throughput * lightRadiance * weight, currentBounce);
            }
        }
        else
#   endif // !DISABLE_NON_NEE
        {
            addRadiance(radiance, throughput * lightRadiance, currentBounce);
        }
    }
#endif // !DISABLE_NON_NEE && !DISABLE_AREA_LIGHTS
}

/**
 * Calculate any radiance from a hit light.
 * @param ray               The traced ray that hit a surface.
 * @param material          Material data describing BRDF of surface.
 * @param currentBounce     The current number of bounces along path for current segment.
 * @param normal            Shading normal vector at current position.
 * @param viewDirection     Outgoing ray view direction.
 * @param throughput        The current paths combined throughput.
 * @param lightPDF          The PDF of sampling the returned light direction.
 * @param radianceLi        The radiance visible along sampled light.
 * @param selectedLight     The light that was selected for sampling.
 * @param [in,out] radiance The combined radiance. Any new radiance is added to the existing value and returned.
 */
template<typename RadianceT>
void shadeLightHit(RayInfo ray, MaterialBRDF material, uint currentBounce, float3 normal, float3 viewDirection, float3 throughput,
    float lightPDF, float3 radianceLi, Light selectedLight, inout RadianceT radiance)
{
#if defined(DISABLE_NON_NEE) || (defined(DISABLE_AREA_LIGHTS) && defined(DISABLE_ENVIRONMENT_LIGHTS))
    float3 sampleReflectance = evaluateBRDF(material, normal, viewDirection, ray.direction);
    addRadiance(radiance, throughput * sampleReflectance * radianceLi / lightPDF, currentBounce);
#else
    // Evaluate BRDF for new light direction and calculate PDF for current sample
    float3 sampleReflectance;
    float samplePDF = sampleBRDFPDFAndEvalute(material, normal, viewDirection, ray.direction, sampleReflectance);
    if (samplePDF != 0.0f)
    {
        bool deltaLight = isDeltaLight(selectedLight);
        float weight = (!deltaLight) ? heuristicMIS(lightPDF, samplePDF) : 1.0f;
        addRadiance(radiance, throughput * sampleReflectance * radianceLi * (weight / lightPDF), currentBounce);
    }
#endif // DISABLE_NON_NEE
}

#ifdef USE_CUSTOM_HIT_FUNCTIONS
#   define shadePathMissFunc shadePathMissCustom
#   define shadeLightHitFunc shadeLightHitCustom
#   define shadePathHitFunc shadePathHitCustom
#else
#   define shadePathMissFunc shadePathMiss
#   define shadeLightHitFunc shadeLightHit
#   define shadePathHitFunc shadePathHit
#endif

/**
 * Calculates a new light ray direction from a surface by sampling the scenes lighting.
 * @param randomStratified    Random number sampler used to sample light surface.
 * @param randomNG            Random number generator.
 * @param position            Current position on surface.
 * @param normal              Shading normal vector at current position.
 * @param geometryNormal      Surface normal vector at current position.
 * @param [out] ray           The ray containing the new light ray parameters (may not be normalised).
 * @param [out] lightPDF      The PDF of sampling the returned light direction.
 * @param [out] radianceLi    The radiance visible along sampled light.
 * @param [out] selectedLight The light that was selected for sampling.
 * @return True if light path was generated, False if no ray returned.
 */
bool sampleLightsNEEDirection(inout StratifiedSampler randomStratified, inout Random randomNG, float3 position,
    float3 normal, float3 geometryNormal, out RayInfo ray, out float lightPDF, out float3 radianceLi, out Light selectedLight)
{
    uint lightIndex = sampleLights(randomNG, position, normal, lightPDF);
    if (lightPDF == 0.0f)
    {
        return false;
    }

    // Initialise returned radiance
    float3 lightPosition;
    float3 lightDirection;
    selectedLight = getLight(lightIndex);
    float sampledLightPDF;
    float2 unused;
    radianceLi = sampleLight(selectedLight, randomStratified, position, normal, lightDirection, sampledLightPDF, lightPosition, unused);

    // Combine PDFs
    lightPDF *= sampledLightPDF;

    // Early discard lights behind surface
    if (dot(lightDirection, geometryNormal) < 0.0f || dot(lightDirection, normal) < 0.0f || lightPDF == 0.0f)
    {
        return false;
    }

    // Create shadow ray
    ray = MakeRayInfoShadow(position, geometryNormal, lightPosition, lightDirection, selectedLight);
    return true;
}

/**
 * Calculates radiance from a new light ray direction from a surface by sampling the scenes lighting.
 * @param material          Material data describing BRDF of surface.
 * @param randomStratified  Random number sampler used to sample light.
 * @param randomNG          Random number generator.
 * @param currentBounce     The current number of bounces along path for current segment.
 * @param position          Current position on surface.
 * @param normal            Shading normal vector at current position.
 * @param geometryNormal    Surface normal vector at current position.
 * @param viewDirection     Outgoing ray view direction.
 * @param throughput        The current paths combined throughput.
 * @param [in,out] radiance The combined radiance. Any new radiance is added to the existing value and returned.
 */
template<typename RadianceT>
void sampleLightsNEE(MaterialBRDF material, inout StratifiedSampler randomStratified, inout Random randomNG, uint currentBounce,
    float3 position, float3 normal, float3 geometryNormal, float3 viewDirection, float3 throughput, inout RadianceT radiance)
{
#if !defined(DISABLE_NEE)
    // Get sampled light direction
    float lightPDF;
    RayInfo ray;
    float3 radianceLi;
    Light selectedLight;
    if (!sampleLightsNEEDirection(randomStratified, randomNG, position, normal, geometryNormal, ray, lightPDF, radianceLi, selectedLight))
    {
        return;
    }

    // Trace shadow ray
#   if USE_INLINE_RT
    ShadowRayQuery rayShadowQuery = TraceRay<ShadowRayQuery>(ray);
    bool visible = rayShadowQuery.CommittedStatus() == COMMITTED_NOTHING;
#   else
    ShadowRayPayload payload = {false};
    TraceRayShadow(ray, payload);
    bool visible = payload.visible;
#   endif

    // If nothing was hit then we have hit the light
    if (visible)
    {
        // Normalise ray direction
        ray.direction = normalize(ray.direction);
        ray.range.y = FLT_MAX;

        // Add lighting contribution
        shadeLightHitFunc(ray, material, currentBounce, normal, viewDirection, throughput, lightPDF, radianceLi, selectedLight, radiance);
    }
#endif // DISABLE_NEE
}

/**
 * Calculate the next segment along a path after a valid surface hit.
 * @param materialBRDF        The material on the hit surface.
 * @param randomStratified    Random number sampler used for sampling.
 * @param position            Current position on surface.
 * @param normal              Shading normal vector at current position.
 * @param geometryNormal      Surface normal vector at current position.
 * @param viewDirection       Outgoing ray view direction.
 * @param [in,out] radiance   The radiance payload.
 * @param [in,out] throughput Combined throughput for current path.
 * @param [out] ray           New outgoing path segment.
 * @param [out] samplePDF     The PDF of sampling the new paths direction.
 * @return True if path has new segment, False if path should be terminated.
 */
template<typename RadianceT>
bool pathNext(MaterialBRDF materialBRDF, inout StratifiedSampler randomStratified,
    float3 position, float3 normal, float3 geometryNormal, float3 viewDirection, inout RadianceT radiance,
    inout float3 throughput, out RayInfo ray, out float samplePDF)
{
    // Sample BRDF to get next ray direction
    float3 sampleReflectance;
    samplePDF = 0.0F;
    float3 rayDirection = sampleBRDFAndEvaluate(materialBRDF, randomStratified, normal, viewDirection, sampleReflectance, samplePDF);

    // Prevent tracing directions below the surface
    if (dot(geometryNormal, rayDirection) <= 0.0f || samplePDF == 0.0f)
    {
        return false;
    }

    // Add sampling weight to current weight
    throughput *= sampleReflectance / samplePDF;

    // Update path information
    ray = MakeRayInfoClosest(position, geometryNormal, rayDirection);

    return true;
}

#ifdef USE_CUSTOM_PATH_NEXT
#   define pathNextFunc pathNextCustom
#else
#   define pathNextFunc pathNext
#endif

/**
 * Handle case when a traced ray hits a surface.
 * @param [in,out] ray        The traced ray that hit a surface (returns ray for next path segment).
 * @param hitData             Data associated with the hit surface.
 * @param iData               Retrieved data associated with the hit surface.
 * @param randomStratified    Random number sampler used for sampling.
 * @param randomNG            Random number generator.
 * @param currentBounce       The current number of bounces along path for current segment.
 * @param minBounces          The minimum number of allowed bounces along path segment before termination.
 * @param maxBounces          The maximum number of allowed bounces along path segment.
 * @param [in,out] normal     Shading normal vector at path segments origin (returns shading normal at current position).
 * @param [in,out] samplePDF  The PDF of sampling the current path segments direction (returns the PDF of sampling the new paths direction).
 * @param [in,out] throughput Combined throughput for current path.
 * @param [in,out] radiance   The visible radiance contribution of the path hit.
 * @return True if path has new segment, False if path should be terminated.
 */
template<typename RadianceT>
bool pathHit(inout RayInfo ray, HitInfo hitData, IntersectData iData, inout StratifiedSampler randomStratified,
    inout Random randomNG, uint currentBounce, uint minBounces, uint maxBounces, inout float3 normal,
    inout float samplePDF, inout float3 throughput, inout RadianceT radiance)
{
    // Shade current position
    shadePathHitFunc(ray, hitData, iData, randomNG, currentBounce, normal, samplePDF, throughput, radiance);

    // Terminate early if no more bounces
    if (currentBounce >= maxBounces)
    {
        return false;
    }

    float3 viewDirection = -ray.direction;
    // Stop if surface normal places ray behind surface (note surface normal != geometric normal)
    //  Currently disabled due to incorrect normals generated by normal mapping when not using displacement/parallax
    //if (dot(iData.normal, viewDirection) <= 0.0f)
    //{
    //    return false;
    //}

    MaterialBRDF materialBRDF = MakeMaterialBRDF(iData.material, iData.uv);
#if defined(DISABLE_ALBEDO_MATERIAL)
    // Disable material albedo if requested
    if (currentBounce == 0)
    {
        materialBRDF.albedo = 0.3f.xxx;
#   if !defined(DISABLE_SPECULAR_MATERIALS)
        materialBRDF.F0 = 0.0f.xxx;
#   endif // !DISABLE_SPECULAR_MATERIALS
    }
#endif // DISABLE_ALBEDO_MATERIAL

#if !defined(DISABLE_NEE)
#   if defined(DISABLE_DIRECT_LIGHTING)
    // Disable direct lighting if requested
    if (currentBounce > 0)
#   endif // DISABLE_DIRECT_LIGHTING
    {
        // Sample a single light
        sampleLightsNEE(materialBRDF, randomStratified, randomNG, currentBounce, iData.position,
            iData.normal, iData.geometryNormal, viewDirection, throughput, radiance);
    }
#endif // DISABLE_NEE

#if defined(DISABLE_NON_NEE) && (defined(DISABLE_AREA_LIGHTS) && defined(DISABLE_ENVIRONMENT_LIGHTS))
    return false;
#else
    normal = iData.normal;
    // Sample to get next ray direction
    if (!pathNextFunc(materialBRDF, randomStratified, iData.position, normal, iData.geometryNormal,
        viewDirection, radiance, throughput, ray, samplePDF))
    {
        // Terminate path if no new segment
        return false;
    }

    // Russian Roulette early termination
    if (currentBounce > minBounces)
    {
        float rrSample = hmax(throughput);
        if (rrSample <= randomNG.rand())
        {
            return false;
        }
        throughput /= rrSample;
    }

    return true;
#endif
}

/**
 * Trace a new path.
 * @param ray               The ray for the first path segment.
 * @param randomStratified  Random number sampler used for sampling.
 * @param randomNG          Random number generator.
 * @param currentBounce     The current number of bounces along path for current segment.
 * @param minBounces        The minimum number of allowed bounces along path segment before termination.
 * @param maxBounces        The maximum number of allowed bounces along path segment.
 * @param normal            The shading normal of the current surface (Only valid if bounce > 0).
 * @param throughput        Initial combined throughput for current path.
 * @param [in,out] radiance The visible radiance contribution of the path hit.
 */
template<typename RadianceT>
void tracePath(RayInfo ray, inout StratifiedSampler randomStratified, inout Random randomNG,
    uint currentBounce, uint minBounces, uint maxBounces, float3 normal, float3 throughput, inout RadianceT radiance)
{
    // Initialise per-sample path tracing values
#if USE_INLINE_RT
    float samplePDF = 1.0f; // The PDF of the last sampled BRDF
#else
    PathData pathData;
    pathData.radiance = radiance;
    pathData.throughput = throughput;
    pathData.samplePDF = 1.0f;
    pathData.terminated = false;
    pathData.randomNG = randomNG;
    pathData.randomStratified = randomStratified;
#endif

    for (uint bounce = currentBounce; bounce <= maxBounces; ++bounce)
    {
        // Trace the ray through the scene
#if USE_INLINE_RT
        ClosestRayQuery rayQuery = TraceRay<ClosestRayQuery>(ray);

        // Check for valid intersection
        if (rayQuery.CommittedStatus() == COMMITTED_NOTHING)
        {
            shadePathMissFunc(ray, bounce, randomNG, normal, samplePDF, throughput, radiance);
            break;
        }
        else
        {
            // Get the intersection data
            HitInfo hitData = GetHitInfoRtInlineCommitted(rayQuery);
            IntersectData iData = MakeIntersectData(hitData);
            if (!pathHit(ray, hitData, iData, randomStratified, randomNG,
                bounce, minBounces, maxBounces, normal, samplePDF, throughput, radiance))
            {
                break;
            }
        }
#else
        pathData.bounce = bounce;
        TraceRayClosest(ray, pathData);
        // Create new ray
        ray.origin = pathData.origin;
        ray.direction = pathData.direction;
        ray.range = float2(0.0f, FLT_MAX);

        if (pathData.terminated)
        {
            break;
        }
#endif
    }

#if !USE_INLINE_RT
    radiance = pathData.radiance;
#endif
}

/**
 * Trace a new path from beginning.
 * @param ray               The ray for the first path segment.
 * @param randomStratified  Random number sampler used for sampling.
 * @param randomNG          Random number generator.
 * @param minBounces        The minimum number of allowed bounces along path segment before termination.
 * @param maxBounces        The maximum number of allowed bounces along path segment.
 * @param [in,out] radiance The visible radiance contribution of the path hit.
 */
template<typename RadianceT>
void traceFullPath(RayInfo ray, inout StratifiedSampler randomStratified, inout Random randomNG,
    uint minBounces, uint maxBounces, inout RadianceT radiance)
{
    tracePath(ray, randomStratified, randomNG, 0, minBounces, maxBounces, 0.0f.xxx, 1.0f.xxx, radiance);
}

#endif // PATH_TRACING_HLSL
