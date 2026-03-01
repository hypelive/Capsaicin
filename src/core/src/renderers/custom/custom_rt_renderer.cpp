#include "renderer.h"
#include "ct_ray_tracer/ct_ray_tracer.h"

namespace Capsaicin
{
/** The GI-1 renderer. */
class CtRtRenderer final
    : public Renderer
    , RendererFactory::Registrar<CtRtRenderer>
{
public:
    static constexpr std::string_view Name = "Custom RT Renderer";

    /** Default constructor. */
    CtRtRenderer() noexcept {}

    /**
     * Sets up the required render techniques.
     * @param renderOptions The current global render options.
     * @return A list of all required render techniques in the order that they are required. The calling
     * function takes all ownership of the returned list.
     */
    std::vector<std::unique_ptr<RenderTechnique>> setupRenderTechniques(
        [[maybe_unused]] RenderOptionList const &renderOptions) noexcept override
    {
        std::vector<std::unique_ptr<RenderTechnique>> render_techniques;
        render_techniques.emplace_back(std::make_unique<CtRayTracer>());
        return render_techniques;
    }
};
} // namespace Capsaicin
