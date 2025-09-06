#include "custom_visibility_to_gbuffer.h"
#include "capsaicin_internal.h"
#include "custom_visibility_to_gbuffer_shared.h"

namespace Capsaicin
{
CustomVisibilityToGBuffer::CustomVisibilityToGBuffer()
    : RenderTechnique("Custom Visibility To GBuffer") {}

CustomVisibilityToGBuffer::~CustomVisibilityToGBuffer()
{
    CustomVisibilityToGBuffer::terminate();
}

RenderOptionList CustomVisibilityToGBuffer::getRenderOptions() noexcept
{
    RenderOptionList newOptions;
    return newOptions;
}

CustomVisibilityToGBuffer::RenderOptions CustomVisibilityToGBuffer::convertOptions(
    [[maybe_unused]] RenderOptionList const &options) noexcept
{
    RenderOptions newOptions;
    return newOptions;
}

ComponentList CustomVisibilityToGBuffer::getComponents() const noexcept
{
    ComponentList components;
    return components;
}

SharedBufferList CustomVisibilityToGBuffer::getSharedBuffers() const noexcept
{
    SharedBufferList buffers;
    return buffers;
}

SharedTextureList CustomVisibilityToGBuffer::getSharedTextures() const noexcept
{
    SharedTextureList textures;
    textures.push_back({"VisibilityBuffer", SharedTexture::Access::Read, SharedTexture::Flags::None,
                        DXGI_FORMAT_R32G32_UINT});
    textures.push_back({"Depth", SharedTexture::Access::Read, SharedTexture::Flags::None});
    // Albedo, NormalZ
    textures.push_back({"GBuffer0", SharedTexture::Access::Write, SharedTexture::Flags::Clear,
                        DXGI_FORMAT_R8G8B8A8_UNORM});
    // NormalXY, roughness, metallic
    textures.push_back({"GBuffer1", SharedTexture::Access::Write, SharedTexture::Flags::Clear,
                        DXGI_FORMAT_R16G16B16A16_UNORM});
    // Emissive, ?
    textures.push_back({"GBuffer2", SharedTexture::Access::Write, SharedTexture::Flags::Clear,
                        DXGI_FORMAT_R16G16B16A16_FLOAT});
    return textures;
}

DebugViewList CustomVisibilityToGBuffer::getDebugViews() const noexcept
{
    DebugViewList views;
    return views;
}

bool CustomVisibilityToGBuffer::init([[maybe_unused]] CapsaicinInternal const &capsaicin) noexcept
{
    m_program = capsaicin.createProgram(
        "render_techniques/custom_visibility_to_gbuffer/custom_visibility_to_gbuffer");

    GfxDrawState const shadingDrawState = {};
    gfxDrawStateSetCullMode(shadingDrawState, D3D12_CULL_MODE_BACK);
    gfxDrawStateSetDepthFunction(shadingDrawState, D3D12_COMPARISON_FUNC_LESS);
    gfxDrawStateSetDepthWriteMask(shadingDrawState, D3D12_DEPTH_WRITE_MASK_ZERO);

    gfxDrawStateSetColorTarget(shadingDrawState, 0, capsaicin.getSharedTexture("GBuffer0").getFormat());
    gfxDrawStateSetColorTarget(shadingDrawState, 1, capsaicin.getSharedTexture("GBuffer1").getFormat());
    gfxDrawStateSetColorTarget(shadingDrawState, 2, capsaicin.getSharedTexture("GBuffer2").getFormat());
    gfxDrawStateSetDepthStencilTarget(shadingDrawState, capsaicin.getSharedTexture("Depth").getFormat());

    m_kernel = gfxCreateGraphicsKernel(gfx_, m_program, shadingDrawState);

    return m_kernel;
}

void CustomVisibilityToGBuffer::render([[maybe_unused]] CapsaicinInternal &capsaicin) noexcept
{
    auto const &gBuffer0 = capsaicin.getSharedTexture("GBuffer0");

    auto const &gpuDrawConstants = capsaicin.allocateConstantBuffer<VisibilityToGbufferConstants>(1);
    {
        VisibilityToGbufferConstants drawConstants = {};
        drawConstants.viewProjection               = capsaicin.getCameraMatrices().view_projection;
        drawConstants.invViewProjection            = capsaicin.getCameraMatrices().inv_view_projection;
        drawConstants.cameraPosition               = capsaicin.getCamera().eye;
        drawConstants.invScreenSize                = 1.0f / float2{gBuffer0.getWidth(), gBuffer0.getHeight()};

        gfxBufferGetData<VisibilityToGbufferConstants>(gfx_, gpuDrawConstants)[0] = drawConstants;
    }

    // Set the root parameters.
    {
        gfxProgramSetParameter(gfx_, m_program, "g_DrawConstants", gpuDrawConstants);

        gfxProgramSetParameter(gfx_, m_program, "g_InstanceBuffer",
            capsaicin.getInstanceBuffer());
        gfxProgramSetParameter(gfx_, m_program, "g_TransformBuffer",
            capsaicin.getTransformBuffer());
        gfxProgramSetParameter(gfx_, m_program, "g_MeshletBuffer",
            capsaicin.getSharedBuffer("Meshlets"));
        gfxProgramSetParameter(gfx_, m_program, "g_MeshletPackBuffer",
            capsaicin.getSharedBuffer("MeshletPack"));
        gfxProgramSetParameter(gfx_, m_program, "g_VertexBuffer",
            capsaicin.getVertexBuffer());
        gfxProgramSetParameter(gfx_, m_program, "g_VertexDataIndex",
            capsaicin.getVertexDataIndex());
        gfxProgramSetParameter(gfx_, m_program, "g_MaterialBuffer",
            capsaicin.getMaterialBuffer());
        gfxProgramSetParameter(gfx_, m_program, "g_VisibilityBuffer",
            capsaicin.getSharedTexture("VisibilityBuffer"));

        auto const &textures = capsaicin.getTextures();
        gfxProgramSetParameter(gfx_, m_program, "g_TextureMaps", textures.data(),
            static_cast<uint32_t>(textures.size()));
        gfxProgramSetParameter(gfx_, m_program, "g_TextureSampler",
            capsaicin.getLinearWrapSampler());
        gfxProgramSetParameter(gfx_, m_program, "g_LinearSampler",
            capsaicin.getLinearSampler());
    }

    {
        gfxCommandBindColorTarget(gfx_, 0, gBuffer0);
        gfxCommandBindColorTarget(gfx_, 1, capsaicin.getSharedTexture("GBuffer1"));
        gfxCommandBindColorTarget(gfx_, 2, capsaicin.getSharedTexture("GBuffer2"));
        gfxCommandBindDepthStencilTarget(gfx_, capsaicin.getSharedTexture("Depth"));
        gfxCommandBindKernel(gfx_, m_kernel);

        gfxCommandDraw(gfx_, 3);
    }

    gfxDestroyBuffer(gfx_, gpuDrawConstants);
}

void CustomVisibilityToGBuffer::terminate() noexcept
{
    gfxDestroyKernel(gfx_, m_kernel);
    m_kernel = {};
    gfxDestroyProgram(gfx_, m_program);
    m_program = {};
}

void CustomVisibilityToGBuffer::renderGUI([[maybe_unused]] CapsaicinInternal &capsaicin) const noexcept { }
} // namespace Capsaicin
