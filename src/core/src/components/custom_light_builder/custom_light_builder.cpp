#include "custom_light_builder.h"
#include "capsaicin_internal.h"

namespace Capsaicin
{
inline CustomLightBuilder::CustomLightBuilder() noexcept
    : Component(Name) {}

CustomLightBuilder::~CustomLightBuilder() noexcept
{
    terminate();
}

bool CustomLightBuilder::init([[maybe_unused]] CapsaicinInternal const &capsaicin) noexcept
{
    return true;
}

void CustomLightBuilder::run([[maybe_unused]] CapsaicinInternal &capsaicin) noexcept
{
    const uint32_t    lightsCount = gfxSceneGetLightCount(capsaicin.getScene());
    auto const *const lights      = gfxSceneGetLights(capsaicin.getScene());

    gfxDestroyBuffer(gfx_, m_gpuLightsBufferInfo);
    m_gpuLightsBufferInfo = capsaicin.allocateConstantBuffer<LightsBufferInfo>(1);
    if (lightsCount > 0)
    {
        // Collect Directional, then Point, then Spot, then Area.
        LightsBufferInfo cpuLightBufferInfo = {};
        m_cpuLightsBuffer.reserve(lightsCount);

        for (uint32_t lightIndex = 0; lightIndex < lightsCount; ++lightIndex)
        {
            auto const &gfxLight = lights[lightIndex];
            if (gfxLight.type == kGfxLightType_Directional)
            {
                cpuLightBufferInfo.directionalLightsCount++;
                m_cpuLightsBuffer.push_back(MakeDirectionalLight(gfxLight.color * gfxLight.intensity,
                    normalize(gfxLight.direction), gfxLight.range));
            }
        }

        for (uint32_t lightIndex = 0; lightIndex < lightsCount; ++lightIndex)
        {
            auto const &gfxLight = lights[lightIndex];
            if (gfxLight.type == kGfxLightType_Point)
            {
                cpuLightBufferInfo.pointLightsCount++;
                m_cpuLightsBuffer.push_back(MakePointLight(gfxLight.color * gfxLight.intensity,
                    gfxLight.position, gfxLight.range));
            }
        }

        for (uint32_t lightIndex = 0; lightIndex < lightsCount; ++lightIndex)
        {
            auto const &gfxLight = lights[lightIndex];
            if (gfxLight.type == kGfxLightType_Spot)
            {
                cpuLightBufferInfo.spotLightsCount++;
                m_cpuLightsBuffer.push_back(
                    MakeSpotLight(gfxLight.color * gfxLight.intensity, gfxLight.position, gfxLight.range,
                        normalize(gfxLight.direction), gfxLight.outer_cone_angle, gfxLight.inner_cone_angle));
            }
        }

        // TODO find out how to parse area lights from a scene file.
        cpuLightBufferInfo.areaLightsCount   = 0;
        cpuLightBufferInfo.pointLightsOffset = cpuLightBufferInfo.directionalLightsCount;
        cpuLightBufferInfo.spotLightsOffset  =
            cpuLightBufferInfo.pointLightsOffset + cpuLightBufferInfo.pointLightsCount;
        cpuLightBufferInfo.areaLightsOffset =
            cpuLightBufferInfo.spotLightsOffset + cpuLightBufferInfo.spotLightsCount;

        gfxBufferGetData<LightsBufferInfo>(gfx_, m_gpuLightsBufferInfo)[0] = cpuLightBufferInfo;

        gfxDestroyBuffer(gfx_, m_gpuLightsBuffer);
        m_gpuLightsBuffer = gfxCreateBuffer<Light>(gfx_, static_cast<uint32_t>(m_cpuLightsBuffer.size()),
            m_cpuLightsBuffer.data());
        m_gpuLightsBuffer.setName("Lights Buffer");
    }
    else
    {
        gfxBufferGetData<LightsBufferInfo>(gfx_, m_gpuLightsBufferInfo)[0] = {};
    }
}

void CustomLightBuilder::terminate() noexcept
{
    gfxDestroyBuffer(gfx_, m_gpuLightsBufferInfo);
    gfxDestroyBuffer(gfx_, m_gpuLightsBuffer);
}

void CustomLightBuilder::addProgramParameters(
    [[maybe_unused]] CapsaicinInternal const &capsaicin,
    GfxProgram const &                        program) const noexcept
{
    gfxProgramSetParameter(gfx_, program, "g_LightsBufferInfo", m_gpuLightsBufferInfo);
    gfxProgramSetParameter(gfx_, program, "g_LightsBuffer", m_gpuLightsBuffer);
}
} // namespace Capsaicin
