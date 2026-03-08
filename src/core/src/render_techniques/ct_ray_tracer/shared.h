#ifndef CT_RT_SHARED_H
#define CT_RT_SHARED_H

#include "gpu_shared.h"

#ifndef __cplusplus

#    define ShadedVertex Vertex

#endif

// Per draw call data
struct InstanceData
{
    uint instanceId;
    uint vertexOffset;
    uint numVertices;
    uint padding;
};

// TODO For some reason the float3x4 is defined as 4x3 matrix on CPU. It leads to artifacts.
struct VertexShadingConstants
{
    float4x4 view;
};

struct RtConstants
{
    uint2  resolution;
    float2 invResolution;
    uint   numInstances;
    float  lensDistortion;
    uint   frameIndex;
};

// TODO Move to some header?
#ifndef __cplusplus
struct CtRay
{
    float3 o;
    uint   _pad0;
    float3 d;
    uint   _pad1;
};

struct CtRayGenerator
{
    float3 m_origin;
    uint   _pad0;
    float3 m_right;
    uint   _pad1;
    float3 m_up;
    uint   _pad2;

    void setup(float3 origin, float3 right, float3 up)
    {
        m_origin = origin;
        m_right  = right;
        m_up     = up;
    }

    CtRay generateRay(float2 uv)
    {
        float3 from    = m_origin;
        float3 forward = normalize(cross(m_up, m_right));
        float3 to      = m_origin + forward + lerp(-m_right, m_right, uv.x) + lerp(m_up, -m_up, uv.y);

        CtRay ray;
        ray.o = from;
        ray.d = normalize(to - from);

        return ray;
    }
};

struct CtLensDistortionRayGenerator
{
    float3 m_origin;
    float  m_distortion;
    float3 m_right;
    uint   _pad1;
    float3 m_up;
    uint   _pad2;

    void setup(float3 origin, float3 right, float3 up)
    {
        m_origin     = origin;
        m_right      = right;
        m_up         = up;
    }

    CtRay generateRay(float2 uv)
    {
        float3 from    = m_origin;
        float3 forward = normalize(cross(m_up, m_right));
        float3 X       = lerp(-m_right, m_right, uv.x);
        float3 Y       = lerp(m_up, -m_up, uv.y);
        float3 p       = X + Y;
        float  r2      = dot(p, p);
        float  d       = m_distortion;
        float  c       = d * r2;
        float3 to      = m_origin + forward + p / (1.0f + c);

        CtRay ray;
        ray.o = from;
        ray.d = normalize(to - from);

        return ray;
    }
};
#endif

#endif
