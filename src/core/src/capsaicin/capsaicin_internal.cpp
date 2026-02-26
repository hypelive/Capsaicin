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
#include "capsaicin_internal.h"

#include "common_functions.inl"
#include "components/light_builder/light_builder.h"
#include "render_technique.h"

#include <chrono>
#include <filesystem>
#include <gfx_imgui.h>
#include <imgui_stdlib.h>
#include <ppl.h>
#include <ranges>

using namespace std;

namespace Capsaicin
{
CapsaicinInternal::~CapsaicinInternal()
{
    terminate();
}

GfxContext CapsaicinInternal::getGfx() const
{
    return gfx_;
}

GfxScene CapsaicinInternal::getScene() const
{
    return scene_;
}

vector<string> CapsaicinInternal::getShaderPaths() const
{
    return {shader_path_, third_party_shader_path_, third_party_shader_path_ + "FidelityFX/gpu/"};
}

uint2 CapsaicinInternal::getWindowDimensions() const noexcept
{
    return window_dimensions_;
}

uint2 CapsaicinInternal::getRenderDimensions() const noexcept
{
    return render_dimensions_;
}

float CapsaicinInternal::getRenderDimensionsScale() const noexcept
{
    return render_scale_;
}

void CapsaicinInternal::setRenderDimensionsScale(float const scale) noexcept
{
    render_scale_                  = max(scale, 1.0F / 3.0F);
    auto const newRenderDimensions = max(uint2(round(float2(window_dimensions_) * render_scale_)), uint2(1));
    render_dimensions_updated_     = newRenderDimensions != render_dimensions_;
    render_dimensions_             = newRenderDimensions;
}

uint32_t CapsaicinInternal::getFrameIndex() const noexcept
{
    return frame_index_;
}

double CapsaicinInternal::getFrameTime() const noexcept
{
    return frame_time_;
}

double CapsaicinInternal::getAverageFrameTime() const noexcept
{
    return frameGraph.getAverageValue();
}

bool CapsaicinInternal::hasAnimation() const noexcept
{
    return gfxSceneGetAnimationCount(scene_) > 0;
}

void CapsaicinInternal::setPaused(bool const paused) noexcept
{
    play_paused_ = paused;
}

bool CapsaicinInternal::getPaused() const noexcept
{
    return play_paused_;
}

void CapsaicinInternal::setFixedFrameRate(bool const playMode) noexcept
{
    play_fixed_framerate_ = playMode;
}

void CapsaicinInternal::setFixedFrameTime(double const fixed_frame_time) noexcept
{
    play_fixed_frame_time_ = fixed_frame_time;
}

bool CapsaicinInternal::getFixedFrameRate() const noexcept
{
    return play_fixed_framerate_;
}

void CapsaicinInternal::restartPlayback() noexcept
{
    play_time_ = 0.0;
    // Also reset frame index so that rendering resumes from start as well
    frame_index_ = numeric_limits<uint32_t>::max();
}

void CapsaicinInternal::increasePlaybackSpeed() noexcept
{
    play_speed_ *= 2.0;
}

void CapsaicinInternal::decreasePlaybackSpeed() noexcept
{
    play_speed_ *= 0.5;
}

double CapsaicinInternal::getPlaybackSpeed() const noexcept
{
    return play_speed_;
}

void CapsaicinInternal::resetPlaybackSpeed() noexcept
{
    play_speed_ = 1.0;
}

void CapsaicinInternal::stepPlaybackForward(uint32_t const frames) noexcept
{
    play_time_ += static_cast<double>(frames) * play_fixed_frame_time_;
}

void CapsaicinInternal::stepPlaybackBackward(uint32_t const frames) noexcept
{
    play_time_ -= static_cast<double>(frames) * play_fixed_frame_time_;
}

void CapsaicinInternal::setPlayRewind(bool const rewind) noexcept
{
    play_rewind_ = rewind;
}

bool CapsaicinInternal::getPlayRewind() const noexcept
{
    return play_rewind_;
}

void CapsaicinInternal::setRenderPaused(bool const paused) noexcept
{
    render_paused_ = paused;
}

bool CapsaicinInternal::getRenderPaused() const noexcept
{
    return render_paused_;
}

bool CapsaicinInternal::getRenderDimensionsUpdated() const noexcept
{
    return render_dimensions_updated_;
}

bool CapsaicinInternal::getWindowDimensionsUpdated() const noexcept
{
    return window_dimensions_updated_;
}

bool CapsaicinInternal::getMeshesUpdated() const noexcept
{
    return mesh_updated_;
}

bool CapsaicinInternal::getTransformsUpdated() const noexcept
{
    return transform_updated_;
}

bool CapsaicinInternal::getInstancesUpdated() const noexcept
{
    return instances_updated_;
}

bool CapsaicinInternal::getSceneUpdated() const noexcept
{
    return scene_updated_;
}

bool CapsaicinInternal::getCameraChanged() const noexcept
{
    return camera_changed_;
}

bool CapsaicinInternal::getCameraUpdated() const noexcept
{
    return camera_updated_;
}

bool CapsaicinInternal::getAnimationUpdated() const noexcept
{
    return animation_updated_;
}

bool CapsaicinInternal::getEnvironmentMapUpdated() const noexcept
{
    return environment_map_updated_;
}

vector<string_view> CapsaicinInternal::getSharedTextures() const noexcept
{
    vector<string_view> textures;
    for (auto const &i : shared_textures_)
    {
        textures.emplace_back(i.first);
    }
    return textures;
}

bool CapsaicinInternal::hasSharedTexture(string_view const &texture) const noexcept
{
    return ranges::any_of(shared_textures_, [&texture](auto const &item) { return item.first == texture; });
}

bool CapsaicinInternal::checkSharedTexture(
    string_view const &texture, uint2 const dimensions, uint32_t const mips)
{
    if (auto const i =
            ranges::find_if(shared_textures_, [&texture](auto const &item) { return item.first == texture; });
        i != shared_textures_.end())
    {
        uint2      checkDim = dimensions;
        bool const autoSize = any(equal(dimensions, uint2(0)));
        if (autoSize)
        {
            checkDim = render_dimensions_;
        }
        if (i->second.getWidth() != checkDim.x || i->second.getHeight() != checkDim.y)
        {
            auto const        format = i->second.getFormat();
            auto const *const name   = i->second.getName();
            GfxTexture        newTexture;
            constexpr float   clearValue[] = {0.0F, 0.0F, 0.0F, 0.0F};
            if (autoSize)
            {
                newTexture = gfxCreateTexture2D(gfx_, render_dimensions_.x, render_dimensions_.y, format,
                    mips, (format != DXGI_FORMAT_D32_FLOAT) ? nullptr : clearValue,
                    (format != DXGI_FORMAT_D32_FLOAT)
                        ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS | D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET
                        : D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
            }
            else
            {
                newTexture = gfxCreateTexture2D(gfx_, dimensions.x, dimensions.y, format, mips,
                    (format != DXGI_FORMAT_D32_FLOAT) ? nullptr : clearValue,
                    (format != DXGI_FORMAT_D32_FLOAT)
                        ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS | D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET
                        : D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
            }
            newTexture.setName(name);
            gfxDestroyTexture(gfx_, i->second);
            i->second = newTexture;
            return !!i->second;
        }
        return true;
    }
    return false;
}

GfxTexture const &CapsaicinInternal::getSharedTexture(string_view const &texture) const noexcept
{
    if (auto const i =
            ranges::find_if(shared_textures_, [&texture](auto const &item) { return item.first == texture; });
        i != shared_textures_.cend())
    {
        return i->second;
    }
    GFX_PRINTLN("Error: Unknown VAO requested: %s", texture.data());
    static GfxTexture const invalidReturn;
    return invalidReturn;
}

vector<string_view> CapsaicinInternal::getDebugViews() const noexcept
{
    vector<string_view> views;
    for (auto const &i : debug_views_)
    {
        views.emplace_back(i.first);
    }
    return views;
}

bool CapsaicinInternal::checkDebugViewSharedTexture(string_view const &view) const noexcept
{
    if (auto const i =
            ranges::find_if(debug_views_, [&view](auto const &item) { return item.first == view; });
        i != debug_views_.cend())
    {
        return !i->second;
    }
    GFX_PRINTLN("Error: Unknown debug view requested: %s", view.data());
    return false;
}

bool CapsaicinInternal::hasSharedBuffer(string_view const &buffer) const noexcept
{
    return ranges::any_of(shared_buffers_, [&buffer](auto const &item) { return item.first == buffer; });
}

bool CapsaicinInternal::checkSharedBuffer(
    string_view const &buffer, uint64_t const size, bool const exactSize, bool const copy)
{
    if (auto const i =
            ranges::find_if(shared_buffers_, [&buffer](auto const &item) { return item.first == buffer; });
        i != shared_buffers_.end())
    {
        if (exactSize ? i->second.getSize() == size : i->second.getSize() >= size)
        {
            return true;
        }
        auto const *const name      = i->second.getName();
        auto const        stride    = i->second.getStride();
        GfxBuffer         newBuffer = gfxCreateBuffer(gfx_, size);
        if (copy)
        {
            gfxCommandCopyBuffer(gfx_, newBuffer, 0, i->second, 0, i->second.getSize());
        }
        newBuffer.setName(name);
        newBuffer.setStride(stride);
        gfxDestroyBuffer(gfx_, i->second);
        i->second = newBuffer;
        return !!i->second;
    }
    return false;
}

GfxBuffer const &CapsaicinInternal::getSharedBuffer(string_view const &buffer) const noexcept
{
    if (auto const i =
            ranges::find_if(shared_buffers_, [&buffer](auto const &item) { return item.first == buffer; });
        i != shared_buffers_.cend())
    {
        return i->second;
    }
    GFX_PRINTLN("Error: Unknown buffer requested: %s", buffer.data());
    static GfxBuffer const invalidReturn;
    return invalidReturn;
}

bool CapsaicinInternal::hasComponent(string_view const &component) const noexcept
{
    return ranges::any_of(components_, [&component](auto const &item) { return item.first == component; });
}

shared_ptr<Component> const &CapsaicinInternal::getComponent(string_view const &component) const noexcept
{
    if (auto const i =
            ranges::find_if(components_, [&component](auto const &item) { return item.first == component; });
        i != components_.end())
    {
        return i->second;
    }
    GFX_PRINTLN("Error: Unknown component requested: %s", component.data());
    static shared_ptr<Component> const nullReturn;
    return nullReturn;
}

vector<string_view> CapsaicinInternal::GetRenderers() noexcept
{
    return RendererFactory::getNames();
}

string_view CapsaicinInternal::getCurrentRenderer() const noexcept
{
    return renderer_name_;
}

bool CapsaicinInternal::setRenderer(string_view const &name) noexcept
{
    auto const renderers = RendererFactory::getNames();
    auto const renderer  = ranges::find_if(renderers, [&name](auto val) { return name == val; });
    if (renderer == renderers.cend())
    {
        GFX_PRINTLN("Error: Requested invalid renderer: %s", name.data());
        return false;
    }
    if (renderer_ != nullptr)
    {
        renderer_      = nullptr;
        renderer_name_ = "";
    }
    frameGraph.reset();
    return setupRenderTechniques(*renderer);
}

string_view CapsaicinInternal::getCurrentDebugView() const noexcept
{
    return debug_view_;
}

bool CapsaicinInternal::setDebugView(string_view const &name) noexcept
{
    auto const debugView =
        ranges::find_if(as_const(debug_views_), [&name](auto val) { return name == val.first; });
    if (debugView == debug_views_.cend())
    {
        GFX_PRINTLN("Error: Requested invalid debug view: %s", name.data());
        return false;
    }
    debug_view_ = debugView->first;
    return true;
}

RenderOptionList const &CapsaicinInternal::getOptions() const noexcept
{
    return options_;
}

RenderOptionList &CapsaicinInternal::getOptions() noexcept
{
    return options_;
}

CameraMatrices const &CapsaicinInternal::getCameraMatrices(bool const jittered) const
{
    return camera_matrices_[jittered];
}

GfxBuffer CapsaicinInternal::getCameraMatricesBuffer(bool const jittered) const
{
    return camera_matrices_buffer_[jittered];
}

float2 CapsaicinInternal::getCameraJitter() const noexcept
{
    return camera_jitter_;
}

uint32_t CapsaicinInternal::getCameraJitterPhase() const noexcept
{
    return jitter_phase_count_;
}

void CapsaicinInternal::stepJitterFrameIndex(uint32_t const frames) noexcept
{
    if (uint32_t const remaining_frames = numeric_limits<uint32_t>::max() - jitter_frame_index_;
        frames < remaining_frames)
    {
        jitter_frame_index_ += frames;
    }
    else
    {
        jitter_frame_index_ = frames - remaining_frames;
    }
}

void CapsaicinInternal::setCameraJitterPhase(uint32_t const length) noexcept
{
    jitter_phase_count_ = length;
}

uint32_t CapsaicinInternal::getDeltaLightCount() const noexcept
{
    if (hasComponent("LightBuilder"))
    {
        return getComponent<LightBuilder>()->getDeltaLightCount();
    }
    return 0;
}

uint32_t CapsaicinInternal::getAreaLightCount() const noexcept
{
    if (hasComponent("LightBuilder"))
    {
        return getComponent<LightBuilder>()->getAreaLightCount();
    }
    return 0;
}

uint32_t CapsaicinInternal::getEnvironmentLightCount() const noexcept
{
    if (hasComponent("LightBuilder"))
    {
        return getComponent<LightBuilder>()->getEnvironmentLightCount();
    }
    return 0;
}

uint32_t CapsaicinInternal::getTriangleCount() const noexcept
{
    return triangle_count_;
}

uint64_t CapsaicinInternal::getBvhDataSize() const noexcept
{
    uint64_t     bvh_data_size      = gfxAccelerationStructureGetDataSize(gfx_, acceleration_structure_);
    size_t const rt_primitive_count = raytracing_primitives_.size();

    for (size_t i = 0; i < rt_primitive_count; ++i)
    {
        auto const &rt_primitive = raytracing_primitives_[i];
        bvh_data_size += gfxRaytracingPrimitiveGetDataSize(gfx_, rt_primitive);
    }

    return bvh_data_size;
}

GfxBuffer CapsaicinInternal::getInstanceBuffer() const
{
    return instance_buffer_;
}

vector<Instance> const &CapsaicinInternal::getInstanceData() const
{
    return instance_data_;
}

GfxBuffer CapsaicinInternal::getInstanceIdBuffer() const
{
    return instance_id_buffer_;
}

vector<uint32_t> const &CapsaicinInternal::getInstanceIdData() const
{
    return instance_id_data_;
}

GfxBuffer CapsaicinInternal::getTransformBuffer() const
{
    return transform_buffer_;
}

GfxBuffer CapsaicinInternal::getPrevTransformBuffer() const
{
    return prev_transform_buffer_;
}

GfxBuffer CapsaicinInternal::getMaterialBuffer() const
{
    return material_buffer_;
}

vector<GfxTexture> const &CapsaicinInternal::getTextures() const
{
    return texture_atlas_;
}

GfxSamplerState CapsaicinInternal::getLinearSampler() const
{
    return linear_sampler_;
}

GfxSamplerState CapsaicinInternal::getLinearWrapSampler() const
{
    return linear_wrap_sampler_;
}

GfxSamplerState CapsaicinInternal::getNearestSampler() const
{
    return nearest_sampler_;
}

GfxSamplerState CapsaicinInternal::getAnisotropicSampler() const
{
    return anisotropic_sampler_;
}

GfxBuffer CapsaicinInternal::getIndexBuffer() const
{
    return index_buffer_;
}

GfxBuffer CapsaicinInternal::getVertexBuffer() const
{
    return vertex_buffer_;
}

GfxBuffer CapsaicinInternal::getVertexSourceBuffer() const
{
    return vertex_source_buffer_;
}

GfxBuffer CapsaicinInternal::getJointBuffer() const
{
    return joint_buffer_;
}

GfxBuffer CapsaicinInternal::getJointMatricesBuffer() const
{
    return joint_matrices_buffer_;
}

GfxBuffer CapsaicinInternal::getMorphWeightBuffer() const
{
    return morph_weight_buffer_;
}

uint32_t CapsaicinInternal::getVertexDataIndex() const
{
    return vertex_data_index_;
}

uint32_t CapsaicinInternal::getPrevVertexDataIndex() const
{
    return prev_vertex_data_index_;
}

uint32_t CapsaicinInternal::getRaytracingPrimitiveCount() const
{
    return static_cast<uint32_t>(raytracing_primitives_.size());
}

GfxAccelerationStructure CapsaicinInternal::getAccelerationStructure() const
{
    return acceleration_structure_;
}

uint32_t CapsaicinInternal::getSbtStrideInEntries(GfxShaderGroupType const type) const
{
    return sbt_stride_in_entries_[type];
}

pair<float3, float3> CapsaicinInternal::getSceneBounds() const
{
    // Calculate the scene bounds
    uint32_t const numInstance = gfxSceneGetObjectCount<GfxInstance>(scene_);
    float3         sceneMin(numeric_limits<float>::max());
    float3         sceneMax(numeric_limits<float>::lowest());
    for (uint i = 0; i < numInstance; ++i)
    {
        uint32_t const instanceIndex           = instance_id_data_[i];
        auto const &[instanceMin, instanceMax] = instance_bounds_[instanceIndex];
        float3 const minBounds                 = min(instanceMin, instanceMax);
        float3 const maxBounds                 = max(instanceMin, instanceMax);
        sceneMin                               = min(sceneMin, minBounds);
        sceneMax                               = max(sceneMax, maxBounds);
    }
    return make_pair(sceneMin, sceneMax);
}

GfxBuffer CapsaicinInternal::allocateConstantBuffer(uint64_t const size)
{
    GfxBuffer     &constant_buffer_pool        = constant_buffer_pools_[gfxGetBackBufferIndex(gfx_)];
    uint64_t const constant_buffer_pool_cursor = GFX_ALIGN(constant_buffer_pool_cursor_ + size, 256);

    if (constant_buffer_pool_cursor >= constant_buffer_pool.getSize())
    {
        gfxDestroyBuffer(gfx_, constant_buffer_pool);

        uint64_t constant_buffer_pool_size = constant_buffer_pool_cursor;
        constant_buffer_pool_size += (constant_buffer_pool_size + 2) >> 1;
        constant_buffer_pool_size = GFX_ALIGN(constant_buffer_pool_size, 65536);

        constant_buffer_pool = gfxCreateBuffer(gfx_, constant_buffer_pool_size, nullptr, kGfxCpuAccess_Write);

        char buffer[256];
        GFX_SNPRINTF(buffer, sizeof(buffer), "Capsaicin_ConstantBufferPool%u", gfxGetBackBufferIndex(gfx_));

        constant_buffer_pool.setName(buffer);
    }

    GfxBuffer const constant_buffer =
        gfxCreateBufferRange(gfx_, constant_buffer_pool, constant_buffer_pool_cursor_, size);

    constant_buffer_pool_cursor_ = constant_buffer_pool_cursor;

    return constant_buffer;
}

GfxTexture CapsaicinInternal::createRenderTexture(
    const DXGI_FORMAT format, string_view const &name, uint32_t mips, float const scale) const noexcept
{
    auto const dimensions = scale == 1.0F ? render_dimensions_ : uint2(float2(render_dimensions_) * scale);
    mips = mips == UINT_MAX ? max(gfxCalculateMipCount(dimensions.x, dimensions.y), 1U) : mips;
    constexpr float clear[] = {0.0F, 0.0F, 0.0F, 0.0F};
    auto            ret     = gfxCreateTexture2D(gfx_, dimensions.x, dimensions.y, format, mips,
        (format != DXGI_FORMAT_D32_FLOAT) ? nullptr : clear,
        (format != DXGI_FORMAT_D32_FLOAT)
                           ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS | D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET
                           : D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
    ret.setName(name.data());
    return ret;
}

GfxTexture CapsaicinInternal::resizeRenderTexture(
    GfxTexture const &texture, bool const clear, uint32_t mips, float const scale) const noexcept
{
    auto const        format = texture.getFormat();
    auto const *const name   = texture.getName();
    auto const dimensions    = scale == 1.0F ? render_dimensions_ : uint2(float2(render_dimensions_) * scale);
    mips                     = (mips == UINT_MAX || (mips == 0 && texture.getMipLevels() > 1))
                                 ? gfxCalculateMipCount(dimensions.x, dimensions.y)
                                 : ((mips == 0) ? 1 : mips);
    auto ret = gfxCreateTexture2D(gfx_, dimensions.x, dimensions.y, format, mips, texture.getClearValue(),
        (format != DXGI_FORMAT_D32_FLOAT)
            ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS | D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET
            : D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
    ret.setName(name);
    gfxDestroyTexture(gfx_, texture);
    if (clear)
    {
        gfxCommandClearTexture(gfx_, ret);
    }
    return ret;
}

GfxTexture CapsaicinInternal::createWindowTexture(
    const DXGI_FORMAT format, string_view const &name, uint32_t mips, float const scale) const noexcept
{
    auto const dimensions = scale == 1.0F ? window_dimensions_ : uint2(float2(window_dimensions_) * scale);
    mips = mips == UINT_MAX ? max(gfxCalculateMipCount(dimensions.x, dimensions.y), 1U) : mips;
    constexpr float clearValue[] = {0.0F, 0.0F, 0.0F, 0.0F};
    auto            ret          = gfxCreateTexture2D(gfx_, dimensions.x, dimensions.y, format, mips,
        (format != DXGI_FORMAT_D32_FLOAT) ? nullptr : clearValue,
        (format != DXGI_FORMAT_D32_FLOAT)
                                ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS | D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET
                                : D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
    ret.setName(name.data());
    return ret;
}

GfxTexture CapsaicinInternal::resizeWindowTexture(
    GfxTexture const &texture, bool const clear, uint32_t mips, float const scale) const noexcept
{
    auto const        format = texture.getFormat();
    auto const *const name   = texture.getName();
    auto const dimensions    = scale == 1.0F ? window_dimensions_ : uint2(float2(window_dimensions_) * scale);
    mips                     = (mips == UINT_MAX || (mips == 0 && texture.getMipLevels() > 1))
                                 ? gfxCalculateMipCount(dimensions.x, dimensions.y)
                                 : ((mips == 0) ? 1 : mips);
    auto ret = gfxCreateTexture2D(gfx_, dimensions.x, dimensions.y, format, mips, texture.getClearValue(),
        (format != DXGI_FORMAT_D32_FLOAT)
            ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS | D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET
            : D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
    ret.setName(name);
    gfxDestroyTexture(gfx_, texture);
    if (clear)
    {
        gfxCommandClearTexture(gfx_, ret);
    }
    return ret;
}

GfxProgram CapsaicinInternal::createProgram(char const *file_name) const noexcept
{
    auto const  shaderPaths      = getShaderPaths();
    // New include paths can be added here.
    char const *include_paths[3] = {shaderPaths[0].c_str(), shaderPaths[1].c_str(), shaderPaths[2].c_str()};
    return gfxCreateProgram(gfx_, file_name, shader_path_.c_str(), nullptr, include_paths, 3U);
}

void CapsaicinInternal::dispatchKernel(GfxKernel const &kernel, uint2 const dimensions) const noexcept
{
    uint32_t const *num_threads = gfxKernelGetNumThreads(gfx_, kernel);
    auto const      num_groups  = uint2((dimensions.x + num_threads[0] - 1) / num_threads[0],
              (dimensions.y + num_threads[1] - 1) / num_threads[1]);

    gfxCommandBindKernel(gfx_, kernel);
    gfxCommandDispatch(gfx_, num_groups.x, num_groups.y, 1);
}

void CapsaicinInternal::initialize(GfxContext const &gfx, ImGuiContext *imgui_context)
{
    if (!gfx)
    {
        return; // invalid graphics context
    }

    if (gfx_)
    {
        terminate();
    }

    {
        linear_sampler_      = gfxCreateSamplerState(gfx, D3D12_FILTER_MIN_MAG_MIP_LINEAR);
        linear_wrap_sampler_ = gfxCreateSamplerState(gfx, D3D12_FILTER_MIN_MAG_MIP_LINEAR,
            D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP);
        nearest_sampler_     = gfxCreateSamplerState(gfx, D3D12_FILTER_MIN_MAG_MIP_POINT);
        anisotropic_sampler_ = gfxCreateSamplerState(
            gfx, D3D12_FILTER_ANISOTROPIC, D3D12_TEXTURE_ADDRESS_MODE_WRAP, D3D12_TEXTURE_ADDRESS_MODE_WRAP);
    }
    shader_path_             = "src/core/src/";
    third_party_shader_path_ = "third_party/";
    // Check if shader source can be found
    error_code ec;
    bool       found = false;
    for (uint32_t i = 0; i < 4; ++i)
    {
        if (filesystem::exists(shader_path_ + "gpu_shared.h", ec))
        {
            found = true;
            break;
        }
        shader_path_.insert(0, "../");
        third_party_shader_path_.insert(0, "../");
    }
    if (!found)
    {
        GFX_PRINTLN("Could not find directory containing shader source files");
        return;
    }

    sbt_stride_in_entries_[kGfxShaderGroupType_Raygen]   = 1;
    sbt_stride_in_entries_[kGfxShaderGroupType_Miss]     = 2;
    sbt_stride_in_entries_[kGfxShaderGroupType_Hit]      = 2;
    sbt_stride_in_entries_[kGfxShaderGroupType_Callable] = 1;

    gfx_ = gfx;

    blit_program_ = createProgram("capsaicin/blit");
    blit_kernel_  = gfxCreateGraphicsKernel(gfx, blit_program_);

    generate_animated_vertices_program_ = createProgram("capsaicin/generate_animated_vertices");
    generate_animated_vertices_kernel_  = gfxCreateComputeKernel(gfx, generate_animated_vertices_program_);

    window_dimensions_ = uint2(gfxGetBackBufferWidth(gfx), gfxGetBackBufferHeight(gfx));
    setRenderDimensionsScale(render_scale_);

    ImGui::SetCurrentContext(imgui_context);
}

void CapsaicinInternal::render()
{
    // Update current frame time
    auto const previousTime = current_time_;
    auto const wallTime =
        chrono::duration_cast<chrono::microseconds>(chrono::high_resolution_clock::now().time_since_epoch());
    current_time_ = static_cast<double>(wallTime.count()) / 1000000.0;
    frame_time_   = current_time_ - previousTime;

    // Check if manual frame increment/decrement has been applied
    if (bool const manual_play = play_time_ != play_time_old_;
        !render_paused_ || manual_play || frame_index_ == numeric_limits<uint32_t>::max())
    {
        // Start a new frame
        ++frame_index_;
        // Handle frame index wraparound by ensuring it never wraps back to zero
        // We wrap starting at UINT_MAX-1 to ensure index only equals UINT_MAX on startup
        if (frame_index_ == numeric_limits<uint32_t>::max())
        {
            frame_index_ = 1;
        }

        frameGraph.addValue(frame_time_);

        constant_buffer_pool_cursor_ = 0;
        auto const currentWindow     = uint2(gfxGetBackBufferWidth(gfx_), gfxGetBackBufferHeight(gfx_));
        window_dimensions_updated_   = window_dimensions_ != currentWindow;
        window_dimensions_           = currentWindow;
        if (window_dimensions_updated_)
        {
            // Update render dimensions
            setRenderDimensionsScale(render_scale_);
        }

        // Update the shared texture history
        if (!render_dimensions_updated_)
        {
            GfxCommandEvent const command_event(gfx_, "UpdatePreviousSharedTextures");

            for (auto const &i : backup_shared_textures_)
            {
                gfxCommandCopyTexture(
                    gfx_, shared_textures_[i.second].second, shared_textures_[i.first].second);
            }
        }

        // Clear our shared textures/buffers
        {
            GfxCommandEvent const command_event(gfx_, "ClearGBuffers");

            if (!render_dimensions_updated_)
            {
                for (auto const &i : clear_shared_buffers_)
                {
                    gfxCommandClearBuffer(gfx_, shared_buffers_[i].second);
                }

                for (auto const &i : clear_shared_textures_)
                {
                    gfxCommandClearTexture(gfx_, shared_textures_[i].second);
                }

                if (!debug_view_.empty() && debug_view_ != "None")
                {
                    gfxCommandClearTexture(gfx_, getSharedTexture("Debug"));
                }
            }
            else
            {
                for (auto &i : shared_textures_)
                {
                    if (i.first != "ColorScaled")
                    {
                        // Render size textures scale.
                        i.second = resizeRenderTexture(i.second);
                    }
                    else
                    {
                        // Display size textures scale.
                        i.second = resizeWindowTexture(i.second);
                    }
                }
            }
        }

        // Update the scene state, i.e. simulation.
        updateScene();

        // Update the components
        for (auto const &component : components_)
        {
            component.second->setGfxContext(gfx_);
            component.second->resetQueries();
            {
                Component::TimedSection const timed_section(*component.second, component.second->getName());
                component.second->run(*this);
            }
        }

        // Execute our render techniques
        for (auto const &render_technique : render_techniques_)
        {
            render_technique->setGfxContext(gfx_);
            render_technique->resetQueries();
            {
                RenderTechnique::TimedSection const timed_section(
                    *render_technique, render_technique->getName());
                render_technique->render(*this);
            }
        }

        // Reset all update flags
        render_dimensions_updated_ = false;
        window_dimensions_updated_ = false;
        mesh_updated_              = false;
        transform_updated_         = false;
        environment_map_updated_   = false;
        scene_updated_             = false;
        camera_changed_            = false;
        camera_updated_            = false;
        animation_updated_         = false;
    }

    // Show debug visualizations if requested or blit Color AOV
    currentView =
        hasSharedTexture("ColorScaled") && hasOption<bool>("taa_enable") && getOption<bool>("taa_enable")
            ? getSharedTexture("ColorScaled")
            : getSharedTexture("Color");
    if (!debug_view_.empty() && debug_view_ != "None")
    {
        if (auto const debugView = ranges::find_if(
                as_const(debug_views_), [this](auto val) { return debug_view_ == val.first; });
            debugView == debug_views_.cend())
        {
            GFX_PRINTLN("Error: Invalid debug view requested: %s", debug_view_.data());
            GfxCommandEvent const command_event(gfx_, "DrawInvalidDebugView");
            gfxCommandClearBackBuffer(gfx_);
        }
        else if (!debugView->second || debug_view_ == "Depth")
        {
            // Output shared texture
            if (auto const &texture = getSharedTexture(debugView->first);
                texture.getFormat() == DXGI_FORMAT_D32_FLOAT
                || (texture.getFormat() == DXGI_FORMAT_R32_FLOAT
                    && (strstr(texture.getName(), "Depth") != nullptr
                        || strstr(texture.getName(), "depth") != nullptr)))
            {
                auto const &debug_texture = getSharedTexture("Debug");
                if (!debug_depth_kernel_)
                {
                    debug_depth_program_    = createProgram("capsaicin/debug_depth");
                    GfxDrawState const draw = {};
                    gfxDrawStateSetColorTarget(draw, 0, debug_texture.getFormat());
                    debug_depth_kernel_ = gfxCreateGraphicsKernel(gfx_, debug_depth_program_, draw);
                }
                {
                    GfxCommandEvent const command_event(gfx_, "DrawDepthDebugView");
                    gfxProgramSetParameter(gfx_, debug_depth_program_, "DepthBuffer", texture);
                    auto const  &camera = getCamera();
                    float2 const nearFar(camera.nearZ, camera.farZ);
                    gfxProgramSetParameter(gfx_, debug_depth_program_, "g_NearFar", nearFar);
                    gfxCommandBindColorTarget(gfx_, 0, debug_texture);
                    gfxCommandBindKernel(gfx_, debug_depth_kernel_);
                    gfxCommandDraw(gfx_, 3);
                }
                currentView = debug_texture;
            }
            else
            {
                // If tone-mapping is enabled then we allow it to tonemap the shared texture into the Debug
                // buffer and then output from there
                if (auto const format = texture.getFormat();
                    hasOption<bool>("tonemap_enable") && getOption<bool>("tonemap_enable")
                    && (format == DXGI_FORMAT_R32G32B32A32_FLOAT || format == DXGI_FORMAT_R32G32B32_FLOAT
                        || format == DXGI_FORMAT_R16G16B16A16_FLOAT || format == DXGI_FORMAT_R11G11B10_FLOAT))
                {
                    currentView = getSharedTexture("Debug");
                }
                else
                {
                    currentView = texture;
                }
            }
        }
        else
        {
            // Output debug AOV
            currentView = getSharedTexture("Debug");
        }
    }
    {
        // Display the current view to back buffer
        GfxCommandEvent const command_event(gfx_, "Display");

        gfxProgramSetTexture(gfx_, blit_program_, "g_InputBuffer", currentView);
        uint2 const inputResolution = uint2(currentView.getWidth(), currentView.getHeight());
        gfxProgramSetParameter(gfx_, blit_program_, "g_InputResolution", inputResolution);
        gfxProgramSetParameter(gfx_, blit_program_, "g_Scale",
            static_cast<float2>(inputResolution) / static_cast<float2>(window_dimensions_));
        gfxCommandBindKernel(gfx_, blit_kernel_);
        gfxCommandDraw(gfx_, 3);
    }

    // Dump buffers for past dump requests (takes X frames to be become available)
    uint32_t dump_available_buffer_count = 0;
    for (auto &dump_in_flight_buffer : dump_in_flight_buffers_)
    {
        if (uint32_t &dump_frame_index = get<5>(dump_in_flight_buffer); dump_frame_index == 0)
        {
            dump_available_buffer_count++;
        }
        else
        {
            --dump_frame_index;
        }
    }

    // Write out each available buffer in parallel
    concurrency::parallel_for(0U, dump_available_buffer_count, 1U, [&](uint32_t const buffer_index) {
        auto const &buffer = dump_in_flight_buffers_[buffer_index];
        saveImage(get<0>(buffer), get<1>(buffer), get<2>(buffer), get<3>(buffer), get<4>(buffer));
    });

    for (uint32_t available_buffer_index = 0; available_buffer_index < dump_available_buffer_count;
        available_buffer_index++)
    {
        gfxDestroyBuffer(gfx_, get<0>(dump_in_flight_buffers_.front()));
        dump_in_flight_buffers_.pop_front();
    }
}

void CapsaicinInternal::renderGUI(bool const readOnly)
{
    // Check if we have a functional UI context
    if (ImGui::GetCurrentContext() == nullptr)
    {
        static bool warned;
        if (!warned)
        {
            GFX_PRINT_ERROR(kGfxResult_InvalidOperation,
                "No ImGui context was supplied on initialization; cannot call `Capsaicin::RenderGUI()'");
        }
        warned = true;
        return; // no ImGui context was supplied on initialization
    }

    // Display scene specific statistics
    ImGui::Text("Selected device  :  %s", gfx_.getName());
    ImGui::Separator();
    uint32_t const deltaLightCount = getDeltaLightCount();
    uint32_t const areaLightCount  = getAreaLightCount();
    uint32_t const envLightCount   = getEnvironmentLightCount();
    uint32_t const triangleCount   = getTriangleCount();
    uint64_t const bvhDataSize     = getBvhDataSize();
    ImGui::Text("Triangle Count            :  %u", triangleCount);
    ImGui::Text("Light Count               :  %u", areaLightCount + deltaLightCount + envLightCount);
    ImGui::Text("  Area Light Count        :  %u", areaLightCount);
    ImGui::Text("  Delta Light Count       :  %u", deltaLightCount);
    ImGui::Text("  Environment Light Count :  %u", envLightCount);
    ImGui::Text(
        "BVH Data Size             :  %.1f MiB", static_cast<double>(bvhDataSize) / (1024.0 * 1024.0));
    ImGui::Text("Render Resolution         :  %ux%u", render_dimensions_.x, render_dimensions_.y);
    ImGui::Text("Window Resolution         :  %ux%u", window_dimensions_.x, window_dimensions_.y);
    auto const backBuffer = gfxGetBackBufferFormat(gfx_);
    ImGui::Text("Display format            :  %s",
        backBuffer == DXGI_FORMAT_R16G16B16A16_FLOAT
            ? "HDR16"
            : (backBuffer == DXGI_FORMAT_R8G8B8A8_UNORM
                      ? "SDR8"
                      : (gfxGetBackBufferColorSpace(gfx_) == DXGI_COLOR_SPACE_RGB_FULL_G2084_NONE_P2020
                                ? "HDR10"
                                : "SDR10")));

    if (!readOnly)
    {
        // Display renderer specific options, this is where any UI elements created by a render technique or
        // component will be displayed
        if (ImGui::CollapsingHeader("Renderer Settings", ImGuiTreeNodeFlags_DefaultOpen))
        {
            renderStockGUI();
            for (auto const &component : components_)
            {
                component.second->renderGUI(*this);
            }
            for (auto const &render_technique : render_techniques_)
            {
                render_technique->renderGUI(*this);
            }
        }
        ImGui::Separator();
    }

    // Display the profiling information
    if (ImGui::CollapsingHeader("Profiling", ImGuiTreeNodeFlags_DefaultOpen))
    {
        float totalTimestampTime = 0.0F;

        auto getTimestamps = [&](Timeable *timeable) -> void {
            // Check the current input for any timeable information
            uint32_t const timestampQueryCount = timeable->getTimestampQueryCount();
            if (timestampQueryCount == 0)
            {
                return; // skip if no profiling info available
            }

            bool const               hasChildren = timestampQueryCount > 1;
            ImGuiTreeNodeFlags const flags = hasChildren ? ImGuiTreeNodeFlags_None : ImGuiTreeNodeFlags_Leaf;
            auto const              &timestampQueries = timeable->getTimestampQueries();
            auto totalQueryDuration = gfxTimestampQueryGetDuration(gfx_, timestampQueries[0].query);

            // Add the current query duration to the total running count for later use
            totalTimestampTime += totalQueryDuration;

            if (timestampQueryCount > 1 && totalQueryDuration <= 1e-4F)
            {
                // Workaround for queries that were not called within the parent query (such as components
                // that are not executed as part of `run`). This results in duplicate times as the queries
                // were probably executed as part of another render technique so they should not be added to
                // the total
                float internalQueryDuration = 0.0F;
                for (uint32_t i = 1; i < timestampQueryCount; ++i)
                {
                    internalQueryDuration += gfxTimestampQueryGetDuration(gfx_, timestampQueries[i].query);
                }
#ifdef CAPSAICIN_ENABLE_HIP
                for (uint32_t i = 1; i < timestampQueryCountHIP; ++i)
                {
                    internalQueryDuration += timeableHIP.getEventDuration(i);
                }
#endif
                if (totalQueryDuration < internalQueryDuration)
                {
                    totalQueryDuration += internalQueryDuration;
                }
            }

            // Display tree of parent with any child timeable. We use a left padding of 25 chars as
            // this should fit any timeable name we currently use
            if (ImGui::TreeNodeEx(timeable->getName().data(), flags, "%-25s: %.3f ms",
                    timeable->getName().data(), static_cast<double>(totalQueryDuration)))
            {
                if (hasChildren)
                {
                    for (uint32_t i = 1; i < timestampQueryCount; ++i)
                    {
                        // Display child element. Children are inset 3 spaces so the left padding is
                        // reduced as a result
                        ImGui::TreeNodeEx(to_string(i).c_str(),
                            ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen, "%-22s: %.3f ms",
                            timestampQueries[i].name.data(),
                            static_cast<double>(
                                gfxTimestampQueryGetDuration(gfx_, timestampQueries[i].query)));
                    }
                }

                ImGui::TreePop();
            }
        };
        // Loop through all components and then all techniques in order and check for timeable information
        for (auto const &component : components_)
        {
            getTimestamps(&*component.second);
        }
        for (auto const &render_technique : render_techniques_)
        {
            getTimestamps(&*render_technique);
        }

        // Add final tree combined total
        if (ImGui::TreeNodeEx("Total", ImGuiTreeNodeFlags_Leaf, "%-25s: %.3f ms", "Total",
                static_cast<double>(totalTimestampTime)))
        {
            ImGui::TreePop();
        }
        ImGui::Separator();

        // Output total frame time, left padding is 3 more than was used for tree nodes due to tree
        // indentation
        ImGui::PushID("Total frame time");
        ImGui::Text("%-28s:", "Total frame time");

        // Add frame time graph
        ImGui::SameLine();
        string const graphName =
            format("{:.2f}", frame_time_ * 1000.0) + " ms (" + format("{:.2f}", 1.0 / frame_time_) + " fps)";
        ImGui::PlotLines("", Graph::GetValueAtIndex, &frameGraph,
            static_cast<int>(frameGraph.getValueCount()), 0, graphName.c_str(), 0.0F, FLT_MAX,
            ImVec2(175, 20));
        ImGui::PopID();

        // Output current frame number
        ImGui::PushID("Frame");
        ImGui::Text("%-28s:", "Frame");
        ImGui::SameLine();
        ImGui::Text("%s", to_string(frame_index_).c_str());
        ImGui::PopID();
    }

    if (!readOnly)
    {
        // As not all techniques/components will expose there settings in a visible way through their own
        // renderGUI functions we expose everything through a developer specific render options tree. This can
        // be used to modify any render option even if it isn't exposed in a more user-friendly way through
        // techniques/components renderGUI. This obviously then doesn't have nice naming or checks for invalid
        // inputs so it is assumed that this is for developer debugging purposes only
        if (ImGui::CollapsingHeader("Render Options", ImGuiTreeNodeFlags_None))
        {
            for (auto const &i : options_)
            {
                if (holds_alternative<bool>(i.second))
                {
                    auto value = *get_if<bool>(&i.second);
                    if (ImGui::Checkbox(i.first.data(), &value))
                    {
                        setOption(i.first, value);
                    }
                }
                else if (holds_alternative<uint32_t>(i.second))
                {
                    auto value = *get_if<uint32_t>(&i.second);
                    if (ImGui::DragInt(i.first.data(), reinterpret_cast<int32_t *>(&value), 1, 0,
                            std::numeric_limits<int32_t>::max(), "%d", ImGuiSliderFlags_AlwaysClamp))
                    {
                        setOption(i.first, value);
                    }
                }
                else if (holds_alternative<int32_t>(i.second))
                {
                    auto value = *get_if<int32_t>(&i.second);
                    if (ImGui::DragInt(i.first.data(), &value, 1))
                    {
                        setOption(i.first, value);
                    }
                }
                else if (holds_alternative<float>(i.second))
                {
                    auto value = *get_if<float>(&i.second);
                    if (ImGui::DragFloat(i.first.data(), &value, 5e-3F))
                    {
                        setOption(i.first, value);
                    }
                }
                else if (holds_alternative<string>(i.second))
                {
                    // ImGui needs a constant string buffer so that it can write data into it while a user is
                    // typing We only accept data once the user hits enter at which point we update our
                    // internal value. This requires a separate static string buffer to temporarily hold
                    // string storage
                    static map<string_view, array<char, 2048>> staticImguiStrings;
                    if (!staticImguiStrings.contains(i.first))
                    {
                        array<char, 2048> buffer {};
                        auto              value = *get_if<string>(&i.second);
                        strncpy_s(buffer.data(), buffer.size(), value.c_str(), value.size() + 1);
                        staticImguiStrings[i.first] = buffer;
                    }
                    else
                    {
                        // Check if string needs updating
                        if (auto value = *get_if<string>(&i.second);
                            string(staticImguiStrings[i.first].data()) != value)
                        {
                            strncpy_s(staticImguiStrings[i.first].data(), staticImguiStrings[i.first].size(),
                                value.c_str(), value.size() + 1);
                        }
                    }
                    if (ImGui::InputText(i.first.data(), staticImguiStrings[i.first].data(),
                            staticImguiStrings[i.first].size(),
                            ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll))
                    {
                        setOption(i.first, string(staticImguiStrings[i.first].data()));
                    }
                }
            }
        }
    }
}

void CapsaicinInternal::terminate() noexcept
{
    if (gfxContextIsValid(gfx_))
    {
        gfxFinish(gfx_);
        // Dump remaining buffers, they are all available after gfxFinish
        concurrency::parallel_for(
            0U, static_cast<uint32_t>(dump_in_flight_buffers_.size()), 1U, [&](uint32_t const buffer_index) {
                auto const &buffer = dump_in_flight_buffers_[buffer_index];
                saveImage(
                    get<0>(buffer), get<1>(buffer), get<2>(buffer), get<3>(buffer), get<4>(buffer).c_str());
            });

        while (!dump_in_flight_buffers_.empty())
        {
            gfxDestroyBuffer(gfx_, get<0>(dump_in_flight_buffers_.front()));
            dump_in_flight_buffers_.pop_front();
        }
    }

    render_techniques_.clear();
    components_.clear();
    renderer_ = nullptr;

    gfxDestroyKernel(gfx_, blit_kernel_);
    gfxDestroyProgram(gfx_, blit_program_);
    gfxDestroyKernel(gfx_, debug_depth_kernel_);
    gfxDestroyProgram(gfx_, debug_depth_program_);
    gfxDestroyKernel(gfx_, generate_animated_vertices_kernel_);
    gfxDestroyProgram(gfx_, generate_animated_vertices_program_);

    gfxDestroyBuffer(gfx_, camera_matrices_buffer_[0]);
    gfxDestroyBuffer(gfx_, camera_matrices_buffer_[1]);
    gfxDestroyBuffer(gfx_, index_buffer_);
    gfxDestroyBuffer(gfx_, vertex_buffer_);
    gfxDestroyBuffer(gfx_, vertex_source_buffer_);
    gfxDestroyBuffer(gfx_, instance_buffer_);
    gfxDestroyBuffer(gfx_, material_buffer_);
    gfxDestroyBuffer(gfx_, transform_buffer_);
    gfxDestroyBuffer(gfx_, instance_id_buffer_);
    gfxDestroyBuffer(gfx_, prev_transform_buffer_);
    gfxDestroyBuffer(gfx_, morph_weight_buffer_);
    gfxDestroyBuffer(gfx_, joint_buffer_);
    gfxDestroyBuffer(gfx_, joint_matrices_buffer_);

    gfxDestroyTexture(gfx_, environment_buffer_);

    gfxDestroySamplerState(gfx_, linear_sampler_);
    gfxDestroySamplerState(gfx_, linear_wrap_sampler_);
    gfxDestroySamplerState(gfx_, nearest_sampler_);
    gfxDestroySamplerState(gfx_, anisotropic_sampler_);

    destroyAccelerationStructure();

    for (auto const &i : shared_textures_)
    {
        gfxDestroyTexture(gfx_, i.second);
    }
    shared_textures_.clear();
    backup_shared_textures_.clear();
    clear_shared_textures_.clear();

    debug_views_.clear();

    for (auto const &i : shared_buffers_)
    {
        gfxDestroyBuffer(gfx_, i.second);
    }
    shared_buffers_.clear();

    for (GfxTexture const &texture : texture_atlas_)
    {
        gfxDestroyTexture(gfx_, texture);
    }
    texture_atlas_.clear();

    for (GfxBuffer const &constant_buffer_pool : constant_buffer_pools_)
    {
        gfxDestroyBuffer(gfx_, constant_buffer_pool);
    }
    memset(constant_buffer_pools_, 0, sizeof(constant_buffer_pools_));

    gfxDestroyScene(scene_);
    scene_ = {};
}

void CapsaicinInternal::reloadShaders() noexcept
{
    // Instead of just recompiling kernels we re-initialise all component/techniques. This has the side
    // effect of not only recompiling kernels but also re-initialising old data that may no longer contain
    // correct values
    gfxFinish(gfx_); // flush & sync

    // Reset the component/techniques
    for (auto const &i : components_)
    {
        i.second->setGfxContext(gfx_);
        i.second->terminate();
    }
    for (auto const &i : render_techniques_)
    {
        i->setGfxContext(gfx_);
        i->terminate();
    }

    resetPlaybackState();
    resetRenderState();

    // Re-initialise the components/techniques
    for (auto const &i : components_)
    {
        if (!i.second->init(*this))
        {
            GFX_PRINTLN("Error: Failed to initialise component: %s", i.first.data());
        }
    }
    for (auto const &i : render_techniques_)
    {
        if (!i->init(*this))
        {
            GFX_PRINTLN("Error: Failed to initialise render technique: %s", i->getName().data());
        }
    }
}

RenderOptionList CapsaicinInternal::getStockRenderOptions() noexcept
{
    RenderOptionList newOptions;
    newOptions.emplace(RENDER_OPTION_MAKE(capsaicin_lod_mode, render_options));
    newOptions.emplace(RENDER_OPTION_MAKE(capsaicin_lod_offset, render_options));
    newOptions.emplace(RENDER_OPTION_MAKE(capsaicin_lod_aggressive, render_options));
    newOptions.emplace(RENDER_OPTION_MAKE(capsaicin_mirror_roughness_threshold, render_options));
    return newOptions;
}

CapsaicinInternal::RenderOptions CapsaicinInternal::convertOptions(RenderOptionList const &options) noexcept
{
    RenderOptions newOptions;
    RENDER_OPTION_GET(capsaicin_lod_mode, newOptions, options)
    RENDER_OPTION_GET(capsaicin_lod_offset, newOptions, options)
    RENDER_OPTION_GET(capsaicin_lod_aggressive, newOptions, options)
    RENDER_OPTION_GET(capsaicin_mirror_roughness_threshold, newOptions, options)
    return newOptions;
}

ComponentList CapsaicinInternal::getStockComponents() const noexcept
{
    // Nothing to do here, available for future use
    return {};
}

SharedBufferList CapsaicinInternal::getStockSharedBuffers() const noexcept
{
    SharedBufferList ret;
    ret.push_back({.name = "Meshlets",
        .access          = SharedBuffer::Access::Write,
        .flags           = (SharedBuffer::Flags::Allocate | SharedBuffer::Flags::Optional),
        .size            = 0,
        .stride          = sizeof(Meshlet)});
    ret.push_back({.name = "MeshletPack",
        .access          = SharedBuffer::Access::Write,
        .flags           = (SharedBuffer::Flags::Allocate | SharedBuffer::Flags::Optional),
        .size            = 0,
        .stride          = sizeof(uint32_t)});
    ret.push_back({.name = "MeshletCull",
        .access          = SharedBuffer::Access::Write,
        .flags           = (SharedBuffer::Flags::Allocate | SharedBuffer::Flags::Optional),
        .size            = 0,
        .stride          = sizeof(MeshletCull)});
    return ret;
}

SharedTextureList CapsaicinInternal::getStockSharedTextures() const noexcept
{
    SharedTextureList ret;
    ret.push_back({.name = "Color",
        .flags           = SharedTexture::Flags::Accumulate,
        .format          = DXGI_FORMAT_R16G16B16A16_FLOAT});
    return ret;
}

DebugViewList CapsaicinInternal::getStockDebugViews() const noexcept
{
    if (hasSharedTexture("Depth"))
    {
        // We provide a custom shader for all depth based textures which we will use as default on the Depth
        // target
        return {"Depth"};
    }
    return {};
}

void CapsaicinInternal::renderStockGUI() noexcept
{
    // Nothing to do (yet)
}

void CapsaicinInternal::negotiateRenderTechniques() noexcept
{
    // Delete old shared textures and buffers
    for (auto const &i : shared_buffers_)
    {
        gfxDestroyBuffer(gfx_, i.second);
    }
    shared_buffers_.clear();
    clear_shared_buffers_.clear();
    for (auto const &i : shared_textures_)
    {
        gfxDestroyTexture(gfx_, i.second);
    }
    shared_textures_.clear();
    backup_shared_textures_.clear();
    clear_shared_textures_.clear();
    // Debug views must also be cleared as shared texture views may change after re-negotiation
    debug_views_.clear();
    debug_views_.emplace_back("None", nullptr);
    debug_view_ = "None";

    auto compileRequire = []<typename T>(string require, T const &existing) -> bool {
        require.erase(
            remove_if(require.begin(), require.end(), [](int const c) { return isspace(c); }), require.end());
        // Complex combinations of requires clauses require determining exact values
        constexpr auto symbols  = "!&|()";
        auto           startTag = require.find_first_not_of(symbols);
        while (startTag != string::npos)
        {
            // Get the next tag
            auto const endTag  = min(require.find_first_of(symbols, startTag), require.length());
            auto       tag     = string_view(&require[startTag], endTag - startTag);
            auto const replace = existing.contains(tag) ? "1" : "0";
            // Replace the clause with its value
            require.replace(startTag, endTag - startTag, replace);
            // Get next
            startTag = require.find_first_not_of(symbols, ++startTag);
        }
        // Process the string to combine values
        startTag = 0;
        while ((startTag = require.find_first_of("&|", startTag)) != std::string::npos)
        {
            ++startTag;
            if (require[startTag] == '&' || require[startTag] == '|')
            {
                require.erase(startTag, 1);
                ++startTag;
            }
        }
        function<void(string &)> compileRequireCollapse;
        compileRequireCollapse = [&compileRequireCollapse](string &requireString) -> void {
            // Need to search through operators in correct order of precedence
            for (constexpr array<char const, 3> operators = {'!', '&', '|'}; auto const &op : operators)
            {
                auto startTag2 = requireString.find(op);
                while (startTag2 != string::npos)
                {
                    // Get right tag
                    ++startTag2;
                    auto rightPos = requireString.find_first_of(symbols, startTag2);
                    // Skip any '(' found within the parameters itself
                    if ((rightPos != string::npos) && (requireString.at(rightPos) == '('))
                    {
                        auto back     = rightPos + 1;
                        rightPos      = requireString.find(')', back) + 1;
                        auto findPos3 = requireString.find('(', back);
                        while ((findPos3 != string::npos) && (findPos3 < rightPos))
                        {
                            findPos3 = requireString.find('(', findPos3 + 1);
                            rightPos = requireString.find(')', rightPos) + 1;
                        }
                        --back;
                        auto const length        = rightPos - back;
                        auto       subExpression = requireString.substr(back, length);
                        compileRequireCollapse(subExpression);
                        requireString.replace(back, length, subExpression);
                        rightPos -= length - subExpression.length();
                    }
                    auto const right = requireString[startTag2];
                    --startTag2;

                    // Check current operation
                    if (requireString.at(startTag2) == '!')
                    {
                        if (right == '0')
                        {
                            // !0 = 1
                            requireString.replace(startTag2, rightPos - startTag2, 1, '1');
                        }
                        else if (right == '1')
                        {
                            // !1 = 0
                            requireString.replace(startTag2, rightPos - startTag2, 1, '0');
                        }
                    }
                    else
                    {
                        // Get left tag
                        auto leftPos = requireString.find_last_of(symbols, startTag2 - 1);
                        // Skip any ')' found within the parameters itself
                        if ((leftPos != string::npos) && (requireString.at(leftPos) == ')'))
                        {
                            auto back     = leftPos - 1;
                            leftPos       = requireString.rfind('(', back);
                            auto findPos3 = requireString.rfind(')', back);
                            while ((findPos3 != string::npos) && (findPos3 > leftPos))
                            {
                                findPos3 = requireString.rfind(')', findPos3 - 1);
                                leftPos  = requireString.rfind('(', leftPos - 1);
                            }
                            back += 2;
                            auto const length        = back - leftPos;
                            auto       subExpression = requireString.substr(leftPos, length);
                            compileRequireCollapse(subExpression);
                            requireString.replace(leftPos, length, subExpression);
                            rightPos -= length - subExpression.length();
                        }
                        else
                        {
                            leftPos = (leftPos == string::npos) ? 0 : leftPos + 1;
                        }
                        auto const left = requireString[leftPos];

                        // Check current operation
                        if (op == '&')
                        {
                            requireString.replace(leftPos, rightPos - leftPos, 1,
                                ((left == '1') && (right == '1')) ? '1' : '0');
                        }
                        else if (op == '|')
                        {
                            requireString.replace(leftPos, rightPos - leftPos, 1,
                                ((left == '1') || (right == '1')) ? '1' : '0');
                        }
                        startTag2 = leftPos;
                    }
                    // Get next
                    startTag2 = requireString.find(op, startTag2);
                }
            }
            size_t find = 0;
            while ((find = requireString.find("(0)", find)) != string::npos)
            {
                requireString.replace(find, 3, "0");
            }
            find = 0;
            while ((find = requireString.find("(1)", find)) != string::npos)
            {
                requireString.replace(find, 3, "1");
            }
        };
        compileRequireCollapse(require);
        return require == "1";
    };
    auto combineRequire = [](string &update, string const &params) {
        if (update != params && !params.empty())
        {
            if (!update.empty() && update.find_first_of("&|") != string::npos)
            {
                update = "(" + update + ")";
            }
            update += "&";
            bool const brace = (params.find_first_of("&|") != string::npos);
            if (brace)
            {
                update += '(';
            }
            update += params;
            if (brace)
            {
                update += ')';
            }
        }
    };

    {
        // Get requested buffers
        struct BufferParams
        {
            BitMask<SharedBuffer::Flags> flags  = SharedBuffer::Flags::None;
            size_t                       size   = 0;
            uint32_t                     stride = 0;
        };

        struct OptionalBufferParams : BufferParams
        {
            string require = "";
        };

        using BufferList = unordered_map<string_view, BufferParams>;
        BufferList                                       requestedBuffers;
        unordered_map<string_view, OptionalBufferParams> optionalBuffers;
        vector<pair<string_view, OptionalBufferParams>>  optionalDependentBuffers;

        auto combineBuffersFunc = [&](BufferParams &update, BufferParams const &bufferParams,
                                      string_view const &bufferName) {
            // Update existing size if it doesn't have one
            if (update.size == 0)
            {
                update.size = bufferParams.size;
            }
            // Validate that requested values match the existing ones
            else if (bufferParams.size != update.size && bufferParams.size != 0)
            {
                GFX_PRINTLN("Error: Requested shared buffer with different sizes: %s", bufferName.data());
            }
            // Do the same for stride
            if (update.stride == 0)
            {
                update.stride = bufferParams.stride;
            }
            else if (bufferParams.stride != update.stride && bufferParams.stride != 0)
            {
                GFX_PRINTLN("Error: Requested shared buffer with different strides: %s", bufferName.data());
            }
            if (((bufferParams.flags & SharedBuffer::Flags::Clear)
                    && (update.flags & SharedBuffer::Flags::Accumulate))
                || ((bufferParams.flags & SharedBuffer::Flags::Accumulate)
                    && (update.flags & SharedBuffer::Flags::Clear)))
            {
                GFX_PRINTLN(
                    "Error: Requested shared buffer with different clear settings: %s", bufferName.data());
            }
            // Add clear/accumulate flag if requested
            if (bufferParams.flags & SharedBuffer::Flags::Clear)
            {
                update.flags = (update.flags | SharedBuffer::Flags::Clear);
            }
            else if (bufferParams.flags & SharedBuffer::Flags::Accumulate)
            {
                update.flags = (update.flags | SharedBuffer::Flags::Accumulate);
            }
            else if (bufferParams.flags & SharedBuffer::Flags::Allocate)
            {
                update.flags = (update.flags | SharedBuffer::Flags::Allocate);
            }
            // Update extra optional flags as needed
            if (bufferParams.flags & SharedBuffer::Flags::OptionalDiscard)
            {
                update.flags = update.flags | SharedBuffer::Flags::OptionalDiscard;
            }
            if (bufferParams.flags & SharedBuffer::Flags::OptionalKeep)
            {
                update.flags = update.flags | SharedBuffer::Flags::OptionalKeep;
            }
        };

        auto addBuffersFunc = [&](string_view const &name, BufferParams const &newParams) {
            if (auto const pos = requestedBuffers.find(name); pos == requestedBuffers.end())
            {
                // Add the new shared texture to requested list
                requestedBuffers.try_emplace(std::move(name), std::move(newParams));
            }
            else
            {
                // Merge with existing
                combineBuffersFunc(pos->second, newParams, name);
            }
        };

        auto bufferFunc = [&](SharedBuffer const &buf) {
            auto newParams = BufferParams {.flags = buf.flags, .size = buf.size, .stride = buf.stride};
            if (auto const found = requestedBuffers.find(buf.name); found == requestedBuffers.end())
            {
                // Check if the shared buffer is being read despite never having been written to
                if (buf.access == SharedBuffer::Access::Read && !optionalBuffers.contains(buf.name)
                    && (buf.flags & SharedBuffer::Flags::Clear))
                {
                    GFX_PRINTLN(
                        "Error: Requested read access to shared buffer that has not been written to: %s",
                        buf.name.data());
                }
                bool addBuffer = false;
                if (newParams.flags & SharedBuffer::Flags::Optional
                    || newParams.flags & SharedBuffer::Flags::OptionalDiscard
                    || newParams.flags & SharedBuffer::Flags::OptionalKeep)
                {
                    if (auto const pos = optionalBuffers.find(buf.name);
                        buf.access != SharedBuffer::Access::Read)
                    {
                        if (pos != optionalBuffers.end() && buf.access != SharedBuffer::Access::ReadWrite)
                        {
                            GFX_PRINTLN(
                                "Error: Found multiple writes to same optional buffer: %s", buf.name.data());
                        }
                        else if (buf.access != SharedBuffer::Access::Write
                                 && (newParams.flags & SharedBuffer::Flags::Clear))
                        {
                            GFX_PRINTLN(
                                "Error: Requested read access to optional shared buffer that has not been written to: %s",
                                buf.name.data());
                        }
                        if ((newParams.flags & SharedBuffer::Flags::OptionalKeep)
                            || (pos != optionalBuffers.end()
                                && pos->second.flags & SharedBuffer::Flags::OptionalKeep))
                        {
                            if (buf.require.empty()
                                && (pos == optionalBuffers.end() || pos->second.require.empty()))
                            {
                                addBuffer = true;
                            }
                            else
                            {
                                OptionalBufferParams reqParams = {newParams, string(buf.require)};
                                if (pos != optionalBuffers.end())
                                {
                                    combineRequire(reqParams.require, pos->second.require);
                                    pos->second.require.clear();
                                }
                                if (auto const find = ranges::find_if(optionalDependentBuffers,
                                        [&](auto const &val) { return val.first == buf.name; });
                                    find == optionalDependentBuffers.end())
                                {
                                    optionalDependentBuffers.emplace_back(buf.name.data(), reqParams);
                                }
                                else
                                {
                                    combineBuffersFunc(find->second, newParams, buf.name);
                                    combineRequire(find->second.require, reqParams.require);
                                }
                            }
                        }
                        if (pos != optionalBuffers.end())
                        {
                            // Merge with existing
                            combineBuffersFunc(pos->second, newParams, buf.name);
                        }
                        else
                        {
                            // Add to list of optional buffers
                            optionalBuffers.try_emplace(buf.name, newParams);
                        }
                    }
                    else if (pos != optionalBuffers.end())
                    {
                        // Check if connection can be made
                        if (buf.require.empty() && pos->second.require.empty())
                        {
                            if ((!(newParams.flags & SharedBuffer::Flags::OptionalDiscard)
                                    && !(pos->second.flags & SharedBuffer::Flags::OptionalDiscard))
                                || (newParams.flags & SharedBuffer::Flags::OptionalKeep
                                    || (pos->second.flags & SharedBuffer::Flags::OptionalKeep)))
                            {
                                addBuffer = true;
                            }
                            else
                            {
                                combineBuffersFunc(pos->second, newParams, buf.name);
                            }
                        }
                        else
                        {
                            // This is a dependent buffers, connecting must be delayed until all other
                            // connections are made
                            combineBuffersFunc(pos->second, newParams, buf.name);
                            OptionalBufferParams reqParams = {newParams, string(buf.require)};
                            if (!pos->second.require.empty() && !buf.require.empty())
                            {
                                combineRequire(reqParams.require, pos->second.require);
                            }
                            pos->second.require.clear();
                            if (auto const find = ranges::find_if(optionalDependentBuffers,
                                    [&](auto const &val) { return val.first == buf.name; });
                                find == optionalDependentBuffers.end())
                            {
                                optionalDependentBuffers.emplace_back(buf.name.data(), reqParams);
                            }
                            else
                            {
                                combineBuffersFunc(find->second, newParams, buf.name);
                                combineRequire(find->second.require, reqParams.require);
                            }
                        }
                    }
                }
                else
                {
                    addBuffer = true;
                }
                if (addBuffer)
                {
                    addBuffersFunc(buf.name, newParams);
                }
            }
            else
            {
                combineBuffersFunc(found->second, newParams, buf.name);
            }
        };
        // Check any internal shared buffers first
        for (auto &j : getStockSharedBuffers())
        {
            bufferFunc(j);
        }

        // Loop through all render techniques and components and check their requested shared buffers
        for (auto const &i : render_techniques_)
        {
            for (auto &j : i->getSharedBuffers())
            {
                bufferFunc(j);
            }
        }
        for (auto const &i : components_)
        {
            for (auto &j : i.second->getSharedBuffers())
            {
                bufferFunc(j);
            }
        }

        // Merge optional shared buffers
        for (auto &[bufferName, bufferParams] : optionalBuffers)
        {
            if (auto j = requestedBuffers.find(bufferName); j != requestedBuffers.end())
            {
                combineBuffersFunc(j->second, bufferParams, bufferName);
            }
        }

        // Perform optional buffer dependent checks
        for (auto &buf : optionalDependentBuffers)
        {
            bool const isValid = compileRequire(buf.second.require, requestedBuffers);
            if (auto pos = requestedBuffers.find(buf.first); pos != requestedBuffers.end())
            {
                // Check that requires clause doesn't conflict
                if (isValid)
                {
                    combineBuffersFunc(pos->second, buf.second, buf.first);
                }
                else
                {
                    GFX_PRINTLN("Error: Shared buffer requires clause was violated: %s, '%s'",
                        buf.first.data(), buf.second.require.data());
                }
            }
            else if (isValid
                     && (!(buf.second.flags & SharedBuffer::Flags::OptionalDiscard)
                         || buf.second.flags & SharedBuffer::Flags::OptionalKeep))
            {
                // Add the new shared buffer to requested list
                addBuffersFunc(buf.first, buf.second);
            }
        }

        // Create all requested shared buffers
        for (auto &i : requestedBuffers)
        {
            if (i.second.size == 0 && !(i.second.flags & SharedBuffer::Flags::Allocate))
            {
                GFX_PRINTLN("Error: Requested shared buffer does not have valid size: %s", i.first.data());
                continue;
            }

            // Create new buffer
            GfxBuffer buffer     = gfxCreateBuffer(gfx_, i.second.size);
            auto      bufferName = string(i.first);
            bufferName += "SharedBuffer";
            buffer.setName(bufferName.c_str());

            // Set stride if available
            if (i.second.stride != 0)
            {
                buffer.setStride(i.second.stride);
            }

            // Add to clear list
            if (i.second.flags & SharedBuffer::Flags::Clear)
            {
                clear_shared_buffers_.emplace_back(static_cast<uint32_t>(shared_buffers_.size()));
            }

            // Add to buffer list
            shared_buffers_.emplace_back(i.first, buffer);
        }

        // Initialise the buffers
        for (auto const &i : shared_buffers_)
        {
            gfxCommandClearBuffer(gfx_, i.second);
        }
    }

    {
        // Get requested shared textures
        struct TextureParams
        {
            DXGI_FORMAT                   format     = DXGI_FORMAT_R16G16B16A16_FLOAT;
            BitMask<SharedTexture::Flags> flags      = SharedTexture::Flags::None;
            uint2                         dimensions = uint2(0, 0);
            bool                          mips       = false;
            string_view                   backup     = "";
        };

        struct OptionalTextureParams : TextureParams
        {
            string require = "";
        };

        // We use 3 main default shared textures that are always available
        using TextureList                     = unordered_map<string_view, TextureParams>;
        TextureList const defaultOptionalAOVs = {
            {      "Depth",         {.format = DXGI_FORMAT_D32_FLOAT, .flags = SharedTexture::Flags::Clear}},
            {      "Debug", {.format = DXGI_FORMAT_R16G16B16A16_FLOAT, .flags = SharedTexture::Flags::None}},
            {"ColorScaled",             {.format   = DXGI_FORMAT_R16G16B16A16_FLOAT,
             .flags = SharedTexture::Flags::None}                               }, //   Optional AOV used when up-scaling
        };
        TextureList                                       requestedTextures;
        unordered_map<string_view, OptionalTextureParams> optionalTextures;
        vector<pair<string_view, OptionalTextureParams>>  optionalDependentTextures;

        auto combineTexturesFunc = [&](TextureParams &update, TextureParams const &textureParams,
                                       string_view const &textureName) {
            // Update existing format if it doesn't have one
            if (update.format == DXGI_FORMAT_UNKNOWN)
            {
                update.format = textureParams.format;
            }
            // Validate that requested values match the existing ones
            else if (textureParams.format != update.format && textureParams.format != DXGI_FORMAT_UNKNOWN)
            {
                GFX_PRINTLN("Error: Requested shared texture with different formats: %s", textureName.data());
            }
            if (((textureParams.flags & SharedTexture::Flags::Clear)
                    && (update.flags & SharedTexture::Flags::Accumulate))
                || ((textureParams.flags & SharedTexture::Flags::Accumulate)
                    && (update.flags & SharedTexture::Flags::Clear)))
            {
                GFX_PRINTLN(
                    "Error: Requested shared texture with different clear settings: %s", textureName.data());
            }
            // Update texture size and mips
            if (any(greaterThan(textureParams.dimensions, uint2(0))))
            {
                if (any(equal(update.dimensions, uint2(0))))
                {
                    update.dimensions = textureParams.dimensions;
                }
            }
            if (any(notEqual(update.dimensions, textureParams.dimensions)))
            {
                GFX_PRINTLN(
                    "Error: Cannot create shared texture with different resolutions: %s", textureName.data());
            }
            if (!update.mips)
            {
                update.mips = textureParams.mips;
            }
            // Add backup name if requested
            if (!textureParams.backup.empty())
            {
                if (update.backup.empty())
                {
                    update.backup = std::move(textureParams.backup);
                }
                else if (update.backup != textureParams.backup)
                {
                    GFX_PRINTLN("Error: Requested shared texture with different backup names: %s, %2",
                        textureName.data(), update.backup.data());
                }
            }
            // Add clear/accumulate flag if requested
            if (textureParams.flags & SharedTexture::Flags::Clear)
            {
                update.flags = update.flags | SharedTexture::Flags::Clear;
            }
            else if (textureParams.flags & SharedTexture::Flags::Accumulate)
            {
                update.flags = update.flags | SharedTexture::Flags::Accumulate;
            }
            // Update extra optional flags as needed
            if (textureParams.flags & SharedTexture::Flags::OptionalDiscard)
            {
                update.flags = update.flags | SharedTexture::Flags::OptionalDiscard;
            }
            if (textureParams.flags & SharedTexture::Flags::OptionalKeep)
            {
                update.flags = update.flags | SharedTexture::Flags::OptionalKeep;
            }
        };
        auto addTextureFunc = [&](string_view const &name, TextureParams &newParams) -> bool {
            // Check if backup shared texture collides
            if (!newParams.backup.empty())
            {
                if (auto const pos = ranges::find_if(requestedTextures,
                        [&newParams](auto const &val) { return val.second.backup == newParams.backup; });
                    pos != requestedTextures.end())
                {
                    GFX_PRINTLN("Error: Multiple backups found with colliding names: %s, for: %s, %s",
                        newParams.backup.data(), name.data(), pos->first.data());
                    newParams.backup = "";
                }
            }

            if (auto const pos = requestedTextures.find(name); pos == requestedTextures.end())
            {
                // Add the new shared texture to requested list
                requestedTextures.try_emplace(std::move(name), std::move(newParams));
            }
            else
            {
                // Merge with existing
                combineTexturesFunc(pos->second, newParams, name);
            }
            return true;
        };

        auto textureFunc = [&](SharedTexture const &tex) {
            auto newParams = TextureParams {.format = tex.format,
                .flags                              = tex.flags,
                .dimensions                         = tex.dimensions,
                .mips                               = tex.mips,
                .backup                             = tex.backup_name};
            if (auto const found = requestedTextures.find(tex.name); found == requestedTextures.end())
            {
                // Check if the shared texture is being read despite never having been written to
                if ((tex.access == SharedTexture::Access::Read) && !optionalTextures.contains(tex.name)
                    && (newParams.flags & SharedTexture::Flags::Clear))
                {
                    GFX_PRINTLN(
                        "Error: Requested read access to shared texture that has not been written to: %s",
                        tex.name.data());
                }
                // Check if shared texture is one of the optional default ones and add it using default
                // values
                if (auto const k = defaultOptionalAOVs.find(tex.name); k != defaultOptionalAOVs.end())
                {
                    newParams.format     = k->second.format;
                    newParams.flags      = k->second.flags | newParams.flags;
                    newParams.dimensions = k->second.dimensions;
                }
                bool addTexture = false;
                if (newParams.flags & SharedTexture::Flags::Optional
                    || newParams.flags & SharedTexture::Flags::OptionalDiscard
                    || newParams.flags & SharedTexture::Flags::OptionalKeep)
                {
                    if (auto const pos = optionalTextures.find(tex.name);
                        tex.access != SharedTexture::Access::Read
                        && !(tex.access == SharedTexture::Access::ReadWrite && pos != optionalTextures.end()))
                    {
                        // Check if texture already contained an optional write
                        if (pos != optionalTextures.end() && tex.access != SharedTexture::Access::ReadWrite)
                        {
                            GFX_PRINTLN(
                                "Error: Found multiple writes to same optional texture: %s", tex.name.data());
                        }
                        else if ((tex.access == SharedTexture::Access::Read)
                                 && (newParams.flags & SharedTexture::Flags::Clear))
                        {
                            GFX_PRINTLN(
                                "Error: Requested read access to optional shared texture that has not been written to: %s",
                                tex.name.data());
                        }
                        if (!tex.backup_name.empty())
                        {
                            GFX_PRINTLN(
                                "Error: Requested backup of optional shared texture: %s", tex.name.data());
                        }
                        if (tex.access == SharedTexture::Access::Write && tex.name == "ColorScaled"
                            && render_scale_ < 1.0F)
                        {
                            // Special handling of up-scaling
                            addTexture = true;
                        }
                        if ((newParams.flags & SharedTexture::Flags::OptionalKeep)
                            || (pos != optionalTextures.end()
                                && pos->second.flags & SharedTexture::Flags::OptionalKeep))
                        {
                            if (tex.require.empty()
                                && (pos == optionalTextures.end() || pos->second.require.empty()))
                            {
                                addTexture = true;
                            }
                            else
                            {
                                OptionalTextureParams reqParams = {newParams, string(tex.require)};
                                if (pos != optionalTextures.end())
                                {
                                    combineRequire(reqParams.require, pos->second.require);
                                    pos->second.require.clear();
                                }
                                if (auto const find = ranges::find_if(optionalDependentTextures,
                                        [&](auto const &val) { return val.first == tex.name; });
                                    find == optionalDependentTextures.end())
                                {
                                    optionalDependentTextures.emplace_back(tex.name.data(), reqParams);
                                }
                                else
                                {
                                    combineTexturesFunc(find->second, newParams, tex.name);
                                    combineRequire(find->second.require, reqParams.require);
                                }
                            }
                        }
                        if (pos != optionalTextures.end())
                        {
                            // Merge with existing
                            combineTexturesFunc(pos->second, newParams, tex.name);
                        }
                        else if (!addTexture)
                        {
                            // Add to list of optional textures
                            optionalTextures.try_emplace(tex.name, newParams, string(tex.require));
                        }
                    }
                    else if (pos != optionalTextures.end())
                    {
                        // Check if connection can be made
                        if (tex.require.empty() && pos->second.require.empty())
                        {
                            if ((!(newParams.flags & SharedTexture::Flags::OptionalDiscard)
                                    && !(pos->second.flags & SharedTexture::Flags::OptionalDiscard))
                                || (newParams.flags & SharedTexture::Flags::OptionalKeep
                                    || (pos->second.flags & SharedTexture::Flags::OptionalKeep)))
                            {
                                addTexture = true;
                            }
                            else
                            {
                                combineTexturesFunc(pos->second, newParams, tex.name);
                            }
                        }
                        else
                        {
                            // This is a dependent texture, connecting must be delayed until all other
                            // connections are made
                            combineTexturesFunc(pos->second, newParams, tex.name);
                            OptionalTextureParams reqParams = {newParams, string(tex.require)};
                            if (!pos->second.require.empty() && !tex.require.empty())
                            {
                                combineRequire(reqParams.require, pos->second.require);
                            }
                            pos->second.require.clear();
                            if (auto const find = ranges::find_if(optionalDependentTextures,
                                    [&](auto const &val) { return val.first == tex.name; });
                                find == optionalDependentTextures.end())
                            {
                                optionalDependentTextures.emplace_back(tex.name.data(), reqParams);
                            }
                            else
                            {
                                combineTexturesFunc(find->second, newParams, tex.name);
                                combineRequire(find->second.require, reqParams.require);
                            }
                        }
                    }
                }
                else
                {
                    addTexture = true;
                }
                if (addTexture)
                {
                    addTextureFunc(tex.name, newParams);
                }
            }
            else
            {
                combineTexturesFunc(found->second, newParams, tex.name);
            }
        };

        // Check any internal shared textures first
        for (auto &j : getStockSharedTextures())
        {
            textureFunc(j);
        }

        // Loop through all render techniques and components and check their requested shared textures
        for (auto const &i : render_techniques_)
        {
            for (auto &j : i->getSharedTextures())
            {
                textureFunc(j);
            }
        }
        for (auto const &i : components_)
        {
            for (auto &j : i.second->getSharedTextures())
            {
                textureFunc(j);
            }
        }

        // Check for up-scaling request
        if (auto j = requestedTextures.find("ColorScaled"); j != requestedTextures.end())
        {
            // Note: Changing the render scale after texture negotiation will not create the display
            // resolution output AOV. Any technique should check for the presence of "ColorScaled" when
            // checking if scaling is enabled
            if (render_scale_ >= 1.0F)
            {
                requestedTextures.erase(j);
            }
        }

        // Merge optional shared textures
        for (auto &[textureName, textureParams] : optionalTextures)
        {
            if (auto j = requestedTextures.find(textureName); j != requestedTextures.end())
            {
                combineTexturesFunc(j->second, textureParams, textureName);
            }
            if (auto j = ranges::find_if(optionalDependentTextures,
                    [&textureName](pair<string_view, OptionalTextureParams> const &val) {
                        return val.first == textureName;
                    });
                j != optionalDependentTextures.end() && !j->second.require.empty())
            {
                combineTexturesFunc(j->second, textureParams, textureName);
                combineRequire(j->second.require, textureParams.require);
            }
        }

        // Perform optional texture dependent checks
        for (auto &tex : optionalDependentTextures)
        {
            bool const isValid = compileRequire(tex.second.require, requestedTextures);
            if (auto pos = requestedTextures.find(tex.first); pos != requestedTextures.end())
            {
                // Check that requires clause doesn't conflict
                if (isValid)
                {
                    combineTexturesFunc(pos->second, tex.second, tex.first);
                }
                else
                {
                    GFX_PRINTLN("Error: Shared texture requires clause was violated: %s, '%s'",
                        tex.first.data(), tex.second.require.data());
                }
            }
            else if (isValid
                     && (!(tex.second.flags & SharedTexture::Flags::OptionalDiscard)
                         || tex.second.flags & SharedTexture::Flags::OptionalKeep))
            {
                addTextureFunc(tex.first, tex.second);
            }
        }

        // Create all requested shared textures
        for (auto &[textureName, textureParams] : requestedTextures)
        {
            if (textureParams.format == DXGI_FORMAT_UNKNOWN)
            {
                GFX_PRINTLN(
                    "Error: Requested shared texture does not have valid format: %s", textureName.data());
                continue;
            }

            // Create new texture
            constexpr array clear = {0.0F, 0.0F, 0.0F, 0.0F};
            GfxTexture      texture;
            if (all(greaterThan(textureParams.dimensions, uint2(0))))
            {
                texture = gfxCreateTexture2D(gfx_, textureParams.dimensions.x, textureParams.dimensions.y,
                    textureParams.format,
                    textureParams.mips
                        ? gfxCalculateMipCount(textureParams.dimensions.x, textureParams.dimensions.y)
                        : 1,
                    (textureParams.format != DXGI_FORMAT_D32_FLOAT) ? nullptr : clear.data(),
                    (textureParams.format != DXGI_FORMAT_D32_FLOAT)
                        ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS | D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET
                        : D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
            }
            else
            {
                // The ColorScaled AOV is a special case that is used for up-scaling. It is set to match
                // the display resolution.
                auto textureDimensions =
                    (textureName != "ColorScaled") ? render_dimensions_ : window_dimensions_;
                texture = gfxCreateTexture2D(gfx_, textureDimensions.x, textureDimensions.y,
                    textureParams.format,
                    textureParams.mips
                        ? gfxCalculateMipCount(textureParams.dimensions.x, textureParams.dimensions.y)
                        : 1,
                    (textureParams.format != DXGI_FORMAT_D32_FLOAT) ? nullptr : clear.data(),
                    (textureParams.format != DXGI_FORMAT_D32_FLOAT)
                        ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS | D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET
                        : D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
            }
            auto bufferName = string(textureName);
            bufferName += "SharedTexture";
            texture.setName(bufferName.c_str());

            // Add to the backup list
            if (!textureParams.backup.empty())
            {
                // Create new backup texture
                GfxTexture texture2;
                if (all(greaterThan(textureParams.dimensions, uint2(0))))
                {
                    texture2 = gfxCreateTexture2D(gfx_, textureParams.dimensions.x,
                        textureParams.dimensions.y, textureParams.format,
                        textureParams.mips
                            ? gfxCalculateMipCount(textureParams.dimensions.x, textureParams.dimensions.y)
                            : 1,
                        (textureParams.format != DXGI_FORMAT_D32_FLOAT) ? nullptr : clear.data(),
                        (textureParams.format != DXGI_FORMAT_D32_FLOAT)
                            ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS
                                  | D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET
                            : D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
                }
                else
                {
                    auto textureDimensions =
                        (textureName != "ColorScaled") ? render_dimensions_ : window_dimensions_;
                    texture2 = gfxCreateTexture2D(gfx_, textureDimensions.x, textureDimensions.y,
                        textureParams.format,
                        textureParams.mips
                            ? gfxCalculateMipCount(textureParams.dimensions.x, textureParams.dimensions.y)
                            : 1,
                        (textureParams.format != DXGI_FORMAT_D32_FLOAT) ? nullptr : clear.data(),
                        (textureParams.format != DXGI_FORMAT_D32_FLOAT)
                            ? D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS
                                  | D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET
                            : D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
                }
                bufferName = string(textureParams.backup);
                bufferName += "SharedTexture";
                texture2.setName(bufferName.c_str());
                auto const location = static_cast<uint32_t>(shared_textures_.size());
                shared_textures_.emplace_back(textureParams.backup, texture2);
                backup_shared_textures_.emplace_back(make_pair(location + 1, location));

                // Add the shared texture as a debug view (Using false to differentiate as shared texture)
                if (textureName != "Color" && textureName != "Debug" && textureName != "ColorScaled")
                {
                    debug_views_.emplace_back(textureParams.backup, false);
                }
            }

            // Add to clear list
            if (textureParams.flags & SharedTexture::Flags::Clear)
            {
                clear_shared_textures_.emplace_back(static_cast<uint32_t>(shared_textures_.size()));
            }

            // Add to texture list
            shared_textures_.emplace_back(textureName, texture);

            // Add the shared texture as a debug view (Using false to differentiate as shared texture)
            if (textureName != "Color" && textureName != "Debug" && textureName != "ColorScaled")
            {
                debug_views_.emplace_back(textureName, false);
            }
        }

        // Initialise the shared textures
        for (auto const &i : shared_textures_)
        {
            gfxCommandClearTexture(gfx_, i.second);
        }
    }

    {
        // Get debug views
        auto debugViewFunc = [&](string_view &name) {
            auto const k = ranges::find_if(debug_views_, [&name](auto val) { return val.first == name; });
            if (k == debug_views_.end())
            {
                debug_views_.emplace_back(name, true);
            }
            else if (!k->second)
            {
                // We allow components to override the default debug view if requested
                k->second = true;
            }
            else
            {
                GFX_PRINTLN("Error: Duplicate debug views detected: %s", name.data());
            }
        };

        // Get any stock debug views
        for (auto &j : this->getStockDebugViews())
        {
            debugViewFunc(j);
        }

        // Check components and render techniques for debug views and add them to the internal list
        for (auto const &i : components_)
        {
            for (auto &j : i.second->getDebugViews())
            {
                debugViewFunc(j);
            }
        }
        for (auto const &i : render_techniques_)
        {
            for (auto &j : i->getDebugViews())
            {
                debugViewFunc(j);
            }
        }
    }
}

bool CapsaicinInternal::setupRenderTechniques(string_view const &name) noexcept
{
    // Clear any existing shared textures
    for (auto const &i : shared_textures_)
    {
        gfxCommandClearTexture(gfx_, i.second);
    }

    // Flush & sync all operations so resources can be safely removed
    gfxFinish(gfx_);

    // Delete any existing render techniques
    render_techniques_.clear();

    // Ensure any resources are available
    gfxFinish(gfx_);

    // Delete old options, debug views and other state
    options_.clear();
    components_.clear();
    renderer_name_ = "";
    renderer_      = nullptr;
    resetPlaybackState();

    // Get default internal options
    options_ = getStockRenderOptions();

    // Create the new renderer
    renderer_ = RendererFactory::make(name);
    if (renderer_)
    {
        render_techniques_ = renderer_->setupRenderTechniques(options_);
        renderer_name_     = name;
    }
    else
    {
        GFX_PRINTLN("Error: Unknown renderer requested: %s", name.data());
        return false;
    }

    {
        // Get render technique options
        for (auto const &i : render_techniques_)
        {
            options_.merge(i->getRenderOptions());
        }

        // Get stock components
        auto requestedComponents = getStockComponents();

        // Get any components requested by active render techniques
        for (auto const &i : render_techniques_)
        {
            for (auto &j : i->getComponents())
            {
                if (ranges::find(as_const(requestedComponents), j) == requestedComponents.cend())
                {
                    // Add the new component to requested list
                    requestedComponents.emplace_back(j);
                }
            }
        }

        // Create all requested components (backwards to maintain ordering)
        for (auto &i : requestedComponents | views::reverse)
        {
            // Create the new component
            if (auto component = ComponentFactory::make(i))
            {
                components_.emplace_back(i, std::move(component));
            }
            else
            {
                GFX_PRINTLN("Error: Unknown component requested: %s", i.data());
                return false;
            }
        }

        // Create any additional components requested by current components
        auto firstNew = components_.begin();
        while (true)
        {
            requestedComponents.clear();
            for (; firstNew < components_.end(); ++firstNew)
            {
                for (auto const newComponents = firstNew->second->getComponents();
                    auto const &j : newComponents)
                {
                    if (ranges::find(as_const(requestedComponents), j) == requestedComponents.cend())
                    {
                        if (auto const pos = ranges::find_if(
                                components_, [&j](ComponentPair const &pair) { return pair.first == j; });
                            pos == components_.end())
                        {
                            // Add the new component to requested list
                            requestedComponents.emplace_back(j);
                        }
                        else
                        {
                            // The component must be reordered in the list to appear after the
                            // component that requested it
                            if (pos < firstNew)
                            {
                                // The component is in the wrong position, so move it to the correct
                                // position
                                auto const posIndex = distance(components_.begin(), pos);
                                firstNew = components_.insert(std::next(firstNew), std::move(*pos));
                                auto const iteratorIndex = distance(components_.begin(), firstNew) - 2;
                                components_.erase(next(components_.begin(), posIndex));
                                firstNew = next(components_.begin(), iteratorIndex);
                            }
                        }
                    }
                }
            }

            if (requestedComponents.empty())
            {
                break;
            }

            // Create all requested components
            for (auto &i : requestedComponents | views::reverse)
            {
                // Create the new component
                if (auto component = ComponentFactory::make(i))
                {
                    components_.emplace_back(i, std::move(component));
                }
                else
                {
                    GFX_PRINTLN("Error: Unknown component requested: %s", i.data());
                    return false;
                }
            }
            // Fix iterator in case of invalidation
            firstNew = next(components_.end(), -static_cast<int32_t>(requestedComponents.size()));
        }

        // Reverse components to maintain original order
        ranges::reverse(components_);

        // Get component options
        for (auto const &i : components_)
        {
            options_.merge(i.second->getRenderOptions());
        }

        // Check if renderer set any renderer specific default options
        for (auto const overrides = renderer_->getRenderOptions(); auto const &i : overrides)
        {
            if (auto j = options_.find(i.first); j != options_.end())
            {
                if (j->second.index() == i.second.index())
                {
                    j->second = i.second;
                }
                else
                {
                    GFX_PRINTLN("Error: Attempted to override option using incorrect type: %s",
                        string(i.first).c_str());
                    return false;
                }
            }
            else
            {
                GFX_PRINTLN("Error: Unknown override option requested: %s", string(i.first).c_str());
                return false;
            }
        }
    }

    negotiateRenderTechniques();

    // If no scene currently loaded then delay initialisation till scene load
    if (!!scene_)
    {
        // Reset flags as everything is about to get reset anyway
        resetEvents();

        // Initialise all components
        for (auto const &i : components_)
        {
            i.second->setGfxContext(gfx_);
            if (!i.second->init(*this))
            {
                GFX_PRINTLN("Error: Failed to initialise component: %s", i.first.data());
                return false;
            }
        }

        // Initialise all render techniques
        for (auto const &i : render_techniques_)
        {
            i->setGfxContext(gfx_);
            if (!i->init(*this))
            {
                GFX_PRINTLN("Error: Failed to initialise render technique: %s", i->getName().data());
                return false;
            }
        }
    }
    return true;
}

void CapsaicinInternal::resetPlaybackState() noexcept
{
    // Reset frame index
    frame_index_ = numeric_limits<uint32_t>::max();
    // Reset frame time
    auto const wallTime =
        chrono::duration_cast<chrono::microseconds>(chrono::high_resolution_clock::now().time_since_epoch());
    current_time_  = static_cast<double>(wallTime.count()) / 1000000.0;
    frame_time_    = 0.0;
    play_time_     = 0.0;
    play_time_old_ = -1.0;
}

void CapsaicinInternal::resetRenderState() const noexcept
{
    // Reset the shared texture history
    {
        GfxCommandEvent const command_event(gfx_, "ResetPreviousGBuffers");

        for (auto const &i : backup_shared_textures_)
        {
            gfxCommandClearTexture(gfx_, shared_textures_[i.second].second);
        }
    }
}

void CapsaicinInternal::destroyAccelerationStructure()
{
    for (auto const &raytracing_primitive : raytracing_primitives_)
    {
        gfxDestroyRaytracingPrimitive(gfx_, raytracing_primitive);
    }

    raytracing_primitives_.clear();

    gfxDestroyAccelerationStructure(gfx_, acceleration_structure_);
}

void CapsaicinInternal::resetEvents() noexcept
{
    render_dimensions_updated_ = false;
    window_dimensions_updated_ = false;
    mesh_updated_              = false;
    transform_updated_         = false;
    environment_map_updated_   = false;
    scene_updated_             = false;
    camera_changed_            = false;
    camera_updated_            = false;
    animation_updated_         = false;
}

} // namespace Capsaicin
