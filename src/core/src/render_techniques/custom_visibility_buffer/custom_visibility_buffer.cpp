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

#include "custom_visibility_buffer.h"
#include "capsaicin_internal.h"

namespace Capsaicin
{
CustomVisibilityBuffer::CustomVisibilityBuffer()
    : RenderTechnique("Custom Visibility buffer")
{}

CustomVisibilityBuffer::~CustomVisibilityBuffer()
{
    CustomVisibilityBuffer::terminate();
}

RenderOptionList CustomVisibilityBuffer::getRenderOptions() noexcept
{
    RenderOptionList newOptions;
    return newOptions;
}

CustomVisibilityBuffer::RenderOptions CustomVisibilityBuffer::convertOptions([[maybe_unused]] RenderOptionList const &options) noexcept
{
    RenderOptions newOptions;
    return newOptions;
}

ComponentList CustomVisibilityBuffer::getComponents() const noexcept
{
    ComponentList components;
    return components;
}

SharedBufferList CustomVisibilityBuffer::getSharedBuffers() const noexcept
{
    SharedBufferList buffers;
    return buffers;
}

SharedTextureList CustomVisibilityBuffer::getSharedTextures() const noexcept
{
    SharedTextureList textures;
    textures.push_back({"Color", SharedTexture::Access::Write});
    return textures;
}

DebugViewList CustomVisibilityBuffer::getDebugViews() const noexcept
{
    DebugViewList views;
    return views;
}

bool CustomVisibilityBuffer::init([[maybe_unused]] CapsaicinInternal const &capsaicin) noexcept
{
    m_visibility_buffer_program = capsaicin.createProgram("render_techniques/custom_visibility_buffer/custom_visibility_buffer");

    GfxDrawState const visibility_buffer_draw_state = {};
    gfxDrawStateSetCullMode(visibility_buffer_draw_state, D3D12_CULL_MODE_NONE);

    gfxDrawStateSetColorTarget(
        visibility_buffer_draw_state, 0, capsaicin.getSharedTexture("Color").getFormat());

    m_visibility_buffer_kernel =
        gfxCreateMeshKernel(gfx_, m_visibility_buffer_program, visibility_buffer_draw_state);

    return m_visibility_buffer_program;
}

void CustomVisibilityBuffer::render([[maybe_unused]] CapsaicinInternal &capsaicin) noexcept
{
    gfxCommandBindColorTarget(gfx_, 0, capsaicin.getSharedTexture("Color"));
    gfxCommandBindKernel(gfx_, m_visibility_buffer_kernel);
    gfxCommandDrawMesh(gfx_, 1, 1, 1);
}

void CustomVisibilityBuffer::terminate() noexcept
{
    // TODO Destroy Resources.
}

void CustomVisibilityBuffer::renderGUI([[maybe_unused]] CapsaicinInternal &capsaicin) const noexcept { }
} // namespace Capsaicin
