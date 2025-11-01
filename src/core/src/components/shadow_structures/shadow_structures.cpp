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
    m_virtualPageTable = gfxCreateTexture2DArray(gfx_, PAGE_TABLE_RESOLUTION_UINT,
        PAGE_TABLE_RESOLUTION_UINT, CASCADES_NUM_UINT, DXGI_FORMAT_R32_UINT);
    m_physicalPages = gfxCreateTexture2D(gfx_, PHYSICAL_TEXTURE_RESOLUTION, PHYSICAL_TEXTURE_RESOLUTION,
        DXGI_FORMAT_R32_UINT);

    m_vptClearProgram = capsaicin.createProgram("components/shadow_structures/clear_virtual_pages");
    m_vptClearKernel  = gfxCreateComputeKernel(gfx_, m_vptClearProgram);
    m_ppClearProgram  = capsaicin.createProgram("components/shadow_structures/clear_physical_pages");
    m_ppClearKernel   = gfxCreateComputeKernel(gfx_, m_ppClearProgram);

    clearResources();

    return m_vptClearKernel && m_ppClearKernel;
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

    // TODO think about making Depth buffer more precise.
    // Calculate view projection matrix.
    const auto defaultLightView = glm::lookAt(glm::vec3{0.0f}, -directionalLightDirection,
        glm::vec3{0.0f, 0.0f, 1.0f});
    const auto defaultLightProjection = glm::ortho(-CASCADE_SIZE_0, CASCADE_SIZE_0, -CASCADE_SIZE_0,
        CASCADE_SIZE_0, -50.0f, 50.0f);

    const auto defaultLightViewProjection = defaultLightProjection * defaultLightView;

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
    m_lightViewProjection = defaultLightProjection * lightView;

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
    gfxProgramSetParameter(gfx_, m_vptClearProgram, "g_VirtualPageTableUav", m_virtualPageTable);
    gfxProgramSetParameter(gfx_, m_ppClearProgram, "g_PhysicalPagesUav", m_physicalPages);

    const uint32_t*  numThreadsPtr = gfxKernelGetNumThreads(gfx_, m_vptClearKernel);
    const glm::uvec3 numThreads    = {numThreadsPtr[0], numThreadsPtr[1], numThreadsPtr[2]};

    const auto getGroupCount = [](uint32_t workSize, uint32_t groupSize) -> uint32_t {
        return (workSize + groupSize - 1) / groupSize;
    };

    gfxCommandBindKernel(gfx_, m_vptClearKernel);
    gfxCommandDispatch(gfx_, getGroupCount(m_virtualPageTable.getWidth(), numThreads.x),
        getGroupCount(m_virtualPageTable.getHeight(), numThreads.y), CASCADES_NUM_UINT);

    gfxCommandBindKernel(gfx_, m_ppClearKernel);
    gfxCommandDispatch(gfx_, getGroupCount(m_physicalPages.getWidth(), numThreads.x),
        getGroupCount(m_physicalPages.getHeight(), numThreads.y), CASCADES_NUM_UINT);
}

void ShadowStructures::clearResourcesDebug()
{
    gfxProgramSetParameter(gfx_, m_vptClearProgram, "g_VirtualPageTableUav", m_virtualPageTable);
    gfxProgramSetParameter(gfx_, m_ppClearProgram, "g_PhysicalPagesUav", m_physicalPages);

    const uint32_t* numThreadsPtr = gfxKernelGetNumThreads(gfx_, m_vptClearKernel);
    const glm::uvec3 numThreads = { numThreadsPtr[0], numThreadsPtr[1], numThreadsPtr[2] };

    const auto getGroupCount = [](uint32_t workSize, uint32_t groupSize) -> uint32_t {
        return (workSize + groupSize - 1) / groupSize;
        };

    gfxCommandBindKernel(gfx_, m_ppClearKernel);
    gfxCommandDispatch(gfx_, getGroupCount(m_physicalPages.getWidth(), numThreads.x),
        getGroupCount(m_physicalPages.getHeight(), numThreads.y), CASCADES_NUM_UINT);
}
} // namespace Capsaicin
