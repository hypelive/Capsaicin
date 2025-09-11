#include "fxaa.h"
#include "capsaicin_internal.h"
#include "fxaa_shared.h"
#include "components/custom_light_builder/custom_light_builder.h"
#include "components/probe_baker/probe_baker.h"

namespace Capsaicin
{
FXAA::FXAA()
    : RenderTechnique("FXAA") {}

FXAA::~FXAA()
{
    FXAA::terminate();
}

RenderOptionList FXAA::getRenderOptions() noexcept
{
    RenderOptionList newOptions;
    return newOptions;
}

FXAA::RenderOptions FXAA::convertOptions(
    [[maybe_unused]] RenderOptionList const &options) noexcept
{
    RenderOptions newOptions;
    return newOptions;
}

ComponentList FXAA::getComponents() const noexcept
{
    ComponentList components;
    return components;
}

SharedBufferList FXAA::getSharedBuffers() const noexcept
{
    SharedBufferList buffers;
    return buffers;
}

SharedTextureList FXAA::getSharedTextures() const noexcept
{
    SharedTextureList textures;
    return textures;
}

DebugViewList FXAA::getDebugViews() const noexcept
{
    DebugViewList views;
    return views;
}

bool FXAA::init([[maybe_unused]] CapsaicinInternal const &capsaicin) noexcept
{
    m_program = capsaicin.createProgram("render_techniques/fxaa/fxaa");
    m_kernel  = gfxCreateComputeKernel(gfx_, m_program);

    return m_kernel;
}

void FXAA::render([[maybe_unused]] CapsaicinInternal &capsaicin) noexcept
{
    m_options                = convertOptions(capsaicin.getOptions());
    const auto &depthTexture = capsaicin.getSharedTexture("DepthCopy");

    const glm::vec2 renderResolution = {depthTexture.getWidth(), depthTexture.getHeight()};
    auto const &    gpuDrawConstants = capsaicin.allocateConstantBuffer<FXAAConstants>(1);
    {
        FXAAConstants drawConstants     = {};
        drawConstants.viewProjection    = capsaicin.getCameraMatrices().view_projection;
        drawConstants.invViewProjection = capsaicin.getCameraMatrices().inv_view_projection;
        drawConstants.cameraPosition    = capsaicin.getCamera().eye;
        drawConstants.screenSize        = glm::vec4{renderResolution.x, renderResolution.y,
                                             1.0f / renderResolution.x,
                                             1.0f / renderResolution.y};

        gfxBufferGetData<FXAAConstants>(gfx_, gpuDrawConstants)[0] = drawConstants;
    }

    // Set the root parameters for Computing FXAA.
    { }

    // Compute FXAA.
    {
        gfxCommandBindKernel(gfx_, m_kernel);

        glm::uvec2 const groupCount =
            ceil(renderResolution / static_cast<float>(TILE_SIZE));
        gfxCommandDispatch(gfx_, groupCount.x, groupCount.y, 1);
    }

    gfxDestroyBuffer(gfx_, gpuDrawConstants);
}

void FXAA::terminate() noexcept
{
    gfxDestroyKernel(gfx_, m_kernel);
    m_kernel = {};
    gfxDestroyProgram(gfx_, m_program);
    m_program = {};
}

void FXAA::renderGUI([[maybe_unused]] CapsaicinInternal &capsaicin) const noexcept { }
} // namespace Capsaicin
