#pragma once

#include "components/component.h"

namespace Capsaicin
{
class ProbeBaker final
    : public Component
      , ComponentFactory::Registrar<ProbeBaker>
{
public:
    static constexpr std::string_view Name = "ProbeBaker";

    /** Constructor. */
    ProbeBaker() noexcept;

    ~ProbeBaker() noexcept override;

    ProbeBaker(ProbeBaker const &other)                = delete;
    ProbeBaker(ProbeBaker &&other) noexcept            = delete;
    ProbeBaker &operator=(ProbeBaker const &other)     = delete;
    ProbeBaker &operator=(ProbeBaker &&other) noexcept = delete;

    /**
     * Initialise any internal data or state.
     * @note This is automatically called by the framework after construction and should be used to create
     * any required CPU|GPU resources.
     * @param capsaicin Current framework context.
     * @return True if initialisation succeeded, False otherwise.
     */
    bool init(CapsaicinInternal const &capsaicin) noexcept override;

    /**
     * Run internal operations.
     * @param [in,out] capsaicin Current framework context.
     */
    void run(CapsaicinInternal &capsaicin) noexcept override;

    /**
     * Destroy any used internal resources and shutdown.
     */
    void terminate() noexcept override;

    /**
     * Add the required program parameters to a shader based on current settings.
     * @param capsaicin Current framework context.
     * @param program   The shader program to bind parameters to.
     */
    void addProgramParameters(CapsaicinInternal const &capsaicin, GfxProgram const &program) const noexcept;

private:
    GfxTexture m_irradianceProbeTexture;
    GfxTexture m_prefilteredEnvironmentMap;
    GfxTexture m_brdfLut;
    GfxBuffer  m_drawConstants;
    GfxProgram m_irradianceBakerProgram;
    GfxKernel  m_irradianceBakerKernel;
    GfxProgram m_prefilteredEnvironmentBakerProgram;
    GfxKernel  m_prefilteredEnvironmentBakerKernel;
};
} // namespace Capsaicin
