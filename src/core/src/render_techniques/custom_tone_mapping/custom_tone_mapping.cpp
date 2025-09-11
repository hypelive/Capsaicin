#include "custom_tone_mapping.h"
#include "capsaicin_internal.h"
#include "custom_tone_mapping_shared.h"
#include "components/custom_light_builder/custom_light_builder.h"
#include "components/probe_baker/probe_baker.h"

namespace Capsaicin
{
CustomToneMapping::CustomToneMapping()
    : RenderTechnique("Custom Tone Mapping") {}

CustomToneMapping::~CustomToneMapping()
{
    CustomToneMapping::terminate();
}

RenderOptionList CustomToneMapping::getRenderOptions() noexcept
{
    RenderOptionList newOptions;
    return newOptions;
}

CustomToneMapping::RenderOptions CustomToneMapping::convertOptions(
    [[maybe_unused]] RenderOptionList const &options) noexcept
{
    RenderOptions newOptions;
    return newOptions;
}

ComponentList CustomToneMapping::getComponents() const noexcept
{
    ComponentList components;
    return components;
}

SharedBufferList CustomToneMapping::getSharedBuffers() const noexcept
{
    SharedBufferList buffers;
    return buffers;
}

SharedTextureList CustomToneMapping::getSharedTextures() const noexcept
{
    SharedTextureList textures;
    return textures;
}

DebugViewList CustomToneMapping::getDebugViews() const noexcept
{
    DebugViewList views;
    return views;
}

bool CustomToneMapping::init([[maybe_unused]] CapsaicinInternal const &capsaicin) noexcept
{
    m_program = capsaicin.createProgram("render_techniques/custom_tone_mapping/custom_tone_mapping");
    m_kernel  = gfxCreateComputeKernel(gfx_, m_program);

    return m_kernel;
}

void CustomToneMapping::render([[maybe_unused]] CapsaicinInternal &capsaicin) noexcept
{
    m_options                = convertOptions(capsaicin.getOptions());
    const auto &depthTexture = capsaicin.getSharedTexture("DepthCopy");

    const glm::vec2 renderResolution = {depthTexture.getWidth(), depthTexture.getHeight()};
    auto const &    gpuDrawConstants = capsaicin.allocateConstantBuffer<ToneMapConstants>(1);
    {
        ToneMapConstants drawConstants  = {};
        drawConstants.viewProjection    = capsaicin.getCameraMatrices().view_projection;
        drawConstants.invViewProjection = capsaicin.getCameraMatrices().inv_view_projection;
        drawConstants.cameraPosition    = capsaicin.getCamera().eye;
        drawConstants.screenSize        = glm::vec4{renderResolution.x, renderResolution.y,
                                             1.0f / renderResolution.x,
                                             1.0f / renderResolution.y};

        gfxBufferGetData<ToneMapConstants>(gfx_, gpuDrawConstants)[0] = drawConstants;
    }

    // Set the root parameters for Computing CustomToneMapping.
    { }

    // Compute CustomToneMapping.
    {
        gfxCommandBindKernel(gfx_, m_kernel);

        glm::uvec2 const groupCount =
            ceil(renderResolution / static_cast<float>(TILE_SIZE));
        gfxCommandDispatch(gfx_, groupCount.x, groupCount.y, 1);
    }

    gfxDestroyBuffer(gfx_, gpuDrawConstants);
}

void CustomToneMapping::terminate() noexcept
{
    gfxDestroyKernel(gfx_, m_kernel);
    m_kernel = {};
    gfxDestroyProgram(gfx_, m_program);
    m_program = {};
}

void CustomToneMapping::renderGUI([[maybe_unused]] CapsaicinInternal &capsaicin) const noexcept { }
} // namespace Capsaicin
