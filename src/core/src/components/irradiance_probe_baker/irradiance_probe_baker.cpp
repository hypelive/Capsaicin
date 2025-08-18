#include "irradiance_probe_baker.h"
#include "capsaicin_internal.h"

namespace Capsaicin
{
inline IrradianceProbeBaker::IrradianceProbeBaker() noexcept
    : Component(Name)
{}

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

    m_bakerProgram = capsaicin.createProgram("components/irradiance_probe_baker/irradiance_probe_baker");
    m_bakerKernel  = gfxCreateGraphicsKernel(gfx_, m_bakerProgram, bakerDrawState);

    return true;
}

void IrradianceProbeBaker::run([[maybe_unused]] CapsaicinInternal &capsaicin) noexcept
{
    // TODO Update on changing of Environment map.
}

void IrradianceProbeBaker::terminate() noexcept
{
    gfxDestroyTexture(gfx_, m_irradianceProbeTexture);
}

void IrradianceProbeBaker::addProgramParameters(
    [[maybe_unused]] CapsaicinInternal const &capsaicin,
    [[maybe_unused]] GfxProgram const &program) const noexcept
{
}

} // namespace Capsaicin
