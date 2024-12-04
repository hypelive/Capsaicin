#include "renderer.h"
/***** Include and headers for used render techniques here *****/

namespace Capsaicin
{
class PBRRayTracer
	: public Renderer
    , public RendererFactory::Registrar<PBRRayTracer>
{
public:
	/***** Must define unique name to represent new type *****/
    static constexpr std::string_view Name = "PBR Ray Tracer";

	/***** Must have empty constructor *****/
    PBRRayTracer() noexcept {}

    std::vector<std::unique_ptr<RenderTechnique>> setupRenderTechniques(
        RenderOptionList const &renderOptions) noexcept override
    {
        for (auto &renderOption : renderOptions)
        {
            if (renderOption.first == "")
            {
                break;
            }
        }

        std::vector<std::unique_ptr<RenderTechnique>> render_techniques;
        return render_techniques;
    }

private:
};
} // namespace Capsaicin
