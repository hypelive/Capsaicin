/**********************************************************************
Copyright (c) 2024 Advanced Micro Devices, Inc. All rights reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
********************************************************************/

#include "custom_skybox.h"
#include "capsaicin_internal.h"
#include "custom_skybox_shared.h"

namespace Capsaicin
{
CustomSkybox::CustomSkybox()
    : RenderTechnique("Custom Skybox")
{}

CustomSkybox::~CustomSkybox()
{
    CustomSkybox::terminate();
}

RenderOptionList CustomSkybox::getRenderOptions() noexcept
{
    RenderOptionList newOptions;
    return newOptions;
}

CustomSkybox::RenderOptions CustomSkybox::convertOptions([[maybe_unused]] RenderOptionList const &options) noexcept
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
    m_skybox_program = capsaicin.createProgram("render_techniques/custom_skybox/custom_skybox");

    GfxDrawState const skybox_draw_state = {};
    gfxDrawStateSetCullMode(skybox_draw_state, D3D12_CULL_MODE_BACK);
    gfxDrawStateSetDepthFunction(skybox_draw_state, D3D12_COMPARISON_FUNC_GREATER);

    gfxDrawStateSetColorTarget(
        skybox_draw_state, 0, capsaicin.getSharedTexture("Color").getFormat());
    gfxDrawStateSetDepthStencilTarget(
        skybox_draw_state, capsaicin.getSharedTexture("Depth").getFormat());

    m_skybox_kernel = gfxCreateGraphicsKernel(gfx_, m_skybox_program, skybox_draw_state);

    return m_skybox_kernel;
}

void CustomSkybox::render([[maybe_unused]] CapsaicinInternal &capsaicin) noexcept
{
    auto const& colorTexture = capsaicin.getSharedTexture("Color");

    // Filling the draw constants.
    {
        DrawConstants drawConstants = {};
        drawConstants.invViewProjection = capsaicin.getCameraMatrices().inv_view_projection;
        drawConstants.cameraPosition = capsaicin.getCamera().eye;
        drawConstants.invScreenSize = 1.0f / float2{colorTexture.getWidth(), colorTexture.getHeight()};

        gfxDestroyBuffer(gfx_, m_draw_constants_buffer);
        m_draw_constants_buffer = gfxCreateBuffer<DrawConstants>(gfx_, 1, &drawConstants);
    }

    // Set the root parameters.
    {
        gfxProgramSetParameter(gfx_, m_skybox_program, "g_DrawConstants", m_draw_constants_buffer);
        gfxProgramSetParameter(gfx_, m_skybox_program, "g_EnvironmentMap", capsaicin.getEnvironmentBuffer());

        gfxProgramSetParameter(gfx_, m_skybox_program, "g_LinearSampler", capsaicin.getLinearSampler());
    }

    // Run full screen triangle.
    {
        gfxCommandBindDepthStencilTarget(gfx_, capsaicin.getSharedTexture("Depth"));
        gfxCommandBindColorTarget(gfx_, 0, colorTexture);
        gfxCommandBindKernel(gfx_, m_skybox_kernel);

        gfxCommandDraw(gfx_, 3);
    }
}

void CustomSkybox::terminate() noexcept
{
    // TODO Destroy Resources.
}

void CustomSkybox::renderGUI([[maybe_unused]] CapsaicinInternal &capsaicin) const noexcept { }
} // namespace Capsaicin
