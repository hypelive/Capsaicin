#include "depth_copy.h"
#include "capsaicin_internal.h"
#include "depth_copy_shared.h"

namespace Capsaicin
{
DepthCopy::DepthCopy()
    : RenderTechnique("Depth Copy") {}

DepthCopy::~DepthCopy()
{
    DepthCopy::terminate();
}

RenderOptionList DepthCopy::getRenderOptions() noexcept
{
    RenderOptionList newOptions;
    return newOptions;
}

DepthCopy::RenderOptions DepthCopy::convertOptions(
    [[maybe_unused]] RenderOptionList const &options) noexcept
{
    RenderOptions newOptions;
    return newOptions;
}

ComponentList DepthCopy::getComponents() const noexcept
{
    ComponentList components;
    components.emplace_back("ProbeBaker");
    components.emplace_back("CustomLightBuilder");
    return components;
}

SharedBufferList DepthCopy::getSharedBuffers() const noexcept
{
    SharedBufferList buffers;
    return buffers;
}

SharedTextureList DepthCopy::getSharedTextures() const noexcept
{
    SharedTextureList textures;
    textures.push_back({"DepthCopy", SharedTexture::Access::Write, SharedTexture::Flags::Clear,
                        DXGI_FORMAT_R32_FLOAT});
    return textures;
}

DebugViewList DepthCopy::getDebugViews() const noexcept
{
    DebugViewList views;
    return views;
}

bool DepthCopy::init([[maybe_unused]] CapsaicinInternal const &capsaicin) noexcept
{
    m_program = capsaicin.createProgram(
        "render_techniques/depth_copy/depth_copy");

    GfxDrawState const shadingDrawState = {};
    gfxDrawStateSetCullMode(shadingDrawState, D3D12_CULL_MODE_BACK);
    gfxDrawStateSetDepthFunction(shadingDrawState, D3D12_COMPARISON_FUNC_ALWAYS);

    gfxDrawStateSetColorTarget(shadingDrawState, 0, capsaicin.getSharedTexture("DepthCopy").getFormat());

    m_kernel =
        gfxCreateGraphicsKernel(gfx_, m_program, shadingDrawState);

    return m_kernel;
}

void DepthCopy::render([[maybe_unused]] CapsaicinInternal &capsaicin) noexcept
{
    gfxProgramSetParameter(gfx_, m_program, "g_Depth", capsaicin.getSharedTexture("Depth"));

    gfxCommandBindColorTarget(gfx_, 0, capsaicin.getSharedTexture("DepthCopy"));
    gfxCommandBindKernel(gfx_, m_kernel);

    gfxCommandDraw(gfx_, 3);
}

void DepthCopy::terminate() noexcept
{
    gfxDestroyKernel(gfx_, m_kernel);
    m_kernel = {};
    gfxDestroyProgram(gfx_, m_program);
    m_program = {};
}

void DepthCopy::renderGUI([[maybe_unused]] CapsaicinInternal &capsaicin) const noexcept { }
} // namespace Capsaicin
