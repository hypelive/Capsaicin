#include "pbr_ray_tracer/pbr_ray_tracer_technique.h"

#include "capsaicin_internal.h"

Capsaicin::PBRRayTracerTechnique::PBRRayTracerTechnique()
    : RenderTechnique("PBR Ray Tracer Technique")
{}

Capsaicin::PBRRayTracerTechnique::~PBRRayTracerTechnique()
{
    /***** Must clean-up any created member variables/data               *****/
    terminate();
}

Capsaicin::RenderOptionList Capsaicin::PBRRayTracerTechnique::getRenderOptions() noexcept
{
    RenderOptionList newOptions;
    /***** Push any desired options to the returned list here (else just 'return {}')  *****/
    /***** Example (using provided helper RENDER_OPTION_MAKE):                         *****/
    /*****  newOptions.emplace(RENDER_OPTION_MAKE(my_technique_enable, options));      *****/
    return newOptions;
}

Capsaicin::PBRRayTracerTechnique::RenderOptions Capsaicin::PBRRayTracerTechnique::convertOptions(
    RenderOptionList const &options) noexcept
{
    /***** Optional function only required if actually providing RenderOptions *****/
    RenderOptions newOptions;
    /***** Used to convert options between external string/variant and internal data type 'RenderOptions'
     * *****/
    /***** Example: (using provided helper RENDER_OPTION_GET): *****/
    /*****  RENDER_OPTION_GET(my_technique_enable, newOptions, options); *****/
    return newOptions;
}

Capsaicin::ComponentList Capsaicin::PBRRayTracerTechnique::getComponents() const noexcept
{
    ComponentList components;
    /***** Push any desired Components to the returned list here (else just 'return {}' or dont override)
     * *****/
    /***** Example: if corresponding header is already included (using provided helper COMPONENT_MAKE):
     * *****/
    /*****  components.emplace_back(COMPONENT_MAKE(TypeOfComponent)); *****/
    return components;
}

Capsaicin::BufferList Capsaicin::PBRRayTracerTechnique::getBuffers() const noexcept
{
    BufferList buffers;
    /***** Push any desired Buffers to the returned list here (else just 'return {}' or dont override)
     * *****/
    return buffers;
}

Capsaicin::AOVList Capsaicin::PBRRayTracerTechnique::getAOVs() const noexcept
{
    AOVList aovs;
    aovs.push_back({"Color", AOV::Write});
    /***** Push any desired AOVs to the returned list here (else just 'return {}' or dont override) *****/
    return aovs;
}

Capsaicin::DebugViewList Capsaicin::PBRRayTracerTechnique::getDebugViews() const noexcept
{
    DebugViewList views;
    /***** Push any desired Debug Views to the returned list here (else just 'return {}' or dont override)
     * *****/
    return views;
}

bool Capsaicin::PBRRayTracerTechnique::init(CapsaicinInternal const &capsaicin) noexcept
{
    const char* programName = "render_techniques/pbr_ray_tracer/pbr_ray_tracer";

    m_gfxProgram = gfxCreateProgram(gfx_, programName, capsaicin.getShaderPath());

    return true;
}

void Capsaicin::PBRRayTracerTechnique::render(CapsaicinInternal &capsaicin) noexcept
{
    /***** If any options are provided they should be checked for changes here *****/
    /***** Example:                                                            *****/
    /*****  RenderOptions newOptions = convertOptions(capsaicin.getOptions()); *****/
    /*****  Check for changes and handle accordingly                           *****/
    /*****  options = newOptions;                                              *****/
    /***** Perform any required rendering operations here                      *****/
    /***** Debug Views can be checked with 'capsaicin.getCurrentDebugView()'   *****/
}

void Capsaicin::PBRRayTracerTechnique::terminate() noexcept
{
    /***** Cleanup any created CPU or GPU resources                     *****/
}

void Capsaicin::PBRRayTracerTechnique::renderGUI(CapsaicinInternal &capsaicin) const noexcept
{
    /***** Add any UI drawing commands here                             *****/
}
