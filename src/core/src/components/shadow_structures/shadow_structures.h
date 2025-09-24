#pragma once

#include "components/component.h"

namespace Capsaicin
{
class ShadowStructures final
    : public Component
      , ComponentFactory::Registrar<ShadowStructures>
{
public:
    static constexpr std::string_view Name = "ShadowStructures";

    /** Constructor. */
    ShadowStructures() noexcept;

    ~ShadowStructures() noexcept override;

    ShadowStructures(ShadowStructures const& other)                = delete;
    ShadowStructures(ShadowStructures&& other) noexcept            = delete;
    ShadowStructures& operator=(ShadowStructures const& other)     = delete;
    ShadowStructures& operator=(ShadowStructures&& other) noexcept = delete;

    /**
     * Initialise any internal data or state.
     * @note This is automatically called by the framework after construction and should be used to create
     * any required CPU|GPU resources.
     * @param capsaicin Current framework context.
     * @return True if initialisation succeeded, False otherwise.
     */
    bool init(CapsaicinInternal const& capsaicin) noexcept override;

    /**
     * Run internal operations.
     * @param [in,out] capsaicin Current framework context.
     */
    void run(CapsaicinInternal& capsaicin) noexcept override;

    /**
     * Destroy any used internal resources and shutdown.
     */
    void terminate() noexcept override;

    /**
     * Add the required program parameters to a shader based on current settings.
     * @param capsaicin Current framework context.
     * @param program   The shader program to bind parameters to.
     */

    void addShadingParameters(CapsaicinInternal const& capsaicin, GfxProgram const& program) const noexcept;
    void clearResources();

    const GfxTexture& getVirtualPageTable() const { return m_virtualPageTable; }
    const GfxTexture& getPhysicalPages() const { return m_physicalPages; }

    glm::mat4x4 m_lightViewProjection;

private:
    GfxBuffer  m_shadowConstants;
    GfxTexture m_virtualPageTable;
    GfxTexture m_physicalPages;

    GfxProgram m_vptClearProgram;
    GfxKernel  m_vptClearKernel;
    GfxProgram m_ppClearProgram;
    GfxKernel  m_ppClearKernel;
};
} // namespace Capsaicin
