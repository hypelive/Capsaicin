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

#include "fsr.h"

#include "capsaicin_internal.h"
#include "dx12/ffx_api_dx12.hpp"
#include "ffx_upscale.hpp"

using namespace std;

namespace
{
/**
 * Converts a GfxTexture to an FfxApiResource, handling resource state transitions as needed.
 * This function ensures that the input texture is in the correct resource state (readable or writeable) for
 * use in FidelityFX Super Resolution (FSR) operations. If required, it transitions the resource state
 * accordingly using the graphics API. It then constructs the appropriate FfxApiResource descriptor from the
 * Direct3D 12 resource and its metadata for downstream FSR usage.
 * @param capsaicin Reference to CapsaicinInternal instance, providing access to the graphics API.
 * @param texture   GfxTexture handle of the resource to convert/wrap.
 * @param writeable Whether the resource needs to be in a writeable state (unordered access) or readable
 * state.
 * @return          FfxApiResource compatible with the FSR library, representing the input GfxTexture.
 */
FfxApiResource ToFfxApiResource(
    Capsaicin::CapsaicinInternal const &capsaicin, GfxTexture const &texture, bool const writeable)
{
    auto const gfx = capsaicin.getGfx();

    // Convert input texture to required resource state. If the desired access (writeable/readable) doesn't
    // match, perform a state transition to ensure safe access.
    auto resource_states = gfxTextureGetResourceState(gfx, texture);
    if (writeable && (resource_states & D3D12_RESOURCE_STATE_UNORDERED_ACCESS) == 0)
    {
        gfxTextureSetResourceState(gfx, texture, D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
        resource_states = gfxTextureGetResourceState(gfx, texture);
    }
    else if (!writeable && (resource_states & D3D12_RESOURCE_STATE_UNORDERED_ACCESS) != 0)
    {
        gfxTextureSetResourceState(gfx, texture, D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
        resource_states = gfxTextureGetResourceState(gfx, texture);
    }

    // Get resource description
    auto *const resource      = gfxTextureGetResource(gfx, texture);
    auto const  resource_desc = resource->GetDesc();

    // Setup the FFX API resource descriptor
    FfxApiResource apiResource       = {};
    apiResource.resource             = resource;
    apiResource.description.type     = FFX_API_RESOURCE_TYPE_TEXTURE2D;
    apiResource.description.width    = static_cast<uint32_t>(resource_desc.Width);
    apiResource.description.height   = static_cast<uint32_t>(resource_desc.Height);
    apiResource.description.mipCount = texture.getMipLevels();
    apiResource.description.usage    = FFX_API_RESOURCE_USAGE_READ_ONLY;
    // This is not all the supported surface formats the FfxApi supports, but they cover all the types we use
    // internally and that make any sense to pass to FSR
    if (auto const textureFormat = texture.getFormat(); textureFormat == DXGI_FORMAT_R16_FLOAT)
    {
        apiResource.description.format = FFX_API_SURFACE_FORMAT_R16_FLOAT;
    }
    else if (textureFormat == DXGI_FORMAT_R16G16_FLOAT)
    {
        apiResource.description.format = FFX_API_SURFACE_FORMAT_R16G16_FLOAT;
    }
    else if (textureFormat == DXGI_FORMAT_R16G16B16A16_FLOAT)
    {
        apiResource.description.format = FFX_API_SURFACE_FORMAT_R16G16B16A16_FLOAT;
    }
    else if (textureFormat == DXGI_FORMAT_R32_FLOAT)
    {
        apiResource.description.format = FFX_API_SURFACE_FORMAT_R32_FLOAT;
    }
    else if (textureFormat == DXGI_FORMAT_R32G32_FLOAT)
    {
        apiResource.description.format = FFX_API_SURFACE_FORMAT_R32G32_FLOAT;
    }
    else if (textureFormat == DXGI_FORMAT_R32G32B32A32_FLOAT)
    {
        apiResource.description.format = FFX_API_SURFACE_FORMAT_R32G32B32A32_FLOAT;
    }
    else
    {
        GFX_ASSERTMSG(0, "An unsupported texture format was supplied");
    }

    if ((resource_desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET) != 0)
    {
        apiResource.description.usage |= FFX_API_RESOURCE_USAGE_RENDERTARGET;
    }
    if ((resource_desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL) != 0)
    {
        apiResource.description.usage |= FFX_API_RESOURCE_USAGE_DEPTHTARGET;
    }
    if ((resource_desc.Flags & D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS) != 0)
    {
        apiResource.description.usage |= FFX_API_RESOURCE_USAGE_UAV;
    }

    apiResource.state = 0;
    if ((resource_states & D3D12_RESOURCE_STATE_COMMON) != 0)
    {
        apiResource.state |= FFX_API_RESOURCE_STATE_COMMON;
    }
    else
    {
        if ((resource_states & D3D12_RESOURCE_STATE_RENDER_TARGET) != 0)
        {
            apiResource.state |= FFX_API_RESOURCE_STATE_RENDER_TARGET;
        }
        if ((resource_states & D3D12_RESOURCE_STATE_UNORDERED_ACCESS) != 0)
        {
            apiResource.state |= FFX_API_RESOURCE_STATE_UNORDERED_ACCESS;
        }
        if ((resource_states & D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE) != 0)
        {
            apiResource.state |= FFX_API_RESOURCE_STATE_COMPUTE_READ;
        }
        if ((resource_states & D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE) != 0)
        {
            apiResource.state |= FFX_API_RESOURCE_STATE_PIXEL_READ;
        }
        if ((resource_states & D3D12_RESOURCE_STATE_INDIRECT_ARGUMENT) != 0)
        {
            apiResource.state |= FFX_API_RESOURCE_STATE_INDIRECT_ARGUMENT;
        }
        if ((resource_states & D3D12_RESOURCE_STATE_COPY_DEST) != 0)
        {
            apiResource.state |= FFX_API_RESOURCE_STATE_COPY_DEST;
        }
        if ((resource_states & D3D12_RESOURCE_STATE_COPY_SOURCE) != 0)
        {
            apiResource.state |= FFX_API_RESOURCE_STATE_COPY_SRC;
        }
        if ((resource_states & D3D12_RESOURCE_STATE_GENERIC_READ) != 0)
        {
            apiResource.state |= FFX_API_RESOURCE_STATE_GENERIC_READ;
        }
        if ((resource_states & D3D12_RESOURCE_STATE_PRESENT) != 0)
        {
            apiResource.state |= FFX_API_RESOURCE_STATE_PRESENT;
        }
    }

    return apiResource;
}

void FFXMessageCallback(uint32_t /*type*/, wchar_t const *message)
{
    size_t const len    = wcstombs(nullptr, message, 0) + 1;
    auto *const  buffer = new char[len];
    ignore              = wcstombs(buffer, message, len);
    GFX_PRINTLN(buffer);
    delete[] buffer;
}
} // unnamed namespace

namespace Capsaicin
{

FSR::FSR()
    : RenderTechnique("FSR")
{}

FSR::~FSR()
{
    FSR::terminate();
}

RenderOptionList FSR::getRenderOptions() noexcept
{
    RenderOptionList newOptions;
    newOptions.emplace(RENDER_OPTION_MAKE(taa_enable, options_));
    newOptions.emplace(RENDER_OPTION_MAKE(fsr_version, options_));
    newOptions.emplace(RENDER_OPTION_MAKE(fsr_sharpen_enable, options_));
    newOptions.emplace(RENDER_OPTION_MAKE(fsr_sharpen_sharpness, options_));
    return newOptions;
}

FSR::RenderOptions FSR::convertOptions(RenderOptionList const &options) noexcept
{
    RenderOptions newOptions;
    RENDER_OPTION_GET(taa_enable, newOptions, options)
    RENDER_OPTION_GET(fsr_version, newOptions, options)
    newOptions.fsr_version =
        min(newOptions.fsr_version, static_cast<uint8_t>(RenderOptions::FSRVersion::FSR4));
    RENDER_OPTION_GET(fsr_sharpen_enable, newOptions, options)
    RENDER_OPTION_GET(fsr_sharpen_sharpness, newOptions, options)
    return newOptions;
}

SharedBufferList FSR::getSharedBuffers() const noexcept
{
    SharedBufferList buffers;
    buffers.push_back({.name = "Exposure",
        .access              = SharedBuffer::Access::Write,
        .flags               = SharedBuffer::Flags::OptionalDiscard});
    return buffers;
}

SharedTextureList FSR::getSharedTextures() const noexcept
{
    SharedTextureList textures;
    textures.push_back({.name = "Color"});
    textures.push_back({.name = "ColorScaled",
        .access               = SharedTexture::Access::Write,
        .flags                = SharedTexture::Flags::Optional});
    textures.push_back({.name = "VisibilityDepth"});
    textures.push_back({.name = "Velocity"});
    return textures;
}

bool FSR::init(CapsaicinInternal const &capsaicin) noexcept
{
    options_ = convertOptions(capsaicin.getOptions());
    if (options_.taa_enable)
    {
        // Create DX12 backend
        ffx::CreateBackendDX12Desc backendDesc = {};
        backendDesc.header.type                = FFX_API_CREATE_CONTEXT_DESC_TYPE_BACKEND_DX12;
        backendDesc.device                     = gfxGetDevice(gfx_);

        // Create FSR upscaler
        auto const                    renderResolution = capsaicin.getRenderDimensions();
        auto const                    windowResolution = capsaicin.getWindowDimensions();
        ffx::CreateContextDescUpscale createFsr        = {};

        createFsr.maxUpscaleSize = {
            .width = max(3840U, windowResolution.x), .height = max(2160U, windowResolution.y)};
        createFsr.maxRenderSize = {
            .width = max(3840U, renderResolution.x), .height = max(2160U, renderResolution.y)};
        createFsr.flags |= FFX_UPSCALE_ENABLE_DEPTH_INVERTED | FFX_UPSCALE_ENABLE_HIGH_DYNAMIC_RANGE;
        if (!capsaicin.hasSharedBuffer("Exposure")) [[unlikely]]
        {
            createFsr.flags |= FFX_UPSCALE_ENABLE_AUTO_EXPOSURE;
        }
        else
        {
            exposure_ = gfxCreateTexture2D(gfx_, 1, 1, DXGI_FORMAT_R32_FLOAT);
            exposure_.setName("FSR_Exposure");
        }
        createFsr.fpMessage = FFXMessageCallback;

        // Query available FSR versions
        ffx::QueryDescGetVersions versionQuery = {};
        versionQuery.header.type               = FFX_API_QUERY_DESC_TYPE_GET_VERSIONS;
        versionQuery.device                    = backendDesc.device;
        versionQuery.createDescType            = FFX_API_CREATE_CONTEXT_DESC_TYPE_UPSCALE;
        uint64_t versionCount                  = 0;
        versionQuery.outputCount               = &versionCount;
        ffxQuery(nullptr, &versionQuery.header);

        vector<char const *> versionNames(versionCount);
        vector<uint64_t>     versionIds(versionCount);
        versionQuery.versionIds   = versionIds.data();
        versionQuery.versionNames = versionNames.data();
        ffxQuery(nullptr, &versionQuery.header);

        // Check if any valid FSR versions are available
        if (versionNames.empty()) [[unlikely]]
        {
            GFX_PRINTLN("Error: No FSR versions available");
            available_versions_ = {false, false, false, false};
            return false;
        }

        // Check which FSR versions are available
        constexpr array versionChars = {'2', '3', '4'};
        if (auto const pos = ranges::find_if(
                versionNames, [&versionChars](char const *name) { return name[0] == versionChars[0]; });
            pos != versionNames.end()) [[likely]]
        {
            available_versions_[static_cast<uint8_t>(RenderOptions::FSRVersion::FSR2)] = true;
        }
        if (auto const pos = ranges::find_if(
                versionNames, [&versionChars](char const *name) { return name[0] == versionChars[1]; });
            pos != versionNames.end()) [[likely]]
        {
            available_versions_[static_cast<uint8_t>(RenderOptions::FSRVersion::FSR3)] = true;
        }
        if (auto const pos = ranges::find_if(
                versionNames, [&versionChars](char const *name) { return name[0] == versionChars[2]; });
            pos != versionNames.end())
        {
            available_versions_[static_cast<uint8_t>(RenderOptions::FSRVersion::FSR4)] = true;
        }
        if (available_versions_[1] || available_versions_[2] || available_versions_[3])
        {
            available_versions_[0] = true;
        }

        if (options_.fsr_version != static_cast<uint8_t>(RenderOptions::FSRVersion::Auto)) [[unlikely]]
        {
            // Check requested FSR version
            if (!available_versions_[options_.fsr_version]) [[unlikely]]
            {
                GFX_PRINTLN(
                    "Warning: Requested FSR version is not available. Falling back to automatic selection");
                options_.fsr_version = static_cast<uint8_t>(RenderOptions::FSRVersion::Auto);
            }
            else
            {
                // Create version override
                uint64_t const versionPos = static_cast<uint64_t>(
                    distance(versionNames.begin(), ranges::find_if(versionNames, [&](char const *name) {
                        return name[0] == versionChars[options_.fsr_version - 1];
                    })));
                ffx::CreateContextDescOverrideVersion versionOverride {};
                versionOverride.versionId = versionIds[versionPos];
                if (ffx::CreateContext(upscale_context_, nullptr, createFsr, backendDesc, versionOverride)
                    != ffx::ReturnCode::Ok)
                {
                    return false;
                }
            }
        }

        if (options_.fsr_version == static_cast<uint8_t>(RenderOptions::FSRVersion::Auto))
        {
            if (ffx::CreateContext(upscale_context_, nullptr, createFsr, backendDesc) != ffx::ReturnCode::Ok)
            {
                return false;
            }
        }

        // Check what version of FSR has been loaded
        ffxQueryGetProviderVersion version = {};
        version.header.type                = FFX_API_QUERY_DESC_TYPE_GET_PROVIDER_VERSION;
        ffxQuery(&upscale_context_, &version.header);
        if (version.versionName != nullptr)
        {
            version_ = version.versionName;
        }
        else
        {
            ffx::DestroyContext(upscale_context_);
            return false;
        }
    }

    return true;
}

void FSR::render(CapsaicinInternal &capsaicin) noexcept
{
    RenderOptions const newOptions = convertOptions(capsaicin.getOptions());

    if (!newOptions.taa_enable)
    {
        if (options_.taa_enable)
        {
            gfxFinish(gfx_); // Must wait for all commands to finish before destroying resources
            // Destroy resources when not being used
            terminate();
        }
        options_ = newOptions; // apply options
        return;
    }

    bool const reInit      = !options_.taa_enable || (newOptions.fsr_version != options_.fsr_version);
    bool       cameraReset = capsaicin.getCameraChanged() || capsaicin.getSceneUpdated()
                    || capsaicin.getEnvironmentMapUpdated() || reInit;

    options_ = newOptions; // apply options

    if (reInit)
    {
        if (upscale_context_ != nullptr)
        {
            gfxFinish(gfx_); // Must wait for all commands to finish before destroying
            terminate();
        }
        // Only initialise data if actually being used
        if (!init(capsaicin))
        {
            return;
        }
    }
    else
    {
        if (capsaicin.getRenderDimensionsUpdated() || capsaicin.getWindowDimensionsUpdated()
            || capsaicin.getFrameIndex() == 0)
        {
            cameraReset = true;
            // Set Capsaicin's jitter to match FSR requirements
            capsaicin.setCameraJitterPhase(
                static_cast<uint32_t>(8.0F * powf(1.0F / capsaicin.getRenderDimensionsScale(), 2.0F)));
        }
    }

    // Check if we have pre-calculated exposure value
    bool const hasExposure = !!exposure_;
    if (hasExposure)
    {
        // Update texture with exposure value
        TimedSection const timed_section(*this, "Update Exposure");
        gfxCommandCopyBufferToTexture(gfx_, exposure_, capsaicin.getSharedBuffer("Exposure"));
    }

    auto const renderDimensions  = capsaicin.getRenderDimensions();
    auto const displayDimensions = capsaicin.getWindowDimensions();
    auto const motionVectorScale = -static_cast<float2>(renderDimensions);
    auto const jitterOffset      = capsaicin.getCameraJitter() * motionVectorScale * 0.5F;
    auto const camera            = capsaicin.getCamera();

    // Get hold of the correct output texture
    bool const usesScaling =
        capsaicin.hasSharedTexture("ColorScaled") && (capsaicin.getRenderDimensionsScale() < 1.0F);
    GfxTexture const &colourAOV = capsaicin.getSharedTexture("Color");

    {
        TimedSection const timed_section(*this, "FSR");

        GfxTexture const &outputAOV = (!usesScaling ? colourAOV : capsaicin.getSharedTexture("ColorScaled"));
        GfxTexture const &depthAOV  = capsaicin.getSharedTexture("VisibilityDepth");
        GfxTexture const &velocityAOV = capsaicin.getSharedTexture("Velocity");
        // Perform our image processing
        ffx::DispatchDescUpscale dispatchUpscale   = {};
        dispatchUpscale.commandList                = gfxGetCommandList(gfx_);
        dispatchUpscale.color                      = ToFfxApiResource(capsaicin, colourAOV, !usesScaling);
        dispatchUpscale.depth                      = ToFfxApiResource(capsaicin, depthAOV, false);
        dispatchUpscale.motionVectors              = ToFfxApiResource(capsaicin, velocityAOV, false);
        dispatchUpscale.exposure                   = hasExposure
                                                       ? ToFfxApiResource(capsaicin, exposure_, false)
                                                       : FfxApiResource {.resource = nullptr, .description = {}, .state = 0};
        dispatchUpscale.reactive                   = {.resource = nullptr, .description = {}, .state = 0};
        dispatchUpscale.transparencyAndComposition = {.resource = nullptr, .description = {}, .state = 0};
        dispatchUpscale.output                     = ToFfxApiResource(capsaicin, outputAOV, true);
        dispatchUpscale.jitterOffset.x             = +jitterOffset.x;
        dispatchUpscale.jitterOffset.y             = -jitterOffset.y;
        dispatchUpscale.motionVectorScale.x        = motionVectorScale.x;
        dispatchUpscale.motionVectorScale.y        = motionVectorScale.y;
        if (options_.fsr_version == static_cast<uint8_t>(RenderOptions::FSRVersion::FSR2))
        {
            // FSR2 requires render size to be doubled
            dispatchUpscale.renderSize.width  = renderDimensions.x * 2;
            dispatchUpscale.renderSize.height = renderDimensions.y * 2;
        }
        else
        {
            dispatchUpscale.renderSize.width  = renderDimensions.x;
            dispatchUpscale.renderSize.height = renderDimensions.y;
        }
        dispatchUpscale.upscaleSize.width  = usesScaling ? displayDimensions.x : renderDimensions.x;
        dispatchUpscale.upscaleSize.height = usesScaling ? displayDimensions.y : renderDimensions.y;
        dispatchUpscale.enableSharpening   = options_.fsr_sharpen_enable;
        dispatchUpscale.sharpness          = options_.fsr_sharpen_sharpness;
        dispatchUpscale.frameTimeDelta =
            static_cast<float>(glm::clamp(capsaicin.getFrameTime(), 0.0, 1.0)) * 1000.0F;
        dispatchUpscale.preExposure             = 1.0F;
        dispatchUpscale.reset                   = cameraReset;
        dispatchUpscale.cameraNear              = camera.nearZ;
        dispatchUpscale.cameraFar               = camera.farZ;
        dispatchUpscale.cameraFovAngleVertical  = camera.fovY;
        dispatchUpscale.viewSpaceToMetersFactor = 1.0F;
        dispatchUpscale.flags                   = 0;

        ffx::Dispatch(upscale_context_, dispatchUpscale);

        gfxResetCommandListState(gfx_);
    }
}

void FSR::renderGUI(CapsaicinInternal &capsaicin) const noexcept
{
    auto const enabled         = capsaicin.getOption<bool>("taa_enable");
    auto       selectedEnabled = enabled;
    if (ImGui::Checkbox("Enable FSR", &selectedEnabled))
    {
        if (enabled != selectedEnabled)
        {
            capsaicin.setOption("taa_enable", selectedEnabled);
        }
    }
    if (enabled)
    {
        auto const currentVersion  = capsaicin.getOption<uint8_t>("fsr_version");
        auto       selectedVersion = currentVersion;
        if (constexpr array<char const *, 4> versionsString = {"Auto", "FSR2", "FSR3", "FSR4"};
            ImGui::BeginCombo("FSR Version", versionsString[currentVersion]))
        {
            if (ImGui::Selectable(versionsString[0], selectedVersion == 0,
                    available_versions_[0] ? 0 : ImGuiSelectableFlags_Disabled))
            {
                selectedVersion = 0;
            }
            if (ImGui::Selectable(versionsString[1], selectedVersion == 1,
                    available_versions_[1] ? 0 : ImGuiSelectableFlags_Disabled))
            {
                selectedVersion = 1;
            }
            if (ImGui::Selectable(versionsString[2], selectedVersion == 2,
                    available_versions_[2] ? 0 : ImGuiSelectableFlags_Disabled))
            {
                selectedVersion = 2;
            }
            if (ImGui::Selectable(versionsString[3], selectedVersion == 3,
                    available_versions_[3] ? 0 : ImGuiSelectableFlags_Disabled))
            {
                selectedVersion = 3;
            }
            ImGui::EndCombo();
            if (currentVersion != selectedVersion)
            {
                capsaicin.setOption("fsr_version", selectedVersion);
            }
        }
        ImGui::Text("FSR active version : %s", version_.c_str());
    }
}

void FSR::terminate() noexcept
{
    if (upscale_context_ != nullptr)
    {
        ffx::DestroyContext(upscale_context_);
        upscale_context_ = nullptr;
    }

    gfxDestroyTexture(gfx_, exposure_);
    exposure_ = {};
}

} // namespace Capsaicin
