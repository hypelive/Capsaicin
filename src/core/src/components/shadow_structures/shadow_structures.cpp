#include "shadow_structures.h"
#include "shadows/shared.h"
#include "capsaicin_internal.h"

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
    uint VPT_CLEAR = 0xFFFFFFFF;
    m_virtualPageTable = gfxCreateTexture2D(gfx_, PAGE_TABLE_RESOLUTION_UINT,
        PAGE_TABLE_RESOLUTION_UINT, DXGI_FORMAT_R32_UINT, 1, reinterpret_cast<float*>(&VPT_CLEAR));
    constexpr float PP_CLEAR = std::numeric_limits<float>::max();
    m_physicalPages            = gfxCreateTexture2D(gfx_, CASCADE_RESOLUTION_UINT, CASCADE_RESOLUTION_UINT,
        DXGI_FORMAT_R32_UINT, 1, &PP_CLEAR);

    return true;
}

void ShadowStructures::run(CapsaicinInternal& capsaicin) noexcept
{
    const uint32_t    lightsCount = gfxSceneGetLightCount(capsaicin.getScene());
    auto const* const lights      = gfxSceneGetLights(capsaicin.getScene());
    const auto&       camera      = capsaicin.getCamera();

    // Find a directional light.
    glm::vec3 directionalLightDirection = glm::normalize(glm::vec3{1.0f});
    for (uint32_t lightIndex = 0; lightIndex < lightsCount; ++lightIndex)
    {
        auto const& gfxLight = lights[lightIndex];
        if (gfxLight.type == kGfxLightType_Directional)
        {
            directionalLightDirection = normalize(gfxLight.direction);
            break;
        }
    }

    // Calculate view projection matrix.
    const auto defaultLightView = glm::lookAt(glm::vec3{0.0f}, -directionalLightDirection,
        glm::vec3{0.0f, 0.0f, 1.0f});
    const auto lightProjection = glm::ortho(-CASCADE_SIZE_0, CASCADE_SIZE_0, -CASCADE_SIZE_0,
        CASCADE_SIZE_0, -50.0f, 50.0f);

    const auto defaultLightViewProjection = lightProjection * defaultLightView;

    const auto cameraPositionNDC = defaultLightViewProjection * glm::vec4{camera.eye, 1.0f};

    const glm::vec2 cameraPageOffset =
        glm::ceil(glm::vec2{cameraPositionNDC.x, cameraPositionNDC.y} / PAGE_NDC);
    const glm::vec2 alignedCameraPositionNDC        = cameraPageOffset * PAGE_NDC;
    const glm::vec4 alignedCameraPositionWorldSpace = glm::inverse(defaultLightViewProjection) * glm::vec4{
                                                          alignedCameraPositionNDC, cameraPositionNDC.z,
                                                          1.0f};

    const glm::vec3 shadowCameraWorldPosition =
        glm::vec3{alignedCameraPositionWorldSpace} + directionalLightDirection * 5.0f;
    const glm::mat4x4 lightView = glm::lookAt(shadowCameraWorldPosition,
        shadowCameraWorldPosition - directionalLightDirection, glm::vec3{0.0f, 0.0f, 1.0f});
    m_lightViewProjection = lightProjection * lightView;


    gfxDestroyBuffer(gfx_, m_shadowConstants);
    m_shadowConstants = capsaicin.allocateConstantBuffer<ShadowConstants>(1);
    m_shadowConstants.setName("Shadow Constants");

    // Set up the constant buffer.
    ShadowConstants cpuShadowConstants = {};
    cpuShadowConstants.viewProjection  = m_lightViewProjection;

    gfxBufferGetData<ShadowConstants>(gfx_, m_shadowConstants)[0] = cpuShadowConstants;
}

void ShadowStructures::terminate() noexcept
{
    gfxDestroyBuffer(gfx_, m_shadowConstants);
    gfxDestroyTexture(gfx_, m_virtualPageTable);
    gfxDestroyTexture(gfx_, m_physicalPages);
}

void ShadowStructures::addShadingParameters(
    [[maybe_unused]] CapsaicinInternal const& capsaicin, GfxProgram const& program) const noexcept
{
    gfxProgramSetParameter(gfx_, program, "g_ShadowConstants", m_shadowConstants);
    gfxProgramSetParameter(gfx_, program, "g_VirtualPageTable", m_virtualPageTable);
    gfxProgramSetParameter(gfx_, program, "g_PhysicalPages", m_physicalPages);
}

void ShadowStructures::clearResources()
{
    gfxCommandClearTexture(gfx_, m_virtualPageTable);
    gfxCommandClearTexture(gfx_, m_physicalPages);
}
} // namespace Capsaicin
