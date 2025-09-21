#include "shadow_structures.h"
#include "shadows/shared.h"
#include "capsaicin_internal.h"
// TODO resolve this dependency
#include "components/custom_light_builder/custom_light_builder.h"

namespace Capsaicin
{
ShadowStructures::ShadowStructures() noexcept
    : Component(Name) {}

ShadowStructures::~ShadowStructures() noexcept
{
    terminate();
}

bool ShadowStructures::init([[maybe_unused]] CapsaicinInternal const& capsaicin) noexcept
{
    m_virtualPageTable = gfxCreateTexture2D(gfx_, PAGE_TABLE_RESOLUTION_UINT,
        PAGE_TABLE_RESOLUTION_UINT, DXGI_FORMAT_R32_UINT);
    constexpr float clearValue = std::numeric_limits<float>::max();
    m_physicalPages            = gfxCreateTexture2D(gfx_, CASCADE_RESOLUTION_UINT, CASCADE_RESOLUTION_UINT,
        DXGI_FORMAT_R32_UINT, 1, &clearValue);

    return true;
}

void ShadowStructures::run(CapsaicinInternal& capsaicin) noexcept
{
    const auto& camera                    = capsaicin.getCamera();
    const auto& directionalLightDirection = capsaicin.getComponent<CustomLightBuilder>()->
                                                      m_directionalLightDirection;

    const auto defaultLightView = glm::lookAt(glm::vec3{0.0f}, -directionalLightDirection,
        glm::vec3{0.0f, 0.0f, 1.0f});
    const auto lightProjection = glm::ortho(-CASCADE_SIZE_0, CASCADE_SIZE_0, -CASCADE_SIZE_0,
        CASCADE_SIZE_0, -50.0f, 50.0f);

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
    m_lightViewProjection = lightProjection * lightView;
}

void ShadowStructures::terminate() noexcept {}

void ShadowStructures::addProgramParameters(
    [[maybe_unused]] CapsaicinInternal const& capsaicin, GfxProgram const& program) const noexcept
{
    gfxProgramSetParameter(gfx_, program, "g_VirtualPageTable", m_virtualPageTable);
    gfxProgramSetParameter(gfx_, program, "g_PhysicalPages", m_physicalPages);
}

void ShadowStructures::clearResources()
{
    gfxCommandClearTexture(gfx_, m_virtualPageTable);
    gfxCommandClearTexture(gfx_, m_physicalPages);
}
} // namespace Capsaicin
