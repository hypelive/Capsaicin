#pragma once

#include "components/component.h"

namespace Capsaicin
{
class CustomLightBuilder final
    : public Component
      , ComponentFactory::Registrar<CustomLightBuilder>
{
public:
    static constexpr std::string_view Name = "CustomLightBuilder";

    /** Constructor. */
    CustomLightBuilder() noexcept;

    ~CustomLightBuilder() noexcept override;

    CustomLightBuilder(CustomLightBuilder const &other)                = delete;
    CustomLightBuilder(CustomLightBuilder &&other) noexcept            = delete;
    CustomLightBuilder &operator=(CustomLightBuilder const &other)     = delete;
    CustomLightBuilder &operator=(CustomLightBuilder &&other) noexcept = delete;

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
    GfxBuffer m_lightsCountBuffer;
    GfxBuffer m_lightsBuffer;
};
} // namespace Capsaicin
