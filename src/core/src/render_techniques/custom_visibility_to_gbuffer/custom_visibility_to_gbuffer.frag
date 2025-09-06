#include "custom_visibility_to_gbuffer_shared.h"
#include "math/math.hlsl"
#include "math/color.hlsl"
#include "math/pack.hlsl"
#include "math/transform.hlsl"
#include "lights/lights.hlsl"

Texture2D g_TextureMaps[] : register(space99);
SamplerState g_TextureSampler;

#include "materials/materials.hlsl"

ConstantBuffer<VisibilityToGbufferConstants> g_DrawConstants;
Texture2D<uint2> g_VisibilityBuffer;
StructuredBuffer<Instance> g_InstanceBuffer;
StructuredBuffer<float3x4> g_TransformBuffer;
StructuredBuffer<Meshlet> g_MeshletBuffer;
StructuredBuffer<uint> g_MeshletPackBuffer;
StructuredBuffer<Vertex> g_VertexBuffer;
StructuredBuffer<Material> g_MaterialBuffer;
ConstantBuffer<LightsBufferInfo> g_LightsBufferInfo;
StructuredBuffer<Light> g_LightsBuffer;
TextureCube<float4> g_IrradianceProbe;
TextureCube<float4> g_PrefilteredEnvironmentMap;
Texture2D<float2> g_BrdfLut;
SamplerState g_LinearSampler;
// We have multiple index data sets for the animations.
uint g_VertexDataIndex;

struct GBuffer
{
    float4 albedoNormalZ : SV_Target0;
    float4 normalXYRoughnessMetallicity : SV_Target1;
    float4 emission : SV_Target2;
};

struct ShadedVertex
{
    float4 screenPosition;
    float3 normal;
    float2 uv;
    float3 worldPosition;
};

ShadedVertex shadeVertex(Vertex vertex, float3x4 transform)
{
    ShadedVertex result;

    float3 worldPosition = transformPoint(vertex.getPosition(), transform);
    float3 normal = transformNormal(vertex.getNormal(), transform);

    result.screenPosition = mul(g_DrawConstants.viewProjection, float4(worldPosition, 1.0f));
    result.worldPosition = worldPosition;
    result.uv = vertex.getUV();
    result.normal = normal;

    return result;
}

ShadedVertex computeInterpolants(float3 barycentrics, ShadedVertex v0, ShadedVertex v1, ShadedVertex v2)
{
    ShadedVertex result;
    result.screenPosition = barycentrics.x * v0.screenPosition + barycentrics.y * v1.screenPosition + barycentrics.z * v2.screenPosition;
    result.worldPosition = barycentrics.x * v0.worldPosition + barycentrics.y * v1.worldPosition + barycentrics.z * v2.worldPosition;
    result.uv = barycentrics.x * v0.uv + barycentrics.y * v1.uv + barycentrics.z * v2.uv;
    result.normal = normalize(barycentrics.x * v0.normal + barycentrics.y * v1.normal + barycentrics.z * v2.normal);
    return result;
}

float2 computeUv(float3 barycentrics, ShadedVertex v0, ShadedVertex v1, ShadedVertex v2)
{
    return barycentrics.x * v0.uv + barycentrics.y * v1.uv + barycentrics.z * v2.uv;
}

float3 computeBarycentrics(float3 rayOrigin, float3 rayDirection, float3 v0, float3 v1, float3 v2)
{
    // Möller–Trumbore intersection algorithm.
    // https://en.wikipedia.org/wiki/M%C3%B6ller%E2%80%93Trumbore_intersection_algorithm
    float3 edge10 = v1 - v0;
    float3 edge20 = v2 - v0;

    float3 DcrossE20 = cross(rayDirection, edge20);
    float det = dot(DcrossE20, edge10);
    if (abs(det) < FLT_EPSILON)
    {
        return 0.0f; // Almost parallel.
    }

    float invDet = rcp(det);
    float3 s = rayOrigin - v0;
    float u = invDet * dot(s, DcrossE20);

    float3 ScrossE10 = cross(s, edge10);
    float v = invDet * dot(rayDirection, ScrossE10);

    return float3(1.0f - u - v, u, v);
}

void calculateTangentBitangent(ShadedVertex v0, ShadedVertex v1, ShadedVertex v2, out float3 tangent, out float3 bitangent)
{
    float3 edge10 = v1.worldPosition - v0.worldPosition;
    float3 edge20 = v2.worldPosition - v0.worldPosition;
    float2 deltaUV10 = v1.uv - v0.uv;
    float2 deltaUV20 = v2.uv - v0.uv;

    float2x3 tangentBitangent = float2x3(1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f);
    float det = deltaUV10.x * deltaUV20.y - deltaUV20.x * deltaUV10.y;
    if (det != 0.0f)
    {
        float2x2 invUV = float2x2(deltaUV20.y, -deltaUV10.y, -deltaUV20.x, deltaUV10.x) / det;
        float2x3 edges = float2x3(edge10, edge20);
        float2x3 tangentBitangent = mul(invUV, edges);
    }

    tangent = tangentBitangent[0];
    bitangent = tangentBitangent[1];
}

float3 calculateNormal(ShadedVertex params, float3 tangent, float3 bitangent, Material material, float2 ddx, float2 ddy)
{
    float3 normal = params.normal;
    uint normalMap = asuint(material.normal_alpha_side.x);
    if (normalMap != uint(-1))
    {
        tangent = normalize(tangent - dot(tangent, normal) * normal);
        float3 newBitangent = cross(normal, tangent);
        newBitangent = dot(newBitangent, bitangent) < 0.0f ? -newBitangent : newBitangent;
        float3x3 TBN = float3x3(tangent.x, newBitangent.x, normal.x,
                                tangent.y, newBitangent.y, normal.y,
                                tangent.z, newBitangent.z, normal.z);
        float3 textureNormal = 2.0f * g_TextureMaps[NonUniformResourceIndex(normalMap)].SampleGrad(g_LinearSampler, params.uv, ddx, ddy).xyz - 1.0f;
        normal = normalize(mul(TBN, textureNormal));
    }

    return normal;
}

