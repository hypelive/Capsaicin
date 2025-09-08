#include "ssao.h"
#include "capsaicin_internal.h"
#include "ssao_shared.h"
#include "components/custom_light_builder/custom_light_builder.h"
#include "components/probe_baker/probe_baker.h"

namespace Capsaicin
{
SSAO::SSAO()
    : RenderTechnique("SSAO") {}

SSAO::~SSAO()
{
    SSAO::terminate();
}

RenderOptionList SSAO::getRenderOptions() noexcept
{
    RenderOptionList newOptions;
    return newOptions;
}

SSAO::RenderOptions SSAO::convertOptions(
    [[maybe_unused]] RenderOptionList const &options) noexcept
{
    RenderOptions newOptions;
    return newOptions;
}

ComponentList SSAO::getComponents() const noexcept
{
    ComponentList components;
    return components;
}

SharedBufferList SSAO::getSharedBuffers() const noexcept
{
    SharedBufferList buffers;
    return buffers;
}

SharedTextureList SSAO::getSharedTextures() const noexcept
{
    SharedTextureList textures;
    textures.push_back({"GBuffer1", SharedTexture::Access::Read});
    textures.push_back({"DepthCopy", SharedTexture::Access::Read});
    textures.push_back(
        {"AO", SharedTexture::Access::Write, SharedTexture::Flags::Clear, DXGI_FORMAT_R8_UNORM});
    textures.push_back(
        {"TempAO", SharedTexture::Access::Write, SharedTexture::Flags::Clear, DXGI_FORMAT_R8_UNORM});
    return textures;
}

DebugViewList SSAO::getDebugViews() const noexcept
{
    DebugViewList views;
    return views;
}

bool SSAO::init([[maybe_unused]] CapsaicinInternal const &capsaicin) noexcept
{
    m_ssaoProgram = capsaicin.createProgram("render_techniques/ssao/ssao");
    m_ssaoKernel  = gfxCreateComputeKernel(gfx_, m_ssaoProgram);
    m_blurProgram = capsaicin.createProgram("render_techniques/ssao/ssao_blur");
    m_blurKernel  = gfxCreateComputeKernel(gfx_, m_blurProgram);

    return m_ssaoKernel && m_blurKernel;
}

void SSAO::render([[maybe_unused]] CapsaicinInternal &capsaicin) noexcept
{
    const auto &depthTexture = capsaicin.getSharedTexture("DepthCopy");

    const glm::vec2 renderResolution = {depthTexture.getWidth(), depthTexture.getHeight()};
    auto const &    gpuDrawConstants = capsaicin.allocateConstantBuffer<SSAOConstants>(1);
    {
        SSAOConstants drawConstants     = {};
        drawConstants.viewProjection    = capsaicin.getCameraMatrices().view_projection;
        drawConstants.invViewProjection = capsaicin.getCameraMatrices().inv_view_projection;
        drawConstants.cameraPosition    = capsaicin.getCamera().eye;
        drawConstants.screenSize        = glm::vec4{renderResolution.x, renderResolution.y,
                                             1.0f / renderResolution.x,
                                             1.0f / renderResolution.y};

        gfxBufferGetData<SSAOConstants>(gfx_, gpuDrawConstants)[0] = drawConstants;
    }

    // Set the root parameters for Computing SSAO.
    {
        gfxProgramSetParameter(gfx_, m_ssaoProgram, "g_Constants", gpuDrawConstants);
        gfxProgramSetParameter(gfx_, m_ssaoProgram, "g_Depth", depthTexture);
        gfxProgramSetParameter(gfx_, m_ssaoProgram, "g_GBuffer1", capsaicin.getSharedTexture("GBuffer1"));
        gfxProgramSetParameter(gfx_, m_ssaoProgram, "g_AO", capsaicin.getSharedTexture("TempAO"));

        gfxProgramSetParameter(gfx_, m_ssaoProgram, "g_NearestSampler",
            capsaicin.getNearestSampler());
    }

    // Compute SSAO.
    {
        gfxCommandBindKernel(gfx_, m_ssaoKernel);

        glm::uvec2 const groupCount =
            ceil(renderResolution / static_cast<float>(TILE_SIZE));
        gfxCommandDispatch(gfx_, groupCount.x, groupCount.y, 1);
    }

    // Set parameters for blur.
    {
        gfxProgramSetParameter(gfx_, m_blurProgram, "g_Constants", gpuDrawConstants);
        gfxProgramSetParameter(gfx_, m_blurProgram, "g_Input", capsaicin.getSharedTexture("TempAO"));
        gfxProgramSetParameter(gfx_, m_blurProgram, "g_Output", capsaicin.getSharedTexture("AO"));
    }

    // Blur/Denoise AO.
    {
        gfxCommandBindKernel(gfx_, m_blurKernel);

        glm::uvec2 const groupCount = ceil(renderResolution / static_cast<float>(TILE_SIZE));
        gfxCommandDispatch(gfx_, groupCount.x, groupCount.y, 1);
    }

    gfxDestroyBuffer(gfx_, gpuDrawConstants);
}

void SSAO::terminate() noexcept
{
    gfxDestroyKernel(gfx_, m_ssaoKernel);
    m_ssaoKernel = {};
    gfxDestroyProgram(gfx_, m_ssaoProgram);
    m_ssaoProgram = {};
}

void SSAO::renderGUI([[maybe_unused]] CapsaicinInternal &capsaicin) const noexcept { }
} // namespace Capsaicin
