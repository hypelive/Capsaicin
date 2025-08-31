#include "custom_shading_shared.h"
#include "math/math.hlsl"
#include "math/color.hlsl"
#include "math/pack.hlsl"
#include "math/transform.hlsl"
#include "lights/lights.hlsl"

Texture2D g_TextureMaps[] : register(space99);
SamplerState g_TextureSampler;

#include "materials/materials.hlsl"

ConstantBuffer<ShadingConstants> g_DrawConstants;
Texture2D<uint> g_VisibilityBuffer;
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

struct Pixel
{
    float4 color : SV_Target0;
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

float3 calculateNormal(ShadedVertex params, float3 tangent, float3 bitangent, Material material)
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
        float3 textureNormal = 2.0f * g_TextureMaps[NonUniformResourceIndex(normalMap)].Sample(g_LinearSampler, params.uv).xyz - 1.0f;
        normal = normalize(mul(TBN, textureNormal));
    }

    return normal;
}

float3 tonemap(float3 radiance)
{
    return radiance / (1.0f + radiance);
}

float3 applyInverseGamma(float3 color)
{
    return pow(color, 1.0f / 2.2f);
}

float3 calculateFresnelSchlick(float VdotH, float3 F0)
{
    return F0 + (1.0f - F0) * pow(saturate(1.0f - VdotH), 5.0f);
}

// https://seblagarde.wordpress.com/2011/08/17/hello-world/
float3 calculateFresnelSchlickWithRoughness(float VdotN, float3 F0, float roughness)
{
    return F0 + (max(1.0f - roughness, F0) - F0) * pow(saturate(1.0f - VdotN), 5.0f);
}

float calculateGGXNormalDistribution(float NdotH, float alpha)
{
    const float alpha2 = alpha * alpha;
    const float numerator = alpha2;
    const float temp = (NdotH * NdotH * (alpha2 - 1.0f) + 1.0f);
    const float denominator = PI * temp * temp;
    return numerator / (denominator + FLT_EPSILON);
}

float calculateSchlickGGXGeometry(float NdotV, float alpha)
{
    const float k = alpha / 2.0f;
    const float denominator = NdotV * (1.0f - k) + k;
    return rcp(denominator + FLT_EPSILON);
}

float calculateSmithGeometry(float NdotV, float NdotL, float alpha)
{
    return calculateSchlickGGXGeometry(saturate(NdotV), alpha) * calculateSchlickGGXGeometry(saturate(NdotL), alpha);
}

// https://graphicrants.blogspot.com/2013/08/specular-brdf-reference.html
float3 calculateDirectRadianceForLight(MaterialBRDF material, float3 normal, float3 viewDirection, float3 lightDirection, float3 radiance)
{
    const float3 halfVector = normalize(viewDirection + lightDirection);

    const float VdotH = dot(viewDirection, halfVector);
    const float NdotH = dot(normal, halfVector);
    const float NdotL = dot(normal, lightDirection);
    const float NdotV = dot(normal, viewDirection);

    const float3 Fresnel = calculateFresnelSchlick(VdotH, material.F0);
    const float NDF = calculateGGXNormalDistribution(NdotH, material.roughnessAlpha);
    const float G = calculateSmithGeometry(NdotV, NdotL, material.roughnessAlpha);

    const float3 specular = (Fresnel * NDF * G) / 4.0f;
    const float3 diffuse = (1.0f - Fresnel) * material.albedo / PI;

    return (specular + diffuse) * max(0.0f, NdotL) * radiance;
}

float3 calculateDirectLighting(MaterialBRDF material, float3 normal, float3 viewDirection, float3 cameraPosition, float3 worldPosition)
{
    float3 radiance = 0.0f;

    // Evaluate directional lights.
    for (uint lightIndex = 0u; lightIndex < g_LightsBufferInfo.directionalLightsCount; ++lightIndex)
    {
        const LightDirectional directionalLight = MakeLightDirectional(g_LightsBuffer[lightIndex]);
        radiance += calculateDirectRadianceForLight(material, normal, viewDirection,
            directionalLight.direction, directionalLight.irradiance);
    }

    // Evaluate point lights.
    for (uint lightIndex = 0u; lightIndex < g_LightsBufferInfo.pointLightsCount; ++lightIndex)
    {
        const LightPoint pointLight = MakeLightPoint(g_LightsBuffer[g_LightsBufferInfo.pointLightsOffset + lightIndex]);
        const float3 lightOffset = pointLight.position - worldPosition;
        const float sqrDistance = dot(lightOffset, lightOffset);
        if (sqrDistance > pointLight.range * pointLight.range)
        {
            continue;
        }

        const float attenuation = rcp(PI * sqrDistance);

        radiance += attenuation * calculateDirectRadianceForLight(material, normal, viewDirection,
            lightOffset / sqrt(sqrDistance), pointLight.intensity);
    }
    
    // Evaluate spot lights.
    for (uint lightIndex = 0u; lightIndex < g_LightsBufferInfo.spotLightsCount; ++lightIndex)
    {
        const LightSpot spotLight = MakeLightSpot(g_LightsBuffer[g_LightsBufferInfo.spotLightsOffset + lightIndex]);
        const float3 lightOffset = spotLight.position - worldPosition;
        const float sqrDistance = dot(lightOffset, lightOffset);
        if (sqrDistance > spotLight.range * spotLight.range)
        {
            continue;
        }

        const float3 lightDirection = lightOffset / sqrt(sqrDistance);
        const float cosTheta = dot(lightDirection, spotLight.direction);

        const float distanceAttenuation = rcp(PI * sqrDistance);
        float angleAttenuation = saturate(cosTheta * spotLight.angleCutoffScale + spotLight.angleCutoffOffset);
        angleAttenuation *= angleAttenuation;
        if (angleAttenuation <= 0.0f)
        {
            continue;
        }

        radiance += distanceAttenuation * angleAttenuation * calculateDirectRadianceForLight(material, normal, viewDirection,
            lightDirection, spotLight.intensity);
    }

    // TODO Evaluate area lights.

    return radiance;
}