float3 calculateEmission(Material material, float2 uv, float2 ddx, float2 ddy)
{
    // TODO scale with alpha?
    float3 result = material.emissivity.xyz;
    uint emissivityTextureId = asuint(material.emissivity.w);
    if (emissivityTextureId != asuint(-1))
    {
        result *= g_TextureMaps[NonUniformResourceIndex(emissivityTextureId)].SampleGrad(g_LinearSampler, uv, ddx, ddy).xyz;
    }

    return result;        
}

void shadeVertices(UnpackedVisibilityBuffer visibilityBufferData, Instance instance, out ShadedVertex v0, out ShadedVertex v1, out ShadedVertex v2)
{
    Meshlet meshlet = g_MeshletBuffer[visibilityBufferData.meshletId];
    float3x4 transform = g_TransformBuffer[instance.transform_index];

    uint indicesOffset = meshlet.data_offset_idx + meshlet.vertex_count + visibilityBufferData.primitiveId;
    uint packedIndices = g_MeshletPackBuffer[indicesOffset];
    uint3 unpackedIndices = uint3(packedIndices & 0x3F, (packedIndices >> 10) & 0x3F, packedIndices >> 20);
    uint instanceVertexOffset = instance.vertex_offset_idx[g_VertexDataIndex];
    uint vertexOffset0 = instanceVertexOffset + g_MeshletPackBuffer[meshlet.data_offset_idx + unpackedIndices.x];
    v0 = shadeVertex(g_VertexBuffer[vertexOffset0], transform);
    uint vertexOffset1 = instanceVertexOffset + g_MeshletPackBuffer[meshlet.data_offset_idx + unpackedIndices.y];
    v1 = shadeVertex(g_VertexBuffer[vertexOffset1], transform);
    uint vertexOffset2 = instanceVertexOffset + g_MeshletPackBuffer[meshlet.data_offset_idx + unpackedIndices.z];
    v2 = shadeVertex(g_VertexBuffer[vertexOffset2], transform);
}

float3 calculateCameraDirection(float2 screenCoordinates)
{
    float2 uv = screenCoordinates * g_DrawConstants.invScreenSize;
    float2 ndc = 2.0f * float2(uv.x, 1.0f - uv.y) - 1.0f;

    // Translate point from near plane (we have inversed depth) to the world space.
    float4 nearPlaneWorldPosition = mul(g_DrawConstants.invViewProjection, float4(ndc, 1.0f, 1.0f));
    nearPlaneWorldPosition.xyz /= nearPlaneWorldPosition.w;

    return normalize(nearPlaneWorldPosition.xyz - g_DrawConstants.cameraPosition.xyz);
}

GBuffer main(in VertexParams params)
{
    UnpackedVisibilityBuffer visibilityBufferData = unpackVisibilityBuffer(g_VisibilityBuffer.Load(int3(params.screenPosition.xy, 0)));
    Instance instance = g_InstanceBuffer[visibilityBufferData.instanceId];

    ShadedVertex vertex0, vertex1, vertex2;
    shadeVertices(visibilityBufferData, instance, vertex0, vertex1, vertex2);

    const float3 cameraDirection00 = calculateCameraDirection(params.screenPosition.xy);
    const float3 cameraDirection10 = calculateCameraDirection(params.screenPosition.xy + float2(1.0f, 0.0f));
    const float3 cameraDirection01 = calculateCameraDirection(params.screenPosition.xy + float2(0.0f, 1.0f));

    float3 barycentrics00 = computeBarycentrics(g_DrawConstants.cameraPosition.xyz, cameraDirection00,
        vertex0.worldPosition, vertex1.worldPosition, vertex2.worldPosition);
    float3 barycentrics10 = computeBarycentrics(g_DrawConstants.cameraPosition.xyz, cameraDirection10,
        vertex0.worldPosition, vertex1.worldPosition, vertex2.worldPosition);
    float3 barycentrics01 = computeBarycentrics(g_DrawConstants.cameraPosition.xyz, cameraDirection01,
        vertex0.worldPosition, vertex1.worldPosition, vertex2.worldPosition);

    ShadedVertex interpolants = computeInterpolants(barycentrics00, vertex0, vertex1, vertex2);
    float2 uv10 = computeUv(barycentrics10, vertex0, vertex1, vertex2);
    float2 uv01 = computeUv(barycentrics01, vertex0, vertex1, vertex2);
    float2 duvdx = uv10 - interpolants.uv;
    float2 duvdy = uv01 - interpolants.uv;

    Material material = g_MaterialBuffer[instance.material_index];
    MaterialEvaluated materialEvaluated = MakeMaterialEvaluated(material, interpolants.uv, duvdx, duvdy);

    float3 tangent, bitangent;
    calculateTangentBitangent(vertex0, vertex1, vertex2, tangent, bitangent);
    float3 normal = calculateNormal(interpolants, tangent, bitangent, material, duvdx, duvdy);
    float3 packedNormal = normal * 0.5f + 0.5f;

    GBuffer gBuffer;
    gBuffer.albedoNormalZ = float4(materialEvaluated.albedo, packedNormal.z);
    gBuffer.normalXYRoughnessMetallicity = float4(packedNormal.xy, materialEvaluated.roughness, materialEvaluated.metallicity);
    gBuffer.emission = float4(calculateEmission(material, interpolants.uv, duvdx, duvdy), 0.0f);

    return gBuffer;
}
