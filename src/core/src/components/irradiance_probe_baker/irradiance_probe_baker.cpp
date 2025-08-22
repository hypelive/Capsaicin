#include "irradiance_probe_baker.h"
#include "capsaicin_internal.h"
#include "irradiance_probe_baker_shared.h"

namespace Capsaicin
{
inline IrradianceProbeBaker::IrradianceProbeBaker() noexcept
    : Component(Name) {}

IrradianceProbeBaker::~IrradianceProbeBaker() noexcept
{
    terminate();
}

bool IrradianceProbeBaker::init(CapsaicinInternal const &capsaicin) noexcept
{
    constexpr uint32_t C_PROBE_RESOLUTION = 64;

    m_irradianceProbeTexture =
        gfxCreateTextureCube(gfx_, C_PROBE_RESOLUTION, DXGI_FORMAT_R16G16B16A16_FLOAT);
    m_irradianceProbeTexture.setName("IrradianceProbe");

    GfxDrawState const bakerDrawState = {};
    gfxDrawStateSetCullMode(bakerDrawState, D3D12_CULL_MODE_BACK);
    gfxDrawStateSetDepthFunction(bakerDrawState, D3D12_COMPARISON_FUNC_GREATER);
    gfxDrawStateSetColorTarget(bakerDrawState, 0, m_irradianceProbeTexture.getFormat());

    m_bakerProgram = capsaicin.createProgram("components/irradiance_probe_baker/irradiance_probe_baker");
    m_bakerKernel  = gfxCreateGraphicsKernel(gfx_, m_bakerProgram, bakerDrawState);

    return true;
}

void IrradianceProbeBaker::run([[maybe_unused]] CapsaicinInternal &capsaicin) noexcept
{
    constexpr glm::vec3 directions[6] = {
        {1.0f, 0.0f, 0.0f},
        {-1.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f},
        {0.0f, -1.0f, 0.0f},
        {0.0f, 0.0f, 1.0f},
        {0.0f, 0.0f, -1.0f}
    };
    constexpr glm::vec3 upVectors[6] = {
        {0.0f, -1.0f, 0.0f},
        {0.0f, -1.0f, 0.0f},
        {0.0f, 0.0f, 1.0f},
        {0.0f, 0.0f, -1.0f},
        {0.0f, -1.0f, 0.0f},
        {0.0f, -1.0f, 0.0f}
    };

    std::array<float4x4, 6> drawData;

    for (uint32_t faceIndex = 0; faceIndex < 6; ++faceIndex)
    {
        auto const viewMatrix = glm::lookAt(glm::vec3{0.0f}, directions[faceIndex], upVectors[faceIndex]);
        const auto projectionMatrix = glm::perspective(glm::half_pi<float>(), 1.0f, 0.1f, 10.0f);
        const auto viewProjectionMatrix = projectionMatrix * viewMatrix;
        const auto invViewProjectionMatrix = glm::inverse(viewProjectionMatrix);

        drawData[faceIndex] = invViewProjectionMatrix;
    }

    gfxDestroyBuffer(gfx_, m_drawConstants);
    m_drawConstants = gfxCreateBuffer<float4x4>(gfx_, 6, drawData.data());

    gfxProgramSetParameter(gfx_, m_bakerProgram, "g_EnvironmentMap", capsaicin.getEnvironmentBuffer());
    gfxProgramSetParameter(gfx_, m_bakerProgram, "g_DrawConstants", m_drawConstants);
    gfxProgramSetParameter(gfx_, m_bakerProgram, "g_invScreenSize",
        float2{1.0f / static_cast<float>(m_irradianceProbeTexture.getWidth()),
               1.0f / static_cast<float>(m_irradianceProbeTexture.getHeight())});
    gfxCommandBindKernel(gfx_, m_bakerKernel);

    for (uint32_t faceIndex = 0; faceIndex < drawData.size(); ++faceIndex)
    {
        gfxProgramSetParameter(gfx_, m_bakerProgram, "g_FaceIndex", faceIndex);
        gfxCommandBindColorTarget(gfx_, 0, m_irradianceProbeTexture, 0, faceIndex);

        gfxCommandDraw(gfx_, 3);
    }
}

void IrradianceProbeBaker::terminate() noexcept
{
    gfxDestroyTexture(gfx_, m_irradianceProbeTexture);
    gfxDestroyBuffer(gfx_, m_drawConstants);
    gfxDestroyKernel(gfx_, m_bakerKernel);
    gfxDestroyProgram(gfx_, m_bakerProgram);
}

void IrradianceProbeBaker::addProgramParameters(
    [[maybe_unused]] CapsaicinInternal const &capsaicin,
    [[maybe_unused]] GfxProgram const &       program) const noexcept {}
} // namespace Capsaicin
