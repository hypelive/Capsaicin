#include "custom_visibility_buffer.h"
#include "capsaicin_internal.h"
#include "custom_visibility_buffer_shared.h"
#include "components/custom_light_builder/custom_light_builder.h"
#include "components/probe_baker/probe_baker.h"

namespace Capsaicin
{
CustomVisibilityBuffer::CustomVisibilityBuffer()
    : RenderTechnique("Custom Visibility buffer") {}

CustomVisibilityBuffer::~CustomVisibilityBuffer()
{
    CustomVisibilityBuffer::terminate();
}

RenderOptionList CustomVisibilityBuffer::getRenderOptions() noexcept
{
    RenderOptionList newOptions;
    return newOptions;
}

CustomVisibilityBuffer::RenderOptions CustomVisibilityBuffer::convertOptions(
    [[maybe_unused]] RenderOptionList const &options) noexcept
{
    RenderOptions newOptions;
    return newOptions;
}

ComponentList CustomVisibilityBuffer::getComponents() const noexcept
{
    ComponentList components;
    return components;
}

SharedBufferList CustomVisibilityBuffer::getSharedBuffers() const noexcept
{
    SharedBufferList buffers;
    buffers.push_back({"Meshlets", SharedBuffer::Access::Read});
    buffers.push_back({"MeshletPack", SharedBuffer::Access::Read});
    return buffers;
}

SharedTextureList CustomVisibilityBuffer::getSharedTextures() const noexcept
{
    SharedTextureList textures;
    textures.push_back({"VisibilityBuffer", SharedTexture::Access::Write, SharedTexture::Flags::Clear,
                        DXGI_FORMAT_R32G32_UINT});
    textures.push_back({"Depth", SharedTexture::Access::ReadWrite, SharedTexture::Flags::Clear});
    return textures;
}

DebugViewList CustomVisibilityBuffer::getDebugViews() const noexcept
{
    DebugViewList views;
    return views;
}

bool CustomVisibilityBuffer::init([[maybe_unused]] CapsaicinInternal const &capsaicin) noexcept
{
    m_visibilityBufferProgram = capsaicin.createProgram(
        "render_techniques/custom_visibility_buffer/custom_visibility_buffer");

    GfxDrawState const visibilityBufferDrawState = {};
    gfxDrawStateSetCullMode(visibilityBufferDrawState, D3D12_CULL_MODE_BACK);
    gfxDrawStateSetDepthFunction(visibilityBufferDrawState, D3D12_COMPARISON_FUNC_GREATER);

    gfxDrawStateSetColorTarget(
        visibilityBufferDrawState, 0, capsaicin.getSharedTexture("VisibilityBuffer").getFormat());
    gfxDrawStateSetDepthStencilTarget(
        visibilityBufferDrawState, capsaicin.getSharedTexture("Depth").getFormat());

    m_visibilityBufferKernel =
        gfxCreateMeshKernel(gfx_, m_visibilityBufferProgram, visibilityBufferDrawState);

    return m_visibilityBufferProgram;
}

void CustomVisibilityBuffer::render([[maybe_unused]] CapsaicinInternal &capsaicin) noexcept
{
    // Prepare draw data.
    {
        std::vector<DrawData> drawData;
#if 1
        for (auto const &index : capsaicin.getInstanceIdData())
        {
            Instance const &instance = capsaicin.getInstanceData()[index];

            for (uint32_t j = 0; j < instance.meshlet_count; ++j)
            {
                drawData.emplace_back(instance.meshlet_offset_idx + j, index);
            }
        }
#else // Draw the first instance only.
        uint32_t instanceId = capsaicin.getInstanceIdData()[0];
        Instance const &instance   = capsaicin.getInstanceData()[instanceId];
        for (uint32_t j = 0; j < instance.meshlet_count; ++j)
        {
            drawData.emplace_back(instance.meshlet_offset_idx + j, instanceId);
        }
#endif
        m_drawDataSize = static_cast<uint32_t>(drawData.size());
        gfxDestroyBuffer(gfx_, m_drawDataBuffer);
        // The data will be uploaded through the staging buffer (it seems), so we don't need to worry about requesting cpu access to the buffer.
        m_drawDataBuffer = gfxCreateBuffer<DrawData>(gfx_, m_drawDataSize, drawData.data());
    }

    // Filling the draw constants.
    {
        VisibilityBufferConstants drawConstants  = {};
        drawConstants.viewProjection = capsaicin.getCameraMatrices().view_projection;
        drawConstants.drawCount      = m_drawDataSize;

        gfxDestroyBuffer(gfx_, m_drawConstantsBuffer);
        m_drawConstantsBuffer = gfxCreateBuffer<VisibilityBufferConstants>(gfx_, 1, &drawConstants);
    }

    // Set the root parameters.
    {
        gfxProgramSetParameter(gfx_, m_visibilityBufferProgram, "g_DrawConstants", m_drawConstantsBuffer);
        gfxProgramSetParameter(gfx_, m_visibilityBufferProgram, "g_DrawDataBuffer", m_drawDataBuffer);

        gfxProgramSetParameter(gfx_, m_visibilityBufferProgram, "g_InstanceBuffer",
            capsaicin.getInstanceBuffer());
        gfxProgramSetParameter(gfx_, m_visibilityBufferProgram, "g_TransformBuffer",
            capsaicin.getTransformBuffer());
        gfxProgramSetParameter(gfx_, m_visibilityBufferProgram, "g_MeshletBuffer",
            capsaicin.getSharedBuffer("Meshlets"));
        gfxProgramSetParameter(gfx_, m_visibilityBufferProgram, "g_MeshletPackBuffer",
            capsaicin.getSharedBuffer("MeshletPack"));
        gfxProgramSetParameter(gfx_, m_visibilityBufferProgram, "g_VertexBuffer",
            capsaicin.getVertexBuffer());
        gfxProgramSetParameter(gfx_, m_visibilityBufferProgram, "g_VertexDataIndex",
            capsaicin.getVertexDataIndex());
        gfxProgramSetParameter(gfx_, m_visibilityBufferProgram, "g_MaterialBuffer",
            capsaicin.getMaterialBuffer());

        auto const &textures = capsaicin.getTextures();
        gfxProgramSetParameter(gfx_, m_visibilityBufferProgram, "g_TextureMaps", textures.data(),
            static_cast<uint32_t>(textures.size()));
        gfxProgramSetParameter(gfx_, m_visibilityBufferProgram, "g_LinearSampler",
            capsaicin.getLinearSampler());
    }

    // Run the amplification shader.
    {
        gfxCommandBindDepthStencilTarget(gfx_, capsaicin.getSharedTexture("Depth"));
        gfxCommandBindColorTarget(gfx_, 0, capsaicin.getSharedTexture("VisibilityBuffer"));
        gfxCommandBindKernel(gfx_, m_visibilityBufferKernel);

        uint32_t const *num_threads  = gfxKernelGetNumThreads(gfx_, m_visibilityBufferKernel);
        uint32_t const  num_groups_x = (m_drawDataSize + num_threads[0] - 1) / num_threads[0];

        gfxCommandDrawMesh(gfx_, num_groups_x, 1, 1);
    }
}

void CustomVisibilityBuffer::terminate() noexcept
{
    gfxDestroyKernel(gfx_, m_visibilityBufferKernel);
    m_visibilityBufferKernel = {};
    gfxDestroyProgram(gfx_, m_visibilityBufferProgram);
    m_visibilityBufferProgram = {};
    gfxDestroyBuffer(gfx_, m_drawConstantsBuffer);
    m_drawConstantsBuffer = {};
    gfxDestroyBuffer(gfx_, m_drawDataBuffer);
    m_drawDataBuffer = {};
}

void CustomVisibilityBuffer::renderGUI([[maybe_unused]] CapsaicinInternal &capsaicin) const noexcept { }
} // namespace Capsaicin
