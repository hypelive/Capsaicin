#include "renderer.h"
#include "custom_shading/custom_shading.h"
#include "custom_visibility_buffer/custom_visibility_buffer.h"
#include "custom_skybox/custom_skybox.h"
#include "custom_tone_mapping/custom_tone_mapping.h"
#include "custom_visibility_to_gbuffer/custom_visibility_to_gbuffer.h"
#include "depth_copy/depth_copy.h"
#include "fxaa/fxaa.h"
#include "ssao/ssao.h"

namespace Capsaicin
{
/** The GI-1 renderer. */
class CustomRenderer final
    : public Renderer
    , RendererFactory::Registrar<CustomRenderer>
{
public:
    static constexpr std::string_view Name = "Custom Renderer";

    /** Default constructor. */
    CustomRenderer() noexcept {}

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
        render_techniques.emplace_back(std::make_unique<CustomVisibilityBuffer>());
        render_techniques.emplace_back(std::make_unique<DepthCopy>());
        render_techniques.emplace_back(std::make_unique<CustomVisibilityToGBuffer>());
        render_techniques.emplace_back(std::make_unique<SSAO>());
        render_techniques.emplace_back(std::make_unique<CustomShading>());
        render_techniques.emplace_back(std::make_unique<CustomSkybox>());
        render_techniques.emplace_back(std::make_unique<CustomToneMapping>());
        render_techniques.emplace_back(std::make_unique<FXAA>());
        return render_techniques;
    }
};
} // namespace Capsaicin
