#include "custom_taa.h"
#include "capsaicin_internal.h"
#include "custom_taa_shared.h"

static constexpr std::string_view SOURCE_TEXTURE_NAME = "LDRColor";
static constexpr std::string_view TARGET_TEXTURE_NAME = "Color";

namespace Capsaicin
{
CustomTAA::CustomTAA()
    : RenderTechnique("CustomTAA") {}

CustomTAA::~CustomTAA()
{
    CustomTAA::terminate();
}

RenderOptionList CustomTAA::getRenderOptions() noexcept
{
    RenderOptionList newOptions;
    return newOptions;
}

CustomTAA::RenderOptions CustomTAA::convertOptions(
    [[maybe_unused]] RenderOptionList const &options) noexcept
{
    RenderOptions newOptions;
    return newOptions;
}

ComponentList CustomTAA::getComponents() const noexcept
{
    ComponentList components;
    return components;
}

SharedBufferList CustomTAA::getSharedBuffers() const noexcept
{
    SharedBufferList buffers;
    return buffers;
}

SharedTextureList CustomTAA::getSharedTextures() const noexcept
{
    SharedTextureList textures;
    textures.push_back({SOURCE_TEXTURE_NAME, SharedTexture::Access::Read});
    textures.push_back({TARGET_TEXTURE_NAME, SharedTexture::Access::Write});
    return textures;
}

DebugViewList CustomTAA::getDebugViews() const noexcept
{
    DebugViewList views;
    return views;
}

bool CustomTAA::init([[maybe_unused]] CapsaicinInternal const &capsaicin) noexcept
{
    m_program = capsaicin.createProgram("render_techniques/CustomTAA/custom_taa");
    m_kernel  = gfxCreateComputeKernel(gfx_, m_program);

    return m_kernel;
}

void CustomTAA::render([[maybe_unused]] CapsaicinInternal &capsaicin) noexcept
{
    m_options                = convertOptions(capsaicin.getOptions());
    auto const &targetTexture = capsaicin.getSharedTexture(TARGET_TEXTURE_NAME);

    const glm::vec2 renderResolution = {targetTexture.getWidth(), targetTexture.getHeight()};
    auto const &    gpuDrawConstants = capsaicin.allocateConstantBuffer<TAAConstants>(1);
    {
        TAAConstants drawConstants     = {};
        drawConstants.screenSize        = glm::vec4{renderResolution.x, renderResolution.y,
                                             1.0f / renderResolution.x,
                                             1.0f / renderResolution.y};

        gfxBufferGetData<TAAConstants>(gfx_, gpuDrawConstants)[0] = drawConstants;
    }

    // Set the root parameters for Computing CustomTAA.
    {
        gfxProgramSetParameter(gfx_, m_program, "g_Constants", gpuDrawConstants);
        gfxProgramSetParameter(gfx_, m_program, "g_SourceTexture", capsaicin.getSharedTexture(SOURCE_TEXTURE_NAME));
        gfxProgramSetParameter(gfx_, m_program, "g_TargetTexture", targetTexture);
    }

    // Compute CustomTAA.
    {
        gfxCommandBindKernel(gfx_, m_kernel);

        glm::uvec2 const groupCount =
            ceil(renderResolution / static_cast<float>(TILE_SIZE));
        gfxCommandDispatch(gfx_, groupCount.x, groupCount.y, 1);
    }

    gfxDestroyBuffer(gfx_, gpuDrawConstants);
}

void CustomTAA::terminate() noexcept
{
    gfxDestroyKernel(gfx_, m_kernel);
    m_kernel = {};
    gfxDestroyProgram(gfx_, m_program);
    m_program = {};
}

void CustomTAA::renderGUI([[maybe_unused]] CapsaicinInternal &capsaicin) const noexcept { }
} // namespace Capsaicin
