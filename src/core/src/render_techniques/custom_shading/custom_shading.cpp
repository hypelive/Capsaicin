#include "custom_shading.h"
#include "capsaicin_internal.h"
#include "custom_shading_shared.h"
#include "components/custom_light_builder/custom_light_builder.h"
#include "components/probe_baker/probe_baker.h"

namespace Capsaicin
{
CustomShading::CustomShading()
    : RenderTechnique("Custom Shading") {}

CustomShading::~CustomShading()
{
    CustomShading::terminate();
}

RenderOptionList CustomShading::getRenderOptions() noexcept
{
    RenderOptionList newOptions;
    return newOptions;
}

CustomShading::RenderOptions CustomShading::convertOptions(
    [[maybe_unused]] RenderOptionList const &options) noexcept
{
    RenderOptions newOptions;
    return newOptions;
}

ComponentList CustomShading::getComponents() const noexcept
{
    ComponentList components;
    components.emplace_back("ProbeBaker");
    components.emplace_back("CustomLightBuilder");
    return components;
}

SharedBufferList CustomShading::getSharedBuffers() const noexcept
{
    SharedBufferList buffers;
    return buffers;
}

SharedTextureList CustomShading::getSharedTextures() const noexcept
{
    SharedTextureList textures;
    textures.push_back({"VisibilityBuffer", SharedTexture::Access::Read, SharedTexture::Flags::None,
                        DXGI_FORMAT_R32_UINT});
    textures.push_back({"Depth", SharedTexture::Access::Read, SharedTexture::Flags::None});
    return textures;
}

DebugViewList CustomShading::getDebugViews() const noexcept
{
    DebugViewList views;
    return views;
}

bool CustomShading::init([[maybe_unused]] CapsaicinInternal const &capsaicin) noexcept
{
    m_shadingProgram = capsaicin.createProgram(
        "render_techniques/custom_shading/custom_shading");

    GfxDrawState const shadingDrawState = {};
    gfxDrawStateSetCullMode(shadingDrawState, D3D12_CULL_MODE_BACK);
    gfxDrawStateSetDepthFunction(shadingDrawState, D3D12_COMPARISON_FUNC_LESS);
    gfxDrawStateSetDepthWriteMask(shadingDrawState, D3D12_DEPTH_WRITE_MASK_ZERO);

    gfxDrawStateSetColorTarget(shadingDrawState, 0, capsaicin.getSharedTexture("Color").getFormat());
    gfxDrawStateSetDepthStencilTarget(shadingDrawState, capsaicin.getSharedTexture("Depth").getFormat());

    m_shadingKernel =
        gfxCreateGraphicsKernel(gfx_, m_shadingProgram, shadingDrawState);

    return m_shadingKernel;
}

void CustomShading::render([[maybe_unused]] CapsaicinInternal &capsaicin) noexcept
{
    auto const &colorTexture = capsaicin.getSharedTexture("Color");

    auto const &gpuDrawConstants = capsaicin.allocateConstantBuffer<ShadingConstants>(1);
    {
        ShadingConstants drawConstants = {};
        drawConstants.viewProjection   = capsaicin.getCameraMatrices().view_projection;
        drawConstants.invViewProjection = capsaicin.getCameraMatrices().inv_view_projection;
        drawConstants.cameraPosition   = capsaicin.getCamera().eye;
        drawConstants.invScreenSize    = 1.0f / float2{colorTexture.getWidth(), colorTexture.getHeight()};

        gfxBufferGetData<ShadingConstants>(gfx_, gpuDrawConstants)[0] = drawConstants;
    }

    // Set the root parameters.
    {
        gfxProgramSetParameter(gfx_, m_shadingProgram, "g_DrawConstants", gpuDrawConstants);

        gfxProgramSetParameter(gfx_, m_shadingProgram, "g_InstanceBuffer",
            capsaicin.getInstanceBuffer());
        gfxProgramSetParameter(gfx_, m_shadingProgram, "g_TransformBuffer",
            capsaicin.getTransformBuffer());
        gfxProgramSetParameter(gfx_, m_shadingProgram, "g_MeshletBuffer",
            capsaicin.getSharedBuffer("Meshlets"));
        gfxProgramSetParameter(gfx_, m_shadingProgram, "g_MeshletPackBuffer",
            capsaicin.getSharedBuffer("MeshletPack"));
        gfxProgramSetParameter(gfx_, m_shadingProgram, "g_VertexBuffer",
            capsaicin.getVertexBuffer());
        gfxProgramSetParameter(gfx_, m_shadingProgram, "g_VertexDataIndex",
            capsaicin.getVertexDataIndex());
        gfxProgramSetParameter(gfx_, m_shadingProgram, "g_MaterialBuffer",
            capsaicin.getMaterialBuffer());
        gfxProgramSetParameter(gfx_, m_shadingProgram, "g_VisibilityBuffer",
            capsaicin.getSharedTexture("VisibilityBuffer"));

        auto const &textures = capsaicin.getTextures();
        gfxProgramSetParameter(gfx_, m_shadingProgram, "g_TextureMaps", textures.data(),
            static_cast<uint32_t>(textures.size()));
        gfxProgramSetParameter(gfx_, m_shadingProgram, "g_TextureSampler",
            capsaicin.getLinearWrapSampler());
        gfxProgramSetParameter(gfx_, m_shadingProgram, "g_LinearSampler",
            capsaicin.getLinearSampler());

        capsaicin.getComponent<ProbeBaker>()->addProgramParameters(capsaicin, m_shadingProgram);
        capsaicin.getComponent<CustomLightBuilder>()->addProgramParameters(capsaicin, m_shadingProgram);
    }

    // Run the amplification shader.
    {
        gfxCommandBindColorTarget(gfx_, 0, colorTexture);
        gfxCommandBindDepthStencilTarget(gfx_, capsaicin.getSharedTexture("Depth"));
        gfxCommandBindKernel(gfx_, m_shadingKernel);

        gfxCommandDraw(gfx_, 3);
    }

    gfxDestroyBuffer(gfx_, gpuDrawConstants);
}

void CustomShading::terminate() noexcept
{
    gfxDestroyKernel(gfx_, m_shadingKernel);
    m_shadingKernel = {};
    gfxDestroyProgram(gfx_, m_shadingProgram);
    m_shadingProgram = {};
}

void CustomShading::renderGUI([[maybe_unused]] CapsaicinInternal &capsaicin) const noexcept { }
} // namespace Capsaicin
