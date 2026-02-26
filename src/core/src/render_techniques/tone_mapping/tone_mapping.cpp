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

#include "tone_mapping.h"

#include "../../components/blue_noise_sampler/blue_noise_sampler.h"
#include "capsaicin_internal.h"

using namespace std;

namespace Capsaicin
{
ToneMapping::ToneMapping()
    : RenderTechnique("Tone mapping")
{}

ToneMapping::~ToneMapping()
{
    ToneMapping::terminate();
}

RenderOptionList ToneMapping::getRenderOptions() noexcept
{
    RenderOptionList newOptions;
    newOptions.emplace(RENDER_OPTION_MAKE(tonemap_enable, options));
    newOptions.emplace(RENDER_OPTION_MAKE(tonemap_operator, options));
    return newOptions;
}

ToneMapping::RenderOptions ToneMapping::convertOptions(RenderOptionList const &options) noexcept
{
    RenderOptions newOptions;
    RENDER_OPTION_GET(tonemap_enable, newOptions, options)
    RENDER_OPTION_GET(tonemap_operator, newOptions, options)
    return newOptions;
}

ComponentList ToneMapping::getComponents() const noexcept
{
    ComponentList components;
    components.emplace_back(COMPONENT_MAKE(BlueNoiseSampler));
    return components;
}

SharedBufferList ToneMapping::getSharedBuffers() const noexcept
{
    SharedBufferList buffers;
    buffers.push_back({.name = "Exposure", .access = SharedBuffer::Access::Read});
    return buffers;
}

SharedTextureList ToneMapping::getSharedTextures() const noexcept
{
    SharedTextureList textures;
    textures.push_back({.name = "Color", .access = SharedTexture::Access::ReadWrite});
    textures.push_back({.name = "Debug", .access = SharedTexture::Access::ReadWrite});
    textures.push_back({.name = "ColorScaled",
        .access               = SharedTexture::Access::ReadWrite,
        .flags                = SharedTexture::Flags::OptionalDiscard});
    return textures;
}

DebugViewList ToneMapping::getDebugViews() const noexcept
{
    DebugViewList views;
    views.emplace_back("ToneMappedOutput"); // Allow viewing output without overwriting input
    return views;
}

bool ToneMapping::init(CapsaicinInternal const &capsaicin) noexcept
{
    options = convertOptions(capsaicin.getOptions());
    if (options.tonemap_enable)
    {
        // Create kernels
        toneMappingProgram = capsaicin.createProgram("render_techniques/tone_mapping/tone_mapping");

        return initToneMapKernel();
    }
    return true;
}

void ToneMapping::render(CapsaicinInternal &capsaicin) noexcept
{
    auto const newOptions = convertOptions(capsaicin.getOptions());

    if (!newOptions.tonemap_enable)
    {
        if (options.tonemap_enable)
        {
            // Destroy resources when not being used
            terminate();
        }
        options = newOptions;
        return;
    }

    bool const recompile = options.tonemap_operator != newOptions.tonemap_operator;
    bool const reInit    = !options.tonemap_enable && newOptions.tonemap_enable;
    options              = newOptions;

    if (reInit)
    {
        if (!init(capsaicin))
        {
            return;
        }
    }
    else if (auto const newColourSpace = gfxGetBackBufferColorSpace(gfx_);
        recompile || newColourSpace != colourSpace)
    {
        if (!initToneMapKernel())
        {
            return;
        }
    }

    bool const usesScaling = capsaicin.hasSharedTexture("ColorScaled")
                          && capsaicin.hasOption<bool>("taa_enable")
                          && capsaicin.getOption<bool>("taa_enable");
    GfxTexture input =
        !usesScaling ? capsaicin.getSharedTexture("Color") : capsaicin.getSharedTexture("ColorScaled");
    GfxTexture output = input;

    if (auto const debugView = capsaicin.getCurrentDebugView(); !debugView.empty() && debugView != "None")
    {
        if (debugView == "ToneMappedOutput")
        {
            // Output tone-mapping to debug view instead of output. This is only possible when the input
            // buffer has the same dimensions as the "Debug" AOV
            if (!usesScaling)
            {
                output = capsaicin.getSharedTexture("Debug");
            }
            else
            {
                capsaicin.setDebugView("None");
            }
        }
        else
        {
            // Tone map the debug buffer if we are using a debug view
            if (capsaicin.checkDebugViewSharedTexture(debugView))
            {
                // If the debug view is actually an AOV then only tonemap if it's a floating point format
                auto const debugAOV = capsaicin.getSharedTexture(debugView);
                if (auto const format = debugAOV.getFormat();
                    format == DXGI_FORMAT_R32G32B32A32_FLOAT || format == DXGI_FORMAT_R32G32B32_FLOAT
                    || format == DXGI_FORMAT_R16G16B16A16_FLOAT || format == DXGI_FORMAT_R11G11B10_FLOAT)
                {
                    input  = debugAOV;
                    output = capsaicin.getSharedTexture("Debug");
                }
            }
            else
            {
                input  = capsaicin.getSharedTexture("Debug");
                output = input;
            }
        }
    }

    // Call the tone mapping kernel on each pixel of colour buffer
    if (usingDither)
    {
        auto const blueNoiseSampler = capsaicin.getComponent<BlueNoiseSampler>();
        blueNoiseSampler->addProgramParameters(capsaicin, toneMappingProgram);
        gfxProgramSetParameter(gfx_, toneMappingProgram, "g_FrameIndex", capsaicin.getFrameIndex());
    }
    auto const bufferDimensions =
        !usesScaling ? capsaicin.getRenderDimensions() : capsaicin.getWindowDimensions();
    gfxProgramSetParameter(gfx_, toneMappingProgram, "g_BufferDimensions", bufferDimensions);
    gfxProgramSetParameter(gfx_, toneMappingProgram, "g_InputBuffer", input);
    if (usingHDR)
    {
        gfxProgramSetParameter(gfx_, toneMappingProgram, "g_MaxLuminance", maxLuminance);
        gfxProgramSetParameter(gfx_, toneMappingProgram, "g_ExposureScale", exposureScale);
    }
    gfxProgramSetParameter(gfx_, toneMappingProgram, "g_OutputBuffer", output);
    gfxProgramSetParameter(gfx_, toneMappingProgram, "g_Exposure", capsaicin.getSharedBuffer("Exposure"));
    {
        TimedSection const timed_section(*this, "ToneMap");
        uint32_t const    *numThreads = gfxKernelGetNumThreads(gfx_, toneMapKernel);
        uint32_t const     numGroupsX = (bufferDimensions.x + numThreads[0] - 1) / numThreads[0];
        uint32_t const     numGroupsY = (bufferDimensions.y + numThreads[1] - 1) / numThreads[1];
        gfxCommandBindKernel(gfx_, toneMapKernel);
        gfxCommandDispatch(gfx_, numGroupsX, numGroupsY, 1);
    }
}

void ToneMapping::terminate() noexcept
{
    gfxDestroyKernel(gfx_, toneMapKernel);
    gfxDestroyProgram(gfx_, toneMappingProgram);

    toneMapKernel      = {};
    toneMappingProgram = {};
}

void ToneMapping::renderGUI(CapsaicinInternal &capsaicin) const noexcept
{
    bool &enabled = capsaicin.getOption<bool>("tonemap_enable");
    ImGui::Checkbox("Enable Tone Mapping", &enabled);
    if (enabled)
    {
        constexpr array<char const *, 10> operatorList     = {"None", "Reinhard Simple", "Reinhard Luminance",
                "ACES Approximate", "ACES Fitted", "ACES", "PBR Neutral", "Uncharted 2", "Agx Fitted", "Agx"};
        auto const                        currentOperator  = capsaicin.getOption<uint8_t>("tonemap_operator");
        auto                              selectedOperator = static_cast<int32_t>(currentOperator);
        if (ImGui::Combo("Tone Mapper", &selectedOperator, operatorList.data(),
                static_cast<int32_t>(operatorList.size())))
        {
            if (currentOperator != static_cast<uint8_t>(selectedOperator))
            {
                capsaicin.setOption("tonemap_operator", static_cast<uint8_t>(selectedOperator));
            }
        }
    }
}

bool ToneMapping::initToneMapKernel() noexcept
{
    gfxDestroyKernel(gfx_, toneMapKernel);

    // Get current display color space and depth
    colourSpace = gfxGetBackBufferColorSpace(gfx_);

    vector<char const *> defines;
    if (colourSpace == DXGI_COLOR_SPACE_RGB_FULL_G10_NONE_P709)
    {
        // scRGB
        defines.push_back("OUTPUT_SCRGB");
    }
    else if (colourSpace == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020)
    {
        // BT2020
        defines.push_back("OUTPUT_HDR10");
    }
    else // DXGI_COLOR_SPACE_RGB_FULL_G22_NONE_P709
    {
        // Assume anything else is just sRGB as we don't know what it is
        defines.push_back("OUTPUT_SRGB");
    }

    usingDither = false;
    usingHDR    = false;
    if (auto const displayFormat = gfxGetBackBufferFormat(gfx_);
        displayFormat == DXGI_FORMAT_R16G16B16A16_FLOAT)
    {
        // HDR, can only be scRGB
        usingHDR = true;
    }
    else if (displayFormat == DXGI_FORMAT_R10G10B10A2_UNORM)
    {
        // Can either be 10bit SDR or HDR10
        if (colourSpace != DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020)
        {
            // 10 bit SDR
            defines.push_back("DITHER_10");
            usingDither = true;
        }
        else
        {
            usingHDR = true;
        }
    }
    else
    {
        // 8bit SDR format
        defines.push_back("DITHER_8");
        usingDither = true;
    }

    if (usingHDR)
    {
        defines.push_back("OUTPUT_HDR");

        // Get display luminance as that's tied to output kernel
        // As many current OLED panels cant provide max brightness at 1000% APL we split somewhere in the
        // middle
        auto const displayValues = gfxGetDisplayDescription(gfx_);
        maxLuminance             = displayValues.max_luminance
                     + ((displayValues.max_luminance_full_frame - displayValues.max_luminance) * 0.5F);
        // Standard SDR white level is 80 cd/m2, HDR displays require brighter white level (see ITU-R
        // BT.2408-7) so we scale by the higher reference white
        exposureScale = displayValues.reference_sdr_white_level / 80.0F;
    }

    switch (static_cast<RenderOptions::TonemapOperator>(options.tonemap_operator))
    {
    case RenderOptions::TonemapOperator::ReinhardSimple:    defines.push_back("TONEMAP_REINHARD"); break;
    case RenderOptions::TonemapOperator::ReinhardLuminance: defines.push_back("TONEMAP_REINHARDL"); break;
    case RenderOptions::TonemapOperator::ACESFast:          defines.push_back("TONEMAP_ACESFAST"); break;
    case RenderOptions::TonemapOperator::ACESFitted:        defines.push_back("TONEMAP_ACESFITTED"); break;
    case RenderOptions::TonemapOperator::ACES:              defines.push_back("TONEMAP_ACES"); break;
    case RenderOptions::TonemapOperator::PBRNeutral:        defines.push_back("TONEMAP_PBRNEUTRAL"); break;
    case RenderOptions::TonemapOperator::Uncharted2:        defines.push_back("TONEMAP_UNCHARTED2"); break;
    case RenderOptions::TonemapOperator::AgxFitted:         defines.push_back("TONEMAP_AGXFITTED"); break;
    case RenderOptions::TonemapOperator::Agx:               defines.push_back("TONEMAP_AGX"); break;
    default:                                                defines.push_back("TONEMAP_NONE"); break;
    }
    toneMapKernel = gfxCreateComputeKernel(
        gfx_, toneMappingProgram, "Tonemap", defines.data(), static_cast<uint32_t>(defines.size()));

    return !!toneMapKernel;
}
} // namespace Capsaicin
