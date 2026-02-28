#pragma once

#include "render_technique.h"

#include <gfx_scene.h>

namespace Capsaicin
{
class CtRayTracer : public RenderTechnique
{
public:
    CtRayTracer();
    ~CtRayTracer() override;

    CtRayTracer(const CtRayTracer& other)                = delete;
    CtRayTracer(CtRayTracer&& other) noexcept            = delete;
    CtRayTracer& operator=(const CtRayTracer& other)     = delete;
    CtRayTracer& operator=(CtRayTracer&& other) noexcept = delete;

    /*
     * Gets configuration options for current technique.
     * @return A list of all valid configuration options.
     */
    RenderOptionList getRenderOptions() noexcept override;

    struct RenderOptions
    {};

    /**
     * Convert render options to internal options format.
     * @param options Current render options.
     * @return The options converted.
     */
    static RenderOptions convertOptions(const RenderOptionList& options) noexcept;

    /**
     * Gets a list of any shared components used by the current render technique.
     * @return A list of all supported components.
     */
    [[nodiscard]] ComponentList getComponents() const noexcept override;

    /**
     * Gets the required list of shared textures needed for the current render technique.
     * @return A list of all required shared textures.
     */
    [[nodiscard]] SharedTextureList getSharedTextures() const noexcept override;

    /**
     * Initialise any internal data or state.
     * @note This is automatically called by the framework after construction and should be used to create
     * any required CPU|GPU resources.
     * @param capsaicin Current framework context.
     * @return True if initialisation succeeded, False otherwise.
     */
    bool init(const CapsaicinInternal& capsaicin) noexcept override;

    /**
     * Perform render operations.
     * @param [in,out] capsaicin The current capsaicin context.
     */
    void render(CapsaicinInternal& capsaicin) noexcept override;

    /**
     * Destroy any used internal resources and shutdown.
     */
    void terminate() noexcept override;

    /**
     * Render GUI options.
     * @param [in,out] capsaicin The current capsaicin context.
     */
    void renderGUI(CapsaicinInternal& capsaicin) const noexcept override;

protected:
    RenderOptions options;
};
} // namespace Capsaicin
