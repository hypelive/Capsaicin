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
    uint32_t const    lightsCount = gfxSceneGetLightCount(capsaicin.getScene());
    auto const *const lights      = gfxSceneGetLights(capsaicin.getScene());

    gfxDestroyBuffer(gfx_, m_lightsCountBuffer);
    m_lightsCountBuffer = gfxCreateBuffer<uint32_t>(gfx_, 1, &lightsCount);
    m_lightsCountBuffer.setName("Lights Count Buffer");

    if (lightsCount > 0)
    {
        auto &firstLight = lights[0];
        if (firstLight.type == kGfxLightType_Directional)
        {
            Light const directionalLight = MakeDirectionalLight(firstLight.color * firstLight.intensity,
                normalize(firstLight.direction), std::numeric_limits<float>::max());

            gfxDestroyBuffer(gfx_, m_lightsBuffer);
            m_lightsBuffer = gfxCreateBuffer<Light>(gfx_, 1, &directionalLight);
            m_lightsBuffer.setName("Lights Buffer");
        }
    }
}

void CustomLightBuilder::terminate() noexcept
{
    gfxDestroyBuffer(gfx_, m_lightsCountBuffer);
    gfxDestroyBuffer(gfx_, m_lightsBuffer);
}

void CustomLightBuilder::addProgramParameters(
    [[maybe_unused]] CapsaicinInternal const &capsaicin,
    GfxProgram const &                        program) const noexcept
{
    gfxProgramSetParameter(gfx_, program, "g_LightsCountBuffer", m_lightsCountBuffer);
    gfxProgramSetParameter(gfx_, program, "g_LightsBuffer", m_lightsBuffer);
}
} // namespace Capsaicin
