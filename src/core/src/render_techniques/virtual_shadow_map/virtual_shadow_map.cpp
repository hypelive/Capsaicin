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
    m_debugProgram = capsaicin.createProgram("render_techniques/virtual_shadow_map/debug_draw");
    m_debugKernel  = gfxCreateComputeKernel(gfx_, m_debugProgram);

    return m_debugKernel;
}

void VirtualShadowMap::render([[maybe_unused]] CapsaicinInternal& capsaicin) noexcept
{
    m_options                 = convertOptions(capsaicin.getOptions());
    auto const& targetTexture = capsaicin.getSharedTexture("Debug");

    const glm::vec2 renderResolution = {targetTexture.getWidth(), targetTexture.getHeight()};
    auto const&     gpuDrawConstants = capsaicin.allocateConstantBuffer<VSMConstants>(1);
    {
        const auto& cameraMatrices   = capsaicin.getCameraMatrices();
        const auto& camera           = capsaicin.getCamera();
        const auto& directionalLightDirection = capsaicin.getComponent<CustomLightBuilder>()->m_directionalLightDirection;

        const float PAGE_STEP_0          = 0.25f;
        const float CASCADE_SIZE_0       = 1.0f;
        const float CASCADE_HALF_SIZE_0  = CASCADE_SIZE_0 / 2.0f;
        glm::vec3   shadowCameraPosition = PAGE_STEP_0 * glm::round(camera.eye / PAGE_STEP_0);

        // TODO light must be in the origin to calculate wrapped NDC.
        glm::mat4x4 lightView = glm::lookAt(
            shadowCameraPosition + 10.0f * directionalLightDirection,
            shadowCameraPosition,
            {0.0f, 1.0f, 0.0f});

        glm::vec3 cascadeCorners[4] = {shadowCameraPosition + CASCADE_HALF_SIZE_0,
                                       shadowCameraPosition - CASCADE_HALF_SIZE_0,
                                       shadowCameraPosition + glm::vec3{
                                           CASCADE_HALF_SIZE_0, -CASCADE_HALF_SIZE_0, 0.0f},
                                       shadowCameraPosition + glm::vec3{
                                           -CASCADE_HALF_SIZE_0, CASCADE_HALF_SIZE_0, 0.0f}};
        glm::vec3 min{std::numeric_limits<float>::max()};
        glm::vec3 max{-std::numeric_limits<float>::max()};
        for (const auto cascadeCorner : cascadeCorners)
        {
            const glm::vec3 cornerViewSpace = lightView * glm::vec4{cascadeCorner.x, cascadeCorner.y, cascadeCorner.z, 1.0f};
            min = glm::min(min, cornerViewSpace);
            max = glm::max(max, cornerViewSpace);
        }

        const glm::mat4x4 lightProjection = glm::ortho(min.x, max.x, min.y, max.y, 0.0f, max.z);

        VSMConstants drawConstants        = {};
        drawConstants.viewProjection      = cameraMatrices.view_projection;
        drawConstants.invViewProjection   = cameraMatrices.inv_view_projection;
        drawConstants.lightViewProjection = lightProjection * lightView;
        drawConstants.cameraPosition      = camera.eye;
        drawConstants.screenSize          = glm::vec4{renderResolution.x, renderResolution.y,
                                                    1.0f / renderResolution.x,
                                                    1.0f / renderResolution.y};

        gfxBufferGetData<VSMConstants>(gfx_, gpuDrawConstants)[0] = drawConstants;
    }

    // Set the root parameters for Computing VirtualShadowMap.
    {
        gfxProgramSetParameter(gfx_, m_debugProgram, "g_Constants", gpuDrawConstants);
        gfxProgramSetParameter(gfx_, m_debugProgram, "g_DepthCopy", capsaicin.getSharedTexture("DepthCopy"));
        gfxProgramSetParameter(gfx_, m_debugProgram, "g_TargetTexture", targetTexture);
    }

    // Compute VirtualShadowMap.
    {
        gfxCommandBindKernel(gfx_, m_debugKernel);

        glm::uvec2 const groupCount =
            ceil(renderResolution / static_cast<float>(TILE_SIZE));
        gfxCommandDispatch(gfx_, groupCount.x, groupCount.y, 1);
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
