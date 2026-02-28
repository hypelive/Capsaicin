#include "ct_ray_tracer.h"

#include "capsaicin_internal.h"

#include <string_view>

namespace
{
constexpr std::string_view PROGRAM_NAME = "render_techniques/ct_ray_tracer/ct_ray_tracer";
} // namespace

namespace Capsaicin
{
CtRayTracer::CtRayTracer()
    : RenderTechnique("Ray Tracer")
{}

CtRayTracer::~CtRayTracer()
{
    CtRayTracer::terminate();
}

RenderOptionList CtRayTracer::getRenderOptions() noexcept
{
    RenderOptionList newOptions;
    return newOptions;
}

CtRayTracer::RenderOptions CtRayTracer::convertOptions(
    [[maybe_unused]] const RenderOptionList& options) noexcept
{
    RenderOptions newOptions;
    return newOptions;
}

ComponentList CtRayTracer::getComponents() const noexcept
{
    ComponentList components;
    return components;
}

SharedTextureList CtRayTracer::getSharedTextures() const noexcept
{
    SharedTextureList textures;
    textures.push_back({"Color", SharedTexture::Access::Write});
    return textures;
}

bool CtRayTracer::init([[maybe_unused]] const CapsaicinInternal& capsaicin) noexcept
{
    return true;
}

void CtRayTracer::render(CapsaicinInternal& capsaicin) noexcept
{
    [[maybe_unused]] const RenderOptions newOptions = convertOptions(capsaicin.getOptions());

    const auto scene = capsaicin.getScene();
    const auto*    meshes    = gfxSceneGetMeshes(scene);
    const uint32_t numMeshes = gfxSceneGetMeshCount(scene);
    for (uint32_t meshIndex = 0u; meshIndex < numMeshes; ++meshIndex)
    {
        const auto mesh = meshes[meshIndex];
    }
}

void CtRayTracer::terminate() noexcept {}

void CtRayTracer::renderGUI([[maybe_unused]] CapsaicinInternal& capsaicin) const noexcept {}
} // namespace Capsaicin
