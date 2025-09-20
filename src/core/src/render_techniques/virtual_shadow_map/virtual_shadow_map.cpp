#include "virtual_shadow_map.h"
#include "capsaicin_internal.h"
#include "virtual_shadow_map_shared.h"
#include "components/custom_light_builder/custom_light_builder.h"

namespace Capsaicin
{
VirtualShadowMap::VirtualShadowMap()
    : RenderTechnique("Virtual Shadow Map") {}

VirtualShadowMap::~VirtualShadowMap()
{
    VirtualShadowMap::terminate();
}

RenderOptionList VirtualShadowMap::getRenderOptions() noexcept
{
    RenderOptionList newOptions;
    return newOptions;
}

VirtualShadowMap::RenderOptions VirtualShadowMap::convertOptions(
    [[maybe_unused]] RenderOptionList const& options) noexcept
{
    RenderOptions newOptions;
    return newOptions;
}

ComponentList VirtualShadowMap::getComponents() const noexcept
{
    ComponentList components;
    return components;
}

SharedBufferList VirtualShadowMap::getSharedBuffers() const noexcept
{
    SharedBufferList buffers;
    return buffers;
}

SharedTextureList VirtualShadowMap::getSharedTextures() const noexcept
{
    SharedTextureList textures;
    textures.push_back({"DepthCopy", SharedTexture::Access::Read});
    textures.push_back({"Debug", SharedTexture::Access::Write});
    return textures;
}

DebugViewList VirtualShadowMap::getDebugViews() const noexcept
{
    DebugViewList views;
    views.emplace_back("Cascades");
    return views;
}

bool VirtualShadowMap::init([[maybe_unused]] CapsaicinInternal const& capsaicin) noexcept
{
    m_virtualPageTable = gfxCreateTexture2D(gfx_, (uint32_t)PAGE_TABLE_RESOLUTION,
        (uint32_t)PAGE_TABLE_RESOLUTION, DXGI_FORMAT_R32_UINT);

    m_markVisiblePagesProgram = capsaicin.createProgram(
        "render_techniques/virtual_shadow_map/mark_visible_pages");
    m_markVisiblePagesKernel = gfxCreateComputeKernel(gfx_, m_markVisiblePagesProgram);

    m_debugProgram = capsaicin.createProgram("render_techniques/virtual_shadow_map/debug_draw");
    m_debugKernel  = gfxCreateComputeKernel(gfx_, m_debugProgram);

    return m_markVisiblePagesKernel && m_debugKernel;
}

void VirtualShadowMap::render([[maybe_unused]] CapsaicinInternal& capsaicin) noexcept
{
    m_options                 = convertOptions(capsaicin.getOptions());
    auto const& targetTexture = capsaicin.getSharedTexture("Debug");

    const glm::vec2 renderResolution = {targetTexture.getWidth(), targetTexture.getHeight()};
    auto const&     gpuDrawConstants = capsaicin.allocateConstantBuffer<VSMConstants>(1);
    {
        const auto& cameraMatrices            = capsaicin.getCameraMatrices();
        const auto& camera                    = capsaicin.getCamera();
        const auto& directionalLightDirection = capsaicin.getComponent<CustomLightBuilder>()->
                                                          m_directionalLightDirection;

        const auto defaultLightView = glm::lookAt(glm::vec3{0.0f}, -directionalLightDirection,
            glm::vec3{0.0f, 0.0f, 1.0f});
        const auto lightProjection = glm::ortho(-CASCADE_SIZE_0, CASCADE_SIZE_0, -CASCADE_SIZE_0,
            CASCADE_SIZE_0, -1000.0f, 1000.0f);

        const auto defaultLightViewProjection = lightProjection * defaultLightView;

        auto cameraPositionNDC = defaultLightViewProjection * glm::vec4{camera.eye, 1.0f};

        glm::vec2 cameraPageOffset =
            glm::ceil(glm::vec2{cameraPositionNDC.x, cameraPositionNDC.y} / PAGE_NDC);
        glm::vec2 alignedCameraPositionNDC        = cameraPageOffset * PAGE_NDC;
        glm::vec4 alignedCameraPositionWorldSpace = glm::inverse(defaultLightViewProjection) * glm::vec4{
                                                        alignedCameraPositionNDC, cameraPositionNDC.z, 1.0f};

        glm::vec3 shadowCameraWorldPosition =
            glm::vec3{alignedCameraPositionWorldSpace} + directionalLightDirection * 5.0f;
        glm::mat4x4 lightView = glm::lookAt(shadowCameraWorldPosition,
            shadowCameraWorldPosition - directionalLightDirection, glm::vec3{0.0f, 0.0f, 1.0f});
        glm::mat4x4 lightViewProjection = lightProjection * lightView;

        VSMConstants drawConstants        = {};
        drawConstants.viewProjection      = cameraMatrices.view_projection;
        drawConstants.invViewProjection   = cameraMatrices.inv_view_projection;
        drawConstants.lightViewProjection = lightViewProjection;
        drawConstants.cameraPosition      = camera.eye;
        drawConstants.screenSize          = glm::vec4{renderResolution.x, renderResolution.y,
                                                      1.0f / renderResolution.x,
                                                      1.0f / renderResolution.y};
        drawConstants.alignedLightPositionNDC = glm::vec4{alignedCameraPositionNDC, cameraPositionNDC.z, 0};
        drawConstants.cameraPageOffset        = glm::vec4{cameraPageOffset.x, cameraPageOffset.y, 1.0f, 1.0f};

        gfxBufferGetData<VSMConstants>(gfx_, gpuDrawConstants)[0] = drawConstants;
    }

    {
        gfxProgramSetParameter(gfx_, m_markVisiblePagesProgram, "g_Constants", gpuDrawConstants);
        gfxProgramSetParameter(gfx_, m_markVisiblePagesProgram, "g_DepthCopy",
            capsaicin.getSharedTexture("DepthCopy"));
        gfxProgramSetParameter(gfx_, m_markVisiblePagesProgram, "g_VirtualPageTable", m_virtualPageTable);
    }

    {
        gfxCommandBindKernel(gfx_, m_markVisiblePagesKernel);

        glm::uvec2 const groupCount =
            ceil(renderResolution / static_cast<float>(TILE_SIZE));
        gfxCommandDispatch(gfx_, groupCount.x, groupCount.y, 1);
    }

    // Set the root parameters for Computing VirtualShadowMap.
    {
        gfxProgramSetParameter(gfx_, m_debugProgram, "g_Constants", gpuDrawConstants);
        gfxProgramSetParameter(gfx_, m_debugProgram, "g_DepthCopy", capsaicin.getSharedTexture("DepthCopy"));
        gfxProgramSetParameter(gfx_, m_debugProgram, "g_TargetTexture", targetTexture);
        gfxProgramSetParameter(gfx_, m_debugProgram, "g_VirtualPageTable", m_virtualPageTable);
    }

    // Compute VirtualShadowMap.
    {
        gfxCommandBindKernel(gfx_, m_debugKernel);

        gfxCommandDispatch(gfx_, static_cast<uint32_t>(PAGE_TABLE_RESOLUTION), static_cast<uint32_t>(PAGE_TABLE_RESOLUTION), 1);
    }

    gfxDestroyBuffer(gfx_, gpuDrawConstants);
}

void VirtualShadowMap::terminate() noexcept
{
    gfxDestroyKernel(gfx_, m_debugKernel);
    m_debugKernel = {};
    gfxDestroyProgram(gfx_, m_debugProgram);
    m_debugProgram = {};
}

void VirtualShadowMap::renderGUI([[maybe_unused]] CapsaicinInternal& capsaicin) const noexcept { }
} // namespace Capsaicin
