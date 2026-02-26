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

#include "ffx_api.hpp"
#include "render_technique.h"

namespace Capsaicin
{

class FSR : public RenderTechnique
{
public:
    FSR();
    ~FSR() override;

    /*
     * Gets configuration options for current technique.
     * @return A list of all valid configuration options.
     */
    RenderOptionList getRenderOptions() noexcept override;

    struct RenderOptions
    {
        enum class FSRVersion : uint8_t
        {
            Auto = 0, /*< Automatically choose the best FSR version */
            FSR2,     /**< Use FSR 2.X */
            FSR3,     /*< Use FSR 3.X */
            FSR4,     /*< Use FSR 4.X */
        };

        bool    taa_enable  = true;
        uint8_t fsr_version = static_cast<uint8_t>(FSRVersion::Auto); /**< Choose which FSR version to use */
        bool    fsr_sharpen_enable    = true;
        float   fsr_sharpen_sharpness = 0.8F;
    };

    /**
     * Convert render options to internal options format.
     * @param options Current render options.
     * @returns The options converted.
     */
    static RenderOptions convertOptions(RenderOptionList const &options) noexcept;

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
     * Initialise any internal data or state.
     * @note This is automatically called by the framework after construction and should be used to create
     * any required CPU|GPU resources.
     * @param capsaicin Current framework context.
     * @return True if initialisation succeeded, False otherwise.
     */
    bool init(CapsaicinInternal const &capsaicin) noexcept override;

    /**
     * Perform render operations.
     * @param [in,out] capsaicin The current capsaicin context.
     */
    void render(CapsaicinInternal &capsaicin) noexcept override;

    /**
     * Render GUI options.
     * @param [in,out] capsaicin The current capsaicin context.
     */
    void renderGUI(CapsaicinInternal &capsaicin) const noexcept override;

    /** Terminate the render technique. */
    void terminate() noexcept override;

protected:
    RenderOptions options_;

    ffx::Context        upscale_context_ = nullptr;
    std::string         version_;
    GfxTexture          exposure_;
    std::array<bool, 4> available_versions_ = {false, false, false, false};
};

} // namespace Capsaicin
