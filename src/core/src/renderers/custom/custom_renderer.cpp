/**********************************************************************
Copyright (c) 2024 Advanced Micro Devices, Inc. All rights reserved.

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in
all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.  IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
THE SOFTWARE.
********************************************************************/

#include "renderer.h"
#include "custom_visibility_buffer/custom_visibility_buffer.h"
#include "custom_skybox/custom_skybox.h"

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
        render_techniques.emplace_back(std::make_unique<CustomSkybox>());
        return render_techniques;
    }
};
} // namespace Capsaicin
