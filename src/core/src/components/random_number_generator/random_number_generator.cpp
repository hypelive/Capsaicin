/**********************************************************************
Copyright (c) 2025 Advanced Micro Devices, Inc. All rights reserved.

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

#include "random_number_generator.h"

#include "capsaicin_internal.h"
#include "components/stratified_sampler/stratified_sampler.h"

#include <random>

using namespace std;

namespace Capsaicin
{
RandomNumberGenerator::RandomNumberGenerator() noexcept
    : Component(Name)
{}

RandomNumberGenerator::~RandomNumberGenerator() noexcept
{
    terminate();
}

RenderOptionList RandomNumberGenerator::getRenderOptions() noexcept
{
    RenderOptionList newOptions;
    newOptions.emplace(RENDER_OPTION_MAKE(random_deterministic, options));
    newOptions.emplace(RENDER_OPTION_MAKE(random_seed, options));
    return newOptions;
}

RandomNumberGenerator::RenderOptions RandomNumberGenerator::convertOptions(
    RenderOptionList const &options) noexcept
{
    RenderOptions newOptions;
    RENDER_OPTION_GET(random_deterministic, newOptions, options)
    RENDER_OPTION_GET(random_seed, newOptions, options)
    return newOptions;
}

bool RandomNumberGenerator::init(CapsaicinInternal const &capsaicin) noexcept
{
    options = convertOptions(capsaicin.getOptions());
    if (!capsaicin.hasComponent("StratifiedSampler")
        || capsaicin.getOption<uint32_t>("stratified_sampler_seed") != options.random_seed
        || capsaicin.getOption<bool>("stratified_sampler_deterministic") != options.random_deterministic)
    {
        auto const     seedDimensions = max(capsaicin.getRenderDimensions(), uint2(1920, 1080));
        uint64_t const seedBufferSize = static_cast<uint64_t>(seedDimensions.x) * seedDimensions.y;

        vector<uint32_t> seedBufferData;
        seedBufferData.reserve(seedBufferSize);
        if (options.random_deterministic)
        {
            mt19937 gen(options.random_seed);
            for (uint32_t i = 0; i < seedBufferSize; ++i)
            {
                seedBufferData.push_back(gen());
            }
        }
        else
        {
            random_device rd;
            mt19937       gen(rd());
            for (uint32_t i = 0; i < seedBufferSize; ++i)
            {
                seedBufferData.push_back(gen());
            }
        }
        seedBuffer = gfxCreateBuffer<uint32_t>(
            gfx_, static_cast<uint32_t>(seedBufferData.size()), seedBufferData.data());
        seedBuffer.setName("RandomNumberGenerator_SeedBuffer");
    }
    else
    {
        // Reuse the stratified sampler seed buffer if it is available and matches our own
        seedBuffer = {};
    }
    return true;
}

void RandomNumberGenerator::run(CapsaicinInternal &capsaicin) noexcept
{
    // Check for option changed
    auto const optionsNew            = convertOptions(capsaicin.getOptions());
    bool       usingStratifiedBuffer = !seedBuffer;
    bool       update                = false;
    if (usingStratifiedBuffer)
    {
        usingStratifiedBuffer =
            capsaicin.getOption<uint32_t>("stratified_sampler_seed") == optionsNew.random_seed
            && capsaicin.getOption<bool>("stratified_sampler_deterministic")
                   == optionsNew.random_deterministic;
        if (!usingStratifiedBuffer)
        {
            init(capsaicin);
            update = true;
        }
    }
    else
    {
        // Check if seed buffer needs to be re-initialised
        auto const seedDimensions = max(capsaicin.getRenderDimensions(), uint2(1920, 1080));
        update                    = optionsNew.random_deterministic != options.random_deterministic
              || (options.random_deterministic && (optionsNew.random_seed != options.random_seed))
              || (sizeof(uint32_t) * seedDimensions.x * seedDimensions.y > seedBuffer.getSize());
    }
    options = optionsNew;

    if (update)
    {
        GfxCommandEvent const command_event(gfx_, "InitRandomNumberGenerator");

        gfxDestroyBuffer(gfx_, seedBuffer);

        init(capsaicin);
    }
}

void RandomNumberGenerator::terminate() noexcept
{
    gfxDestroyBuffer(gfx_, seedBuffer);
    seedBuffer = {};
}

void RandomNumberGenerator::addProgramParameters(
    [[maybe_unused]] CapsaicinInternal const &capsaicin, GfxProgram const &program) const noexcept
{
    if (!!seedBuffer)
    {
        gfxProgramSetParameter(gfx_, program, "g_RandomSeedBuffer", seedBuffer);
        gfxProgramSetParameter(gfx_, program, "g_RandomSeedBufferSize",
            static_cast<uint32_t>(seedBuffer.getSize() / sizeof(uint32_t)));
    }
    else
    {
        auto const stratifiedSampler =
            dynamic_pointer_cast<StratifiedSampler>(capsaicin.getComponent("StratifiedSampler"));
        gfxProgramSetParameter(gfx_, program, "g_RandomSeedBuffer", stratifiedSampler->seedBuffer);
        gfxProgramSetParameter(gfx_, program, "g_RandomSeedBufferSize",
            static_cast<uint32_t>(stratifiedSampler->seedBuffer.getSize() / sizeof(uint32_t)));
    }
}
} // namespace Capsaicin
