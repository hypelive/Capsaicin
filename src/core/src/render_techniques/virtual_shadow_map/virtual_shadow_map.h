#pragma once

#include "render_technique.h"

namespace Capsaicin
{
class VirtualShadowMap final : public RenderTechnique
{
public:
    VirtualShadowMap();
    ~VirtualShadowMap() override;

    VirtualShadowMap(VirtualShadowMap const &other)                = delete;
    VirtualShadowMap(VirtualShadowMap &&other) noexcept            = delete;
    VirtualShadowMap &operator=(VirtualShadowMap const &other)     = delete;
    VirtualShadowMap &operator=(VirtualShadowMap &&other) noexcept = delete;

    /*
     * Gets configuration options for current technique.
     * @return A list of all valid configuration options.
     */
    RenderOptionList getRenderOptions() noexcept override;

    struct RenderOptions { };

    /**
     * Convert render options to internal options format.
     * @param options Current render options.
     * @return The options converted.
     */
    static RenderOptions convertOptions(RenderOptionList const &options) noexcept;

    /**
     * Gets a list of any shared components used by the current render technique.
     * @return A list of all supported components.
     */
    [[nodiscard]] ComponentList getComponents() const noexcept override;

    /**
     * Gets a list of any shared buffers used by the current render technique.
     * @return A list of all supported buffers.
     */
    [[nodiscard]] SharedBufferList getSharedBuffers() const noexcept override;

    /**
     * Gets the required list of shared textures needed for the current render technique.
     * @return A list of all required shared textures.
     */
    [[nodiscard]] SharedTextureList getSharedTextures() const noexcept override;

    /**
     * Gets a list of any debug views provided by the current render technique.
     * @return A list of all supported debug views.
     */
    [[nodiscard]] DebugViewList getDebugViews() const noexcept override;

    /**
     * Initialise any internal data or state.
     * @note This is automatically called by the framework after construction and should be used to create
     * any required CPU|GPU resources.
     * @param capsaicin Current framework context.
     * @return True if initialisation succeeded, False otherwise.
     */
    bool init(CapsaicinInternal const &capsaicin) noexcept override;

    /**
     * Perform render operations.
     * @param [in,out] capsaicin The current capsaicin context.
     */
    void render(CapsaicinInternal &capsaicin) noexcept override;

    /**
     * Destroy any used internal resources and shutdown.
     */
    void terminate() noexcept override;

    /**
     * Render GUI options.
     * @param [in,out] capsaicin The current capsaicin context.
     */
    void renderGUI(CapsaicinInternal &capsaicin) const noexcept override;

private:
    RenderOptions m_options;

    GfxBuffer  m_allocationsState;
    GfxBuffer  m_drawDataBuffer;
    GfxBuffer  m_drawConstantsBuffer;
    // Ring buffers.
    GfxBuffer  m_pendingVisiblePages;
    GfxBuffer  m_unusedPages;
    GfxBuffer  m_invalidPages;

    GfxProgram m_resetVisibleProgram;
    GfxKernel  m_resetVisibleKernel;
    GfxProgram m_markVisiblePagesProgram;
    GfxKernel  m_markVisiblePagesKernel;
    GfxProgram m_markUnusedPagesProgram;
    GfxKernel  m_markUnusedPagesKernel;
    GfxProgram m_allocatePhysicalPagesProgram;
    GfxKernel  m_allocatePhysicalPagesKernel;
    GfxProgram m_renderingProgram;
    GfxKernel  m_renderingKernel;

    GfxProgram m_debugProgram;
    GfxKernel  m_debugKernel;
    GfxTexture m_debugTexture;
};
} // namespace Capsaicin
