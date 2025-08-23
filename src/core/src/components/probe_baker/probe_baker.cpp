#include "probe_baker.h"
#include "capsaicin_internal.h"
#include "irradiance_probe_baker_shared.h"

namespace Capsaicin
{
inline ProbeBaker::ProbeBaker() noexcept
    : Component(Name) {}

ProbeBaker::~ProbeBaker() noexcept
{
    terminate();
}

bool ProbeBaker::init(CapsaicinInternal const &capsaicin) noexcept
{
    constexpr uint32_t C_IRRADIANCE_PROBE_RESOLUTION            = 64;
    constexpr uint32_t C_PREFILTERED_ENVIRONMENT_MAP_RESOLUTION = 1024;

    m_irradianceProbeTexture =
        gfxCreateTextureCube(gfx_, C_IRRADIANCE_PROBE_RESOLUTION, DXGI_FORMAT_R16G16B16A16_FLOAT);
    m_irradianceProbeTexture.setName("IrradianceProbe");
    m_prefilteredEnvironmentMap = gfxCreateTextureCube(
        gfx_, C_PREFILTERED_ENVIRONMENT_MAP_RESOLUTION, DXGI_FORMAT_R16G16B16A16_FLOAT, 5);
    m_prefilteredEnvironmentMap.setName("PrefilteredEnvironmentMap");

    const GfxDrawState irradianceBakerDrawState = {};
    gfxDrawStateSetCullMode(irradianceBakerDrawState, D3D12_CULL_MODE_BACK);
    gfxDrawStateSetDepthFunction(irradianceBakerDrawState, D3D12_COMPARISON_FUNC_GREATER);
    gfxDrawStateSetColorTarget(irradianceBakerDrawState, 0, m_irradianceProbeTexture.getFormat());

    m_irradianceBakerProgram = capsaicin.createProgram("components/probe_baker/irradiance_probe_baker");
    m_irradianceBakerKernel  = gfxCreateGraphicsKernel(gfx_, m_irradianceBakerProgram,
        irradianceBakerDrawState);

    const GfxDrawState prefilteredEnvironmentBakerDrawState = {};
    gfxDrawStateSetCullMode(prefilteredEnvironmentBakerDrawState, D3D12_CULL_MODE_BACK);
    gfxDrawStateSetDepthFunction(prefilteredEnvironmentBakerDrawState, D3D12_COMPARISON_FUNC_GREATER);
    gfxDrawStateSetColorTarget(
        prefilteredEnvironmentBakerDrawState, 0, m_prefilteredEnvironmentMap.getFormat());

    m_prefilteredEnvironmentBakerProgram =
        capsaicin.createProgram("components/probe_baker/prefiltered_environment_baker");
    m_prefilteredEnvironmentBakerKernel = gfxCreateGraphicsKernel(gfx_,
        m_prefilteredEnvironmentBakerProgram, prefilteredEnvironmentBakerDrawState);

    return m_irradianceBakerKernel && m_prefilteredEnvironmentBakerKernel;
}

void ProbeBaker::run([[maybe_unused]] CapsaicinInternal &capsaicin) noexcept
{
    if (!capsaicin.getEnvironmentMapUpdated())
    {
        return;
    }

    constexpr glm::vec3 directions[6] = {
        {1.0f, 0.0f, 0.0f},
        {-1.0f, 0.0f, 0.0f},
        {0.0f, 1.0f, 0.0f},
        {0.0f, -1.0f, 0.0f},
        {0.0f, 0.0f, 1.0f},
        {0.0f, 0.0f, -1.0f}
    };

    // Should be in sync with the fragment shader.
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

    // Bake irradiance probe. 
    {
        gfxProgramSetParameter(gfx_, m_irradianceBakerProgram, "g_EnvironmentMap",
            capsaicin.getEnvironmentBuffer());
        gfxProgramSetParameter(gfx_, m_irradianceBakerProgram, "g_LinearSampler",
            capsaicin.getLinearSampler());
        gfxProgramSetParameter(gfx_, m_irradianceBakerProgram, "g_DrawConstants", m_drawConstants);
        gfxProgramSetParameter(gfx_, m_irradianceBakerProgram, "g_invScreenSize",
            float2{1.0f / static_cast<float>(m_irradianceProbeTexture.getWidth()),
                   1.0f / static_cast<float>(m_irradianceProbeTexture.getHeight())});
        gfxCommandBindKernel(gfx_, m_irradianceBakerKernel);

        for (uint32_t faceIndex = 0; faceIndex < drawData.size(); ++faceIndex)
        {
            gfxProgramSetParameter(gfx_, m_irradianceBakerProgram, "g_FaceIndex", faceIndex);
            gfxCommandBindColorTarget(gfx_, 0, m_irradianceProbeTexture, 0, faceIndex);

            gfxCommandDraw(gfx_, 3);
        }
    }

    // Bake prefiltered environment map.
    {
        gfxProgramSetParameter(
            gfx_, m_prefilteredEnvironmentBakerProgram, "g_EnvironmentMap",
            capsaicin.getEnvironmentBuffer());
        gfxProgramSetParameter(
            gfx_, m_prefilteredEnvironmentBakerProgram, "g_LinearSampler", capsaicin.getLinearSampler());
        gfxProgramSetParameter(
            gfx_, m_prefilteredEnvironmentBakerProgram, "g_DrawConstants", m_drawConstants);
        gfxCommandBindKernel(gfx_, m_prefilteredEnvironmentBakerKernel);

        const uint32_t mipLevels     = m_prefilteredEnvironmentMap.getMipLevels();
        float2         invScreenSize = {1.0f / static_cast<float>(m_prefilteredEnvironmentMap.getWidth()),
                                        1.0f / static_cast<float>(m_prefilteredEnvironmentMap.getHeight())};
        for (uint32_t mipIndex = 0; mipIndex < mipLevels; ++mipIndex)
        {
            // TODO think about the roughness mapping.
            gfxProgramSetParameter(
                gfx_, m_prefilteredEnvironmentBakerProgram, "g_Roughness",
                (float)mipIndex / ((float)mipLevels - 1.0f));
            gfxProgramSetParameter(gfx_, m_prefilteredEnvironmentBakerProgram, "g_invScreenSize",
                invScreenSize);
            invScreenSize *= 2.0f;

            for (uint32_t faceIndex = 0; faceIndex < drawData.size(); ++faceIndex)
            {
                gfxProgramSetParameter(
                    gfx_, m_prefilteredEnvironmentBakerProgram, "g_FaceIndex", faceIndex);
                gfxCommandBindColorTarget(gfx_, 0, m_prefilteredEnvironmentMap, mipIndex, faceIndex);

                gfxCommandDraw(gfx_, 3);
            }
        }
    }
}

void ProbeBaker::terminate() noexcept
{
    gfxDestroyTexture(gfx_, m_irradianceProbeTexture);
    gfxDestroyTexture(gfx_, m_prefilteredEnvironmentMap);
    gfxDestroyBuffer(gfx_, m_drawConstants);
    gfxDestroyKernel(gfx_, m_prefilteredEnvironmentBakerKernel);
    gfxDestroyProgram(gfx_, m_prefilteredEnvironmentBakerProgram);
    gfxDestroyKernel(gfx_, m_irradianceBakerKernel);
    gfxDestroyProgram(gfx_, m_irradianceBakerProgram);
}

void ProbeBaker::addProgramParameters(
    [[maybe_unused]] CapsaicinInternal const &capsaicin,
    [[maybe_unused]] GfxProgram const &       program) const noexcept {}
} // namespace Capsaicin
