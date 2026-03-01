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
    textures.push_back({"Color", SharedTexture::Access::Write});
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

    const auto& gpuDrawConstants = capsaicin.allocateConstantBuffer<Constants>(1);
    {
        Constants drawConstants = {};
        drawConstants.view      = capsaicin.getCameraMatrices().view;

        gfxBufferGetData<Constants>(gfx_, gpuDrawConstants)[0] = drawConstants;
    }

    const auto     scene        = capsaicin.getScene();
    const uint32_t numInstances = gfxSceneGetInstanceCount(scene);
    const auto*    instances    = gfxSceneGetInstances(scene);

    vertexCacheOffset.resize(numInstances);

    gfxProgramSetParameter(gfx_, m_shadeVerticesProgram, "g_Constants", gpuDrawConstants);
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

        vertexCacheOffset[instanceId] = currentVertexCacheOffset;
        currentVertexCacheOffset += meshInfo.vertex_count;
    }

    gfxDestroyBuffer(gfx_, gpuDrawConstants);
}

void CtRayTracer::terminate() noexcept
{
    // TODO remove members
}

void CtRayTracer::renderGUI([[maybe_unused]] CapsaicinInternal& capsaicin) const noexcept {}
} // namespace Capsaicin
