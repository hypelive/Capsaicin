#include "brdf_lut_baker_shared.h"
#include "math/math.hlsl"
#include "math/sampling.hlsl"

float2 g_invScreenSize;

struct Pixel
{
    // F0 Scale, Offset
    float2 brdf : SV_Target0;
};

// https://learnopengl.com/PBR/IBL/Specular-IBL
float GeometrySchlickGGX(float NdotV, float alpha)
{
    float k = alpha / 2.0;

    float nom   = NdotV;
    float denom = NdotV * (1.0 - k) + k;

    return nom / denom;
}

float GeometrySmith(float NdotV, float NdotL, float alpha)
{
    float ggx2 = GeometrySchlickGGX(NdotV, alpha);
    float ggx1 = GeometrySchlickGGX(NdotL, alpha);

    return ggx1 * ggx2;
}

Pixel main(in VertexParams params)
{
    Pixel pixel;

    // X - cosTheta, Y - roughness.
    float2 uv = params.screenPosition.xy * g_invScreenSize;
    float NdotV = uv.x;
    float3 viewDirection = float3(sqrt(1.0f - NdotV * NdotV), 0.0f, NdotV);
    float roughness = uv.y;
    float a = roughness * roughness;

    float3 normal = float3(0.0f, 0.0f, 1.0f);
    float3 tangent = cross(float3(1.0f, 0.0f, 0.0f), normal);
    float3 bitangent = cross(normal, tangent);
    float3x3 tangentMatrix = float3x3(tangent.x, bitangent.x, normal.x,
                                      tangent.y, bitangent.y, normal.y,
                                      tangent.z, bitangent.z, normal.z);

    const uint C_SAMPLE_COUNT = 1024u;
    float2 resultScaleOffset = float2(0.0f, 0.0f);

    for (uint sampleIndex = 0u; sampleIndex < C_SAMPLE_COUNT; ++sampleIndex)
    {
        float2 xi = Hammersley2D(sampleIndex, C_SAMPLE_COUNT);

        // https://blog.selfshadow.com/publications/s2013-shading-course/karis/s2013_pbs_epic_notes_v2.pdf
        float phi = 2.0 * PI * xi.x;
        float2 sinCosTheta;
        sinCosTheta.y = sqrt((1.0 - xi.y) / (1.0 + (a * a - 1.0) * xi.y));
        sinCosTheta.x = sqrt(1.0 - sinCosTheta.y * sinCosTheta.y);
        float2 sinCosPhi;
        sincos(phi, sinCosPhi.x, sinCosPhi.y);

        float3 microsurfaceNormalDirection = float3(sinCosTheta.x * sinCosPhi.y, sinCosTheta.x * sinCosPhi.x, sinCosTheta.y);
        microsurfaceNormalDirection = mul(tangentMatrix, microsurfaceNormalDirection);
        float3 lightDirection = -reflect(viewDirection, microsurfaceNormalDirection);
        float NdotL = max(0.0f, dot(normal, lightDirection));
        if (NdotL > 0.0f)
        {
            float NdotH = max(0.0f, dot(normal, microsurfaceNormalDirection));
            float VdotH = max(0.0f, dot(viewDirection, microsurfaceNormalDirection));

            float G = GeometrySmith(NdotV, NdotL, a);
            float FresnelCoefficient = pow(1.0 - VdotH, 5.0);

            resultScaleOffset += ((G * VdotH) / (NdotH * NdotV)) * float2((1.0 - FresnelCoefficient), FresnelCoefficient);
        }
    }

    resultScaleOffset /= C_SAMPLE_COUNT;
    pixel.brdf = resultScaleOffset;

    return pixel;
}
