/**********************************************************************
Copyright (c) 2025 Advanced Micro Devices, Inc. All rights reserved.

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
#pragma once

#include "render_technique.h"

namespace Capsaicin
{
class ToneMapping final : public RenderTechnique
{
public:
    ToneMapping();
    ~ToneMapping() override;

    ToneMapping(ToneMapping const &other)                = delete;
    ToneMapping(ToneMapping &&other) noexcept            = delete;
    ToneMapping &operator=(ToneMapping const &other)     = delete;
    ToneMapping &operator=(ToneMapping &&other) noexcept = delete;

    /*
     * Gets configuration options for current technique.
     * @return A list of all valid configuration options.
     */
    [[nodiscard]] RenderOptionList getRenderOptions() noexcept override;

    struct RenderOptions
    {
        enum class TonemapOperator : uint8_t
        {
            None,
            ReinhardSimple,
            ReinhardLuminance,
            ACESFast,
            ACESFitted,
            ACES,
            PBRNeutral,
            Uncharted2,
            AgxFitted,
            Agx
        };

        bool    tonemap_enable   = true;
        uint8_t tonemap_operator = static_cast<uint8_t>(TonemapOperator::ACES);
    };

    /**
     * Convert render options to internal options format.
     * @param options Current render options.
     * @return The options converted.
     */
    static RenderOptions convertOptions(RenderOptionList const &options) noexcept;

    /**
     * Gets a list of any shared components used by the current render technique.
     * @return A list of all supported components.
     */
    [[nodiscard]] ComponentList getComponents() const noexcept override;

    /**
     * Gets a list of any shared buffers used by the current render technique.
     * @return A list of all supported buffers.
     */
    [[nodiscard]] SharedBufferList getSharedBuffers() const noexcept override;

    /**
     * Gets the required list of shared textures needed for the current render technique.
     * @return A list of all required shared textures.
     */
    [[nodiscard]] SharedTextureList getSharedTextures() const noexcept override;

    /**
     * Gets a list of any debug views provided by the current render technique.
     * @return A list of all supported debug views.
     */
    [[nodiscard]] DebugViewList getDebugViews() const noexcept override;

    /**
     * Initialise any internal data or state.
     * @note This is automatically called by the framework after construction and should be used to create
     * any required CPU|GPU resources.
     * @param capsaicin Current framework context.
     * @return True if initialisation succeeded, False otherwise.
     */
    [[nodiscard]] bool init(CapsaicinInternal const &capsaicin) noexcept override;

    /**
     * Perform render operations.
     * @param [in,out] capsaicin The current capsaicin context.
     */
    void render(CapsaicinInternal &capsaicin) noexcept override;

    /**
     * Destroy any used internal resources and shutdown.
     */
    void terminate() noexcept override;

    /**
     * Render GUI options.
     * @param [in,out] capsaicin The current capsaicin context.
     */
    void renderGUI(CapsaicinInternal &capsaicin) const noexcept override;

private:
    [[nodiscard]] bool initToneMapKernel() noexcept;

    RenderOptions options;

    DXGI_COLOR_SPACE_TYPE colourSpace;         /**< Current working space of the display */
    bool                  usingDither = false; /**< Whether dithering is being used based on display format */
    bool                  usingHDR    = false; /**< Whether HDR output is being used */
    float                 maxLuminance  = 1.0F; /**< Maximum luminance of the current display */
    float                 exposureScale = 1.0F; /**< Exposure scale for HDR reference white setting */

    GfxProgram toneMappingProgram;
    GfxKernel  toneMapKernel;
};
} // namespace Capsaicin
