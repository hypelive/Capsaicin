#include "ct_ray_tracer.h"

#include "capsaicin_internal.h"
#include "shared.h"

#include <gpu_shared.h>
#include <string_view>

namespace
{
constexpr std::string_view RT_PROGRAM_NAME             = "render_techniques/ct_ray_tracer/ct_ray_tracer";
constexpr std::string_view SHADE_VERTICES_PROGRAM_NAME = "render_techniques/ct_ray_tracer/shade_vertices";
} // namespace

namespace Capsaicin
{
CtRayTracer::CtRayTracer()
    : RenderTechnique("Ray Tracer")
{}

CtRayTracer::~CtRayTracer()
{
    CtRayTracer::terminate();
}

RenderOptionList CtRayTracer::getRenderOptions() noexcept
{
    RenderOptionList newOptions;
    return newOptions;
}

CtRayTracer::RenderOptions CtRayTracer::convertOptions(
    [[maybe_unused]] const RenderOptionList& options) noexcept
{
    RenderOptions newOptions;
    return newOptions;
}

ComponentList CtRayTracer::getComponents() const noexcept
{
    ComponentList components;
    return components;
}

SharedTextureList CtRayTracer::getSharedTextures() const noexcept
{
    SharedTextureList textures;
    textures.push_back({"Color", SharedTexture::Access::ReadWrite});
    return textures;
}

bool CtRayTracer::init([[maybe_unused]] const CapsaicinInternal& capsaicin) noexcept
{
    m_shadeVerticesProgram = capsaicin.createProgram(SHADE_VERTICES_PROGRAM_NAME.data());
    m_shadeVerticesKernel  = gfxCreateComputeKernel(gfx_, m_shadeVerticesProgram);

    m_rtProgram = capsaicin.createProgram(RT_PROGRAM_NAME.data());
    m_rtKernel  = gfxCreateComputeKernel(gfx_, m_rtProgram);

    return m_shadeVerticesKernel && m_rtKernel;
}

void CtRayTracer::render(CapsaicinInternal& capsaicin) noexcept
{
    [[maybe_unused]] const RenderOptions newOptions = convertOptions(capsaicin.getOptions());

    if (!m_vertexCache)
    {
        // Get a buffer for the shaded vertices.
        m_vertexCache = gfxCreateBuffer<Vertex>(gfx_, capsaicin.getVertexBuffer().getCount());
        m_vertexCache.setName("Vertex Cache");
    }

    // Per instance offset to the vertex cache data.
    // It's needed for triangles traversing in the RT shader.
    std::vector<uint32_t> vertexCacheOffset;
    uint32_t              currentVertexCacheOffset = 0u;

    // Vertices shading.
    {
        const auto& gpuVertexShadingConstants = capsaicin.allocateConstantBuffer<VertexShadingConstants>(1);
        {
            VertexShadingConstants drawConstants = {};
            drawConstants.view                   = capsaicin.getCameraMatrices().view;

            gfxBufferGetData<VertexShadingConstants>(gfx_, gpuVertexShadingConstants)[0] = drawConstants;
        }

        const auto     scene        = capsaicin.getScene();
        const uint32_t numInstances = gfxSceneGetInstanceCount(scene);
        const auto*    instances    = gfxSceneGetInstances(scene);

        // Properly resize the cache offsets.
        vertexCacheOffset.resize(numInstances);

        gfxProgramSetParameter(gfx_, m_shadeVerticesProgram, "g_Constants", gpuVertexShadingConstants);
        gfxProgramSetParameter(gfx_, m_shadeVerticesProgram, "g_VertexCache", m_vertexCache);
        gfxProgramSetParameter(gfx_, m_shadeVerticesProgram, "g_InputVertices", capsaicin.getVertexBuffer());
        gfxProgramSetParameter(gfx_, m_shadeVerticesProgram, "g_TransformBuffer", capsaicin.getTransformBuffer());
        gfxProgramSetParameter(gfx_, m_shadeVerticesProgram, "g_InstanceBuffer", capsaicin.getInstanceBuffer());
        gfxProgramSetParameter(gfx_, m_shadeVerticesProgram, "g_VertexDataIndex", capsaicin.getVertexDataIndex());
        gfxCommandBindKernel(gfx_, m_shadeVerticesKernel);
        const uint32_t* groupSize = gfxKernelGetNumThreads(gfx_, m_shadeVerticesKernel);

        for (uint32_t instanceId = 0u; instanceId < numInstances; ++instanceId)
        {
            const auto instance = instances[instanceId];
            const auto meshInfo = capsaicin.getMeshInfo(static_cast<uint32_t>(instance.mesh));

            InstanceData instanceData = {};
            instanceData.instanceId   = instanceId;
            instanceData.vertexOffset = currentVertexCacheOffset;
            instanceData.numVertices  = meshInfo.vertex_count;
            // TODO I can allocate one big buffer and then just set instance id on each iteration.
            // will it be more performant?
            const auto& gpuInstanceData = capsaicin.allocateConstantBuffer<InstanceData>(1);
            gfxBufferGetData<InstanceData>(gfx_, gpuInstanceData)[0] = instanceData;

            gfxProgramSetParameter(gfx_, m_shadeVerticesProgram, "g_InstanceData", gpuInstanceData);

            const uint32_t groupCount = (meshInfo.vertex_count + groupSize[0] - 1u) / groupSize[0];
            gfxCommandDispatch(gfx_, groupCount, 1, 1);
            gfxDestroyBuffer(gfx_, gpuInstanceData);

            // Track these numbers here to pass it to the RT pass.
            vertexCacheOffset[instanceId] = currentVertexCacheOffset;
            currentVertexCacheOffset += meshInfo.vertex_count;
        }
        gfxDestroyBuffer(gfx_, gpuVertexShadingConstants);
    }

    // RT pass.
    {
        const auto& colorTexture   = capsaicin.getSharedTexture("Color");
        const auto& gpuRtConstants = capsaicin.allocateConstantBuffer<RtConstants>(1);
        {
            RtConstants drawConstants   = {};
            drawConstants.numInstances  = currentVertexCacheOffset;
            drawConstants.resolution    = {colorTexture.getWidth(), colorTexture.getHeight()};
            drawConstants.invResolution = 1.0f / static_cast<glm::vec2>(drawConstants.resolution);

            gfxBufferGetData<RtConstants>(gfx_, gpuRtConstants)[0] = drawConstants;
        }

        const auto& gpuVertexCacheOffset = gfxCreateBuffer<uint32_t>(
            gfx_, static_cast<uint32_t>(vertexCacheOffset.size()), vertexCacheOffset.data());

        {
            gfxProgramSetParameter(gfx_, m_rtProgram, "g_Constants", gpuRtConstants);
            gfxProgramSetParameter(gfx_, m_rtProgram, "g_VertexCache", m_vertexCache);
            gfxProgramSetParameter(gfx_, m_rtProgram, "g_InstanceBuffer", capsaicin.getInstanceBuffer());
            gfxProgramSetParameter(gfx_, m_rtProgram, "g_IndexBuffer", capsaicin.getIndexBuffer());
            gfxProgramSetParameter(gfx_, m_rtProgram, "g_VertexOffsetBuffer", gpuVertexCacheOffset);
            gfxProgramSetParameter(gfx_, m_rtProgram, "g_Output", capsaicin.getSharedTexture("Color"));

            gfxCommandBindKernel(gfx_, m_rtKernel);
            const uint32_t*  groupSize  = gfxKernelGetNumThreads(gfx_, m_rtKernel);
            const glm::uvec2 groupCount = {(colorTexture.getWidth() + groupSize[0] - 1u) / groupSize[0],
                (colorTexture.getHeight() + groupSize[1] - 1u) / groupSize[1]};
            gfxCommandDispatch(gfx_, groupCount.x, groupCount.y, 1u);
        }

        gfxDestroyBuffer(gfx_, gpuVertexCacheOffset);
        gfxDestroyBuffer(gfx_, gpuRtConstants);
    }
}

void CtRayTracer::terminate() noexcept
{
    // TODO remove members
    gfxDestroyBuffer(gfx_, m_vertexCache);
    m_vertexCache = {};
}

void CtRayTracer::renderGUI([[maybe_unused]] CapsaicinInternal& capsaicin) const noexcept {}
} // namespace Capsaicin
