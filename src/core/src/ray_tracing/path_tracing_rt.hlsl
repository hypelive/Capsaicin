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

#ifndef PATH_TRACING_RT_HLSL
#define PATH_TRACING_RT_HLSL

#ifndef USE_INLINE_RT
#   define USE_INLINE_RT 0
#endif

#include "path_tracing.hlsl"

/** Ray Tracing AnyHit shader function.
 * @param attr The intersection attributes.
 */
void pathAnyHit(BuiltInTriangleIntersectionAttributes attr)
{
#ifndef DISABLE_ALPHA_TESTING
    if (!AlphaTest(GetHitInfoRt(attr)))
    {
        IgnoreHit();
    }
#endif
}

/** Ray Tracing AnyHit shader function for shadow rays.
 * @param attr The intersection attributes.
 */
void pathShadowAnyHit(BuiltInTriangleIntersectionAttributes attr)
{
#ifndef DISABLE_ALPHA_TESTING
    if (!AlphaTest(GetHitInfoRt(attr)))
    {
        IgnoreHit();
    }
#endif
}

/** Ray Tracing ClosestHit shader function.
 * @param [in,out] path The path data.
 * @param attr          The intersection attributes.
 * @param minBounces    The minimum number of allowed bounces along path segment before termination.
 * @param maxBounces    The maximum number of allowed bounces along path segment.
 */
void pathClosestHit(inout PathData path, BuiltInTriangleIntersectionAttributes attr, uint minBounces, uint maxBounces)
{
    RayInfo ray = GetRayInfoRt();
    HitInfo hitData = GetHitInfoRt(attr);
    IntersectData iData = MakeIntersectData(hitData);
    path.terminated = !pathHit(ray, hitData, iData, path.randomStratified, path.randomNG,
        path.bounce, minBounces, maxBounces, path.normal, path.samplePDF, path.throughput, path.radiance);
    path.origin = ray.origin;
    path.direction = ray.direction;
}

/** Ray Tracing Miss shader function.
 * @param [in,out] pathData The path data.
 */
void pathMiss(inout PathData path)
{
    shadePathMissFunc(GetRayInfoRt(), path.bounce, path.randomNG, path.normal, path.samplePDF, path.throughput, path.radiance);
    path.terminated = true;
}

/** Ray Tracing Miss shader function for shadow rays.
 * @param [out] payload The shadow ray payload.
 */
void pathShadowMiss(out ShadowRayPayload payload)
{
    payload.visible = true;
}

#endif // PATH_TRACING_HLSL
