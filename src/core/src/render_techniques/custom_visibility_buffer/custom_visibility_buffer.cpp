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
#include "custom_visibility_buffer_shared.h"

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
    buffers.push_back({"Meshlets", SharedBuffer::Access::Read});
    buffers.push_back({"MeshletPack", SharedBuffer::Access::Read});
    return buffers;
}

SharedTextureList CustomVisibilityBuffer::getSharedTextures() const noexcept
{
    SharedTextureList textures;
    textures.push_back({"Color", SharedTexture::Access::Write});
    textures.push_back({"Depth", SharedTexture::Access::ReadWrite});
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
    gfxDrawStateSetCullMode(visibility_buffer_draw_state, D3D12_CULL_MODE_BACK);
    gfxDrawStateSetDepthFunction(visibility_buffer_draw_state, D3D12_COMPARISON_FUNC_GREATER);

    gfxDrawStateSetColorTarget(
        visibility_buffer_draw_state, 0, capsaicin.getSharedTexture("Color").getFormat());
    gfxDrawStateSetDepthStencilTarget(
        visibility_buffer_draw_state, capsaicin.getSharedTexture("Depth").getFormat());

    m_visibility_buffer_kernel =
        gfxCreateMeshKernel(gfx_, m_visibility_buffer_program, visibility_buffer_draw_state);

    return m_visibility_buffer_program;
}

void CustomVisibilityBuffer::render([[maybe_unused]] CapsaicinInternal &capsaicin) noexcept
{
    // Prepare draw data.
    {
        std::vector<DrawData> drawData;
#if 1
        for (auto const &index : capsaicin.getInstanceIdData())
        {
            Instance const &instance = capsaicin.getInstanceData()[index];

            for (uint32_t j = 0; j < instance.meshlet_count; ++j)
            {
                drawData.emplace_back(instance.meshlet_offset_idx + j, index);
            }
        }
#else // Draw the first instance only.
        uint32_t instanceId = capsaicin.getInstanceIdData()[0];
        Instance const &instance   = capsaicin.getInstanceData()[instanceId];
        for (uint32_t j = 0; j < instance.meshlet_count; ++j)
        {
            drawData.emplace_back(instance.meshlet_offset_idx + j, instanceId);
        }
#endif
        m_draw_data_size = static_cast<uint32_t>(drawData.size());
        gfxDestroyBuffer(gfx_, m_draw_data_buffer);
        // The data will be uploaded through the staging buffer (it seems), so we don't need to worry about requesting cpu access to the buffer.
        m_draw_data_buffer = gfxCreateBuffer<DrawData>(gfx_, m_draw_data_size, drawData.data());
    }

    // Filling the draw constants.
    {
        DrawConstants drawConstants = {};
        drawConstants.viewProjection = capsaicin.getCameraMatrices().view_projection;
        drawConstants.drawCount      = m_draw_data_size;

        gfxDestroyBuffer(gfx_, m_draw_constants_buffer);
        m_draw_constants_buffer = gfxCreateBuffer<DrawConstants>(gfx_, 1, &drawConstants);
    }

    // Initialize directional light data.
    // TODO move it to a separate component.
    {
        const uint32_t lightsCount = gfxSceneGetLightCount(capsaicin.getScene());
        const auto* const lights = gfxSceneGetLights(capsaicin.getScene());

        gfxDestroyBuffer(gfx_, m_lights_count_buffer);
        m_lights_count_buffer = gfxCreateBuffer<uint32_t>(gfx_, 1, &lightsCount);

        if (lightsCount > 0)
        {
            auto &firstLight = lights[0];
            if (firstLight.type == kGfxLightType_Directional)
            {
                const Light directionalLight = MakeDirectionalLight(firstLight.color * firstLight.intensity,
                    normalize(firstLight.direction), std::numeric_limits<float>::max());

                gfxDestroyBuffer(gfx_, m_lights_buffer);
                m_lights_buffer = gfxCreateBuffer<Light>(gfx_, 1, &directionalLight);
            }
        }
    }

    // Set the root parameters.
    {
        gfxProgramSetParameter(gfx_, m_visibility_buffer_program, "g_DrawConstants", m_draw_constants_buffer);
        gfxProgramSetParameter(gfx_, m_visibility_buffer_program, "g_DrawDataBuffer", m_draw_data_buffer);
        gfxProgramSetParameter(gfx_, m_visibility_buffer_program, "g_LightsCountBuffer", m_lights_count_buffer);
        gfxProgramSetParameter(gfx_, m_visibility_buffer_program, "g_LightsBuffer", m_lights_buffer);

        gfxProgramSetParameter(gfx_, m_visibility_buffer_program, "g_InstanceBuffer", capsaicin.getInstanceBuffer());
        gfxProgramSetParameter(gfx_, m_visibility_buffer_program, "g_TransformBuffer", capsaicin.getTransformBuffer());
        gfxProgramSetParameter(gfx_, m_visibility_buffer_program, "g_MeshletBuffer", capsaicin.getSharedBuffer("Meshlets"));
        gfxProgramSetParameter(gfx_, m_visibility_buffer_program, "g_MeshletPackBuffer",capsaicin.getSharedBuffer("MeshletPack"));
        gfxProgramSetParameter(gfx_, m_visibility_buffer_program, "g_VertexBuffer", capsaicin.getVertexBuffer());
        gfxProgramSetParameter(gfx_, m_visibility_buffer_program, "g_VertexDataIndex", capsaicin.getVertexDataIndex());
    }

    // Run the amplification shader.
    {
        gfxCommandClearTexture(gfx_, capsaicin.getSharedTexture("Depth"));
        gfxCommandClearTexture(gfx_, capsaicin.getSharedTexture("Color"));

        gfxCommandBindDepthStencilTarget(gfx_, capsaicin.getSharedTexture("Depth"));
        gfxCommandBindColorTarget(gfx_, 0, capsaicin.getSharedTexture("Color"));
        gfxCommandBindKernel(gfx_, m_visibility_buffer_kernel);

        uint32_t const* num_threads  = gfxKernelGetNumThreads(gfx_, m_visibility_buffer_kernel);
        uint32_t const  num_groups_x = (m_draw_data_size + num_threads[0] - 1) / num_threads[0];

        gfxCommandDrawMesh(gfx_, num_groups_x, 1, 1);
    }
}

void CustomVisibilityBuffer::terminate() noexcept
{
    // TODO Destroy Resources.
}

void CustomVisibilityBuffer::renderGUI([[maybe_unused]] CapsaicinInternal &capsaicin) const noexcept { }
} // namespace Capsaicin