float3 calculateIndirectLighting(MaterialBRDF material, float3 normal, float3 viewDirection)
{
    const float VdotN = max(0.0f, dot(viewDirection, normal));
    const float3 Fresnel = calculateFresnelSchlickWithRoughness(VdotN, material.F0, material.roughnessAlpha);
    const float3 diffuse = (1.0f - Fresnel) * material.albedo / PI * g_IrradianceProbe.SampleLevel(g_LinearSampler, normal, 0).rgb;

    const float C_PREFILTERED_MAP_MAX_MIP = 4.0f;
    const float prefilteredMapMip = material.roughnessAlpha * C_PREFILTERED_MAP_MAX_MIP;
    const float3 reflectionVector = reflect(-viewDirection, normal);
    const float3 prefilteredColor = g_PrefilteredEnvironmentMap.SampleLevel(g_LinearSampler, reflectionVector, prefilteredMapMip).rgb;
    const float2 brdf = g_BrdfLut.SampleLevel(g_LinearSampler, float2(VdotN, material.roughnessAlpha), 0.0f).xy;
    const float3 specular = Fresnel * prefilteredColor * (material.F0 * brdf.x + brdf.y);

    // TODO add AO.
    return diffuse + specular;
}

Pixel main(in VertexParams params)
{
    Pixel pixel;

    UnpackedVisibilityBuffer visibilityBufferData = unpackVisibilityBuffer(g_VisibilityBuffer.Load(int3(params.screenPosition.xy, 0)));

    float2 uv = params.screenPosition.xy * g_DrawConstants.invScreenSize;
    float2 ndc = 2.0f * float2(uv.x, 1.0f - uv.y) - 1.0f;

    // Translate point from near plane (we have inversed depth) to the world space.
    float4 nearPlaneWorldPosition = mul(g_DrawConstants.invViewProjection, float4(ndc, 1.0f, 1.0f));
    nearPlaneWorldPosition.xyz /= nearPlaneWorldPosition.w;
    const float3 cameraViewDirection = normalize(nearPlaneWorldPosition.xyz - g_DrawConstants.cameraPosition.xyz);

    Meshlet meshlet = g_MeshletBuffer[visibilityBufferData.meshletId];
    Instance instance = g_InstanceBuffer[visibilityBufferData.instanceId];
    float3x4 transform = g_TransformBuffer[instance.transform_index];

    uint indicesOffset = meshlet.data_offset_idx + meshlet.vertex_count + visibilityBufferData.primitiveId;
    uint packedIndices = g_MeshletPackBuffer[indicesOffset];
    uint3 unpackedIndices = uint3(packedIndices & 0x3F, (packedIndices >> 10) & 0x3F, packedIndices >> 20);
    uint instanceVertexOffset = instance.vertex_offset_idx[g_VertexDataIndex];
    uint vertexOffset0 = instanceVertexOffset + g_MeshletPackBuffer[meshlet.data_offset_idx + unpackedIndices.x];
    ShadedVertex vertex0 = shadeVertex(g_VertexBuffer[vertexOffset0], transform);
    uint vertexOffset1 = instanceVertexOffset + g_MeshletPackBuffer[meshlet.data_offset_idx + unpackedIndices.y];
    ShadedVertex vertex1 = shadeVertex(g_VertexBuffer[vertexOffset1], transform);
    uint vertexOffset2 = instanceVertexOffset + g_MeshletPackBuffer[meshlet.data_offset_idx + unpackedIndices.z];
    ShadedVertex vertex2 = shadeVertex(g_VertexBuffer[vertexOffset2], transform);

    float3 barycentrics = computeBarycentrics(g_DrawConstants.cameraPosition.xyz, cameraViewDirection,
        vertex0.worldPosition, vertex1.worldPosition, vertex2.worldPosition);
    ShadedVertex interpolants = computeInterpolants(barycentrics, vertex0, vertex1, vertex2);
    // TODO something is wrong with interpolants. Debug.

    Material material = g_MaterialBuffer[instance.material_index];
    MaterialEvaluated materialEvaluated = MakeMaterialEvaluated(material, interpolants.uv);
    MaterialBRDF materialBrdf = MakeMaterialBRDF(materialEvaluated);

    float3 tangent, bitangent;
    calculateTangentBitangent(vertex0, vertex1, vertex2, tangent, bitangent);
    float3 normal = calculateNormal(interpolants, tangent, bitangent, material);

    const float3 cameraPosition = g_DrawConstants.cameraPosition.xyz;
    const float3 viewDirection = normalize(cameraPosition - interpolants.worldPosition);

    float3 radiance = calculateDirectLighting(materialBrdf, normal, viewDirection, cameraPosition, interpolants.worldPosition) +
        calculateIndirectLighting(materialBrdf, normal, viewDirection);    

    float3 color = tonemap(radiance);
    color = applyInverseGamma(color);

    pixel.color = float4(color, 1.0f);

    return pixel;
}
