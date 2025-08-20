#include "custom_skybox.h"
#include "capsaicin_internal.h"
#include "custom_skybox_shared.h"

namespace Capsaicin
{
CustomSkybox::CustomSkybox()
    : RenderTechnique("Custom Skybox") {}

CustomSkybox::~CustomSkybox()
{
    CustomSkybox::terminate();
}

RenderOptionList CustomSkybox::getRenderOptions() noexcept
{
    RenderOptionList newOptions;
    return newOptions;
}

CustomSkybox::RenderOptions CustomSkybox::convertOptions(
    [[maybe_unused]] RenderOptionList const &options) noexcept
{
    RenderOptions newOptions;
    return newOptions;
}

ComponentList CustomSkybox::getComponents() const noexcept
{
    ComponentList components;
    return components;
}

SharedBufferList CustomSkybox::getSharedBuffers() const noexcept
{
    SharedBufferList buffers;
    return buffers;
}

SharedTextureList CustomSkybox::getSharedTextures() const noexcept
{
    SharedTextureList textures;
    textures.push_back({"Color", SharedTexture::Access::Write});
    textures.push_back({"Depth", SharedTexture::Access::ReadWrite});
    return textures;
}

DebugViewList CustomSkybox::getDebugViews() const noexcept
{
    DebugViewList views;
    return views;
}

bool CustomSkybox::init([[maybe_unused]] CapsaicinInternal const &capsaicin) noexcept
{
    m_skyboxProgram = capsaicin.createProgram("render_techniques/custom_skybox/custom_skybox");

    GfxDrawState const skyboxDrawState = {};
    gfxDrawStateSetCullMode(skyboxDrawState, D3D12_CULL_MODE_BACK);
    gfxDrawStateSetDepthFunction(skyboxDrawState, D3D12_COMPARISON_FUNC_GREATER);

    gfxDrawStateSetColorTarget(
        skyboxDrawState, 0, capsaicin.getSharedTexture("Color").getFormat());
    gfxDrawStateSetDepthStencilTarget(
        skyboxDrawState, capsaicin.getSharedTexture("Depth").getFormat());

    m_skyboxKernel = gfxCreateGraphicsKernel(gfx_, m_skyboxProgram, skyboxDrawState);

    return m_skyboxKernel;
}

void CustomSkybox::render([[maybe_unused]] CapsaicinInternal &capsaicin) noexcept
{
    auto const &colorTexture = capsaicin.getSharedTexture("Color");

    GfxBuffer const constantBuffer = capsaicin.allocateConstantBuffer<SkyboxConstants>(1);
    // Filling the draw constants.
    {
        SkyboxConstants drawConstants     = {};
        drawConstants.invViewProjection = capsaicin.getCameraMatrices().inv_view_projection;
        drawConstants.cameraPosition    = capsaicin.getCamera().eye;
        drawConstants.invScreenSize     = 1.0f / float2{colorTexture.getWidth(), colorTexture.getHeight()};

        gfxBufferGetData<SkyboxConstants>(gfx_, constantBuffer)[0] = drawConstants;
    }

    // Set the root parameters.
    {
        gfxProgramSetParameter(gfx_, m_skyboxProgram, "g_DrawConstants", constantBuffer);
        gfxProgramSetParameter(gfx_, m_skyboxProgram, "g_EnvironmentMap", capsaicin.getEnvironmentBuffer());

        gfxProgramSetParameter(gfx_, m_skyboxProgram, "g_LinearSampler", capsaicin.getLinearSampler());

        gfxCommandBindDepthStencilTarget(gfx_, capsaicin.getSharedTexture("Depth"));
        gfxCommandBindColorTarget(gfx_, 0, colorTexture);

        gfxCommandBindKernel(gfx_, m_skyboxKernel);
        gfxCommandDraw(gfx_, 3);
    }
}

void CustomSkybox::terminate() noexcept
{
    gfxDestroyBuffer(gfx_, m_drawConstantsBuffer);
    gfxDestroyKernel(gfx_, m_skyboxKernel);
    gfxDestroyProgram(gfx_, m_skyboxProgram);
}

void CustomSkybox::renderGUI([[maybe_unused]] CapsaicinInternal &capsaicin) const noexcept { }
} // namespace Capsaicin
