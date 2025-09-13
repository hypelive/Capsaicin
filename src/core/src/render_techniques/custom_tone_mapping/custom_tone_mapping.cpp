#include "custom_tone_mapping.h"
#include "capsaicin_internal.h"
#include "custom_tone_mapping_shared.h"
#include "components/custom_light_builder/custom_light_builder.h"
#include "components/probe_baker/probe_baker.h"

static constexpr std::string_view SOURCE_TEXTURE_NAME = "HDRColor";
static constexpr std::string_view TARGET_TEXTURE_NAME = "LDRColor";

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
    textures.push_back({SOURCE_TEXTURE_NAME, SharedTexture::Access::Read});
    textures.push_back({TARGET_TEXTURE_NAME, SharedTexture::Access::Write, SharedTexture::Flags::None,
                        DXGI_FORMAT_R8G8B8A8_UNORM});
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
    m_options                 = convertOptions(capsaicin.getOptions());
    auto const &targetTexture = capsaicin.getSharedTexture(TARGET_TEXTURE_NAME);

    const glm::vec2 renderResolution = {targetTexture.getWidth(), targetTexture.getHeight()};
    auto const &    gpuDrawConstants = capsaicin.allocateConstantBuffer<ToneMapConstants>(1);
    {
        ToneMapConstants drawConstants = {};
        drawConstants.screenSize       = glm::vec4{renderResolution.x, renderResolution.y,
                                                   1.0f / renderResolution.x,
                                                   1.0f / renderResolution.y};

        gfxBufferGetData<ToneMapConstants>(gfx_, gpuDrawConstants)[0] = drawConstants;
    }

    // Set the root parameters for Computing CustomToneMapping.
    {
        gfxProgramSetParameter(gfx_, m_program, "g_Constants", gpuDrawConstants);
        gfxProgramSetParameter(gfx_, m_program, "g_SourceTexture",
            capsaicin.getSharedTexture(SOURCE_TEXTURE_NAME));
        gfxProgramSetParameter(gfx_, m_program, "g_TargetTexture", targetTexture);
    }

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
