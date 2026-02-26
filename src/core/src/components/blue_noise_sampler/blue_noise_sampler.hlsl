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

#ifndef BLUE_NOISE_SAMPLER_HLSL
#define BLUE_NOISE_SAMPLER_HLSL

// Requires the following data to be defined in any shader that uses this file
StructuredBuffer<uint> g_SobolBuffer;
StructuredBuffer<uint> g_RankingTile;
StructuredBuffer<uint> g_ScramblingTile;
uint g_RandomSeed;

#define GOLDEN_RATIO 1.61803398874989484820f

namespace NoExport
{
    float samplerBlueNoiseErrorDistribution_128x128_OptimizedFor_2d2d2d2d_1spp(uint2 pixel, uint index, uint dimension)
    {
        // A Low-Discrepancy Sampler that Distributes Monte Carlo Errors as a Blue Noise in Screen Space - Heitz etal

        // Xor index based on optimized ranking
        uint rankedSampleIndex = index ^ g_RankingTile[dimension + (pixel.x + pixel.y * 128) * 8];

        // Fetch value in sequence
        uint value = g_SobolBuffer[dimension + rankedSampleIndex * 256];

        // If the dimension is optimized, xor sequence value based on optimized scrambling
        value = value ^ g_ScramblingTile[(dimension % 8) + (pixel.x + pixel.y * 128) * 8];

        // Convert to float and return
        return (0.5f + value) / 256.0f;
    }
}

/**
 * Generate random numbers using a blue noise sampling function.
 * Each new sample is taken from the next available blue noise sequence dimension.
 * This should only be used when each call to one of this classes rand functions is sampling from a different distribution.
 */
class BlueNoiseSampler
{
    uint2 pixel;
    uint index;
    uint dimension;

    /**
     * Generate a random number.
     * @return The new number (range [0, 1)).
     */
    float rand()
    {
        if (dimension >= 256)
        {
            dimension = 0;
        }
        float s = NoExport::samplerBlueNoiseErrorDistribution_128x128_OptimizedFor_2d2d2d2d_1spp(pixel, 0, dimension++);

        // https://blog.demofox.org/2017/10/31/animating-noise-for-integration-over-time/
        return fmod(s + (index & 255) * GOLDEN_RATIO, 1.0f);
    }

    /**
     * Generate 2 random numbers.
     * @return The new numbers (range [0, 1)).
     */
    float2 rand2()
    {
        if (dimension >= 255)
        {
            dimension = 0;
        }
        float s1 = NoExport::samplerBlueNoiseErrorDistribution_128x128_OptimizedFor_2d2d2d2d_1spp(pixel, 0, dimension++);
        float s2 = NoExport::samplerBlueNoiseErrorDistribution_128x128_OptimizedFor_2d2d2d2d_1spp(pixel, 0, dimension++);

        return fmod(float2(s1, s2) + (index & 255) * GOLDEN_RATIO, 1.0f);
    }

    /**
     * Generate 3 random numbers.
     * @return The new numbers (range [0, 1)).
     */
    float3 rand3()
    {
        if (dimension >= 254)
        {
            dimension = 0;
        }
        float s1 = NoExport::samplerBlueNoiseErrorDistribution_128x128_OptimizedFor_2d2d2d2d_1spp(pixel, 0, dimension++);
        float s2 = NoExport::samplerBlueNoiseErrorDistribution_128x128_OptimizedFor_2d2d2d2d_1spp(pixel, 0, dimension++);
        float s3 = NoExport::samplerBlueNoiseErrorDistribution_128x128_OptimizedFor_2d2d2d2d_1spp(pixel, 0, dimension++);

        return fmod(float3(s1, s2, s3) + (index & 255) * GOLDEN_RATIO, 1.0f);
    }
};

/**
 * Initialise a blue noise sample generator.
 * @param pixel     Pixel value to initialise random with.
 * @param index     Index into the sequence (e.g. frame number).
 * @param dimension (Optional) The dimension of the sequence to start at.
 * @return The new blue noise sampler.
 */
BlueNoiseSampler MakeBlueNoiseSampler(uint2 pixel, uint index, uint dimension = 0)
{
    BlueNoiseSampler ret = { pixel % 128, index, dimension + g_RandomSeed };
    return ret;
}

/**
 * Generate 1D number sequence using a progressive blue noise sampling function.
 * Each new sample is taken from the same blue noise sequence.
 */
class BlueNoiseSampler1D
{
    uint2 pixel;
    uint index;
    uint dimension;

    /**
     * Generate a random number.
     * @return The new number (range [0, 1)).
     */
    float rand()
    {
        float s = NoExport::samplerBlueNoiseErrorDistribution_128x128_OptimizedFor_2d2d2d2d_1spp(pixel, 0, dimension);
        return fmod(s + (index++ & 255) * GOLDEN_RATIO, 1.0f);
    }
};

/**
 * Initialise a 1D sequence blue noise sample generator.
 * @param pixel     Pixel value to initialise random with.
 * @param dimension The current dimension of the sequence (0-indexed).
 * @return The 1D new blue noise sampler.
 */
BlueNoiseSampler1D MakeBlueNoiseSampler1D(uint2 pixel, uint dimension = 0)
{
    BlueNoiseSampler1D ret =
    {
        pixel % 128,
        0,
        (dimension + g_RandomSeed) % 256,
    };
    return ret;
}

/**
 * Initialise a 1D sequence blue noise sample generator from a dimensional blue noise sampler.
 * This can be used to branch off from an existing BlueNoiseSampler and locally generate additional samples
 *  from within the same dimension.
 * @param strat  The dimensional sampler to initialise from.
 * @param offset (Optional) The number of values expected to be taken with this sampler that is used to offset the start index accordingly.
 * @return The new 1D blue noise sampler.
 */
BlueNoiseSampler1D MakeBlueNoiseSampler1D(BlueNoiseSampler strat, uint offset = 0)
{
    BlueNoiseSampler1D ret =
    {
        strat.pixel,
        strat.index * offset,
        strat.dimension,
    };
    return ret;
}

/**
 * Generate 2D number sequence using a progressive blue noise Sobol sampling function.
 * Each new sample is taken from the same Sobol sequence.
 */
class BlueNoiseSampler2D
{
    uint2 pixel;
    uint index;
    uint dimension;

    /**
     * Generate 2 random numbers.
     * @return The new numbers (range [0->1)).
     */
    float2 rand2()
    {
        float2 s = float2(NoExport::samplerBlueNoiseErrorDistribution_128x128_OptimizedFor_2d2d2d2d_1spp(pixel, 0, dimension),
        NoExport::samplerBlueNoiseErrorDistribution_128x128_OptimizedFor_2d2d2d2d_1spp(pixel, 0, dimension + 1));

        return fmod(s + (index++ & 255) * GOLDEN_RATIO, 1.0f);
    }
};

/**
 * Initialise a 2D sequence blue noise sample generator.
 * @param pixel     Pixel value to initialise random with.
 * @param dimension The current dimension of the sequence (0-indexed).
 * @return The 2D new blue noise sampler.
 */
BlueNoiseSampler2D MakeBlueNoiseSampler2D(uint2 pixel, uint dimension = 0)
{
    BlueNoiseSampler2D ret =
    {
        pixel % 128,
        0,
        (dimension + g_RandomSeed) % 255,
    };
    return ret;
}

/**
 * Initialise a 2D sequence blue noise sample generator from a dimensional blue noise sampler.
 * This can be used to branch off from an existing BlueNoiseSampler and locally generate additional samples
 *  from within the same dimension.
 * @param strat  The dimensional sampler to initialise from.
 * @param offset (Optional) The number of values expected to be taken with this sampler that is used to offset the start index accordingly.
 * @return The new 2D blue noise sampler.
 */
BlueNoiseSampler2D MakeBlueNoiseSampler2D(BlueNoiseSampler strat, uint offset = 0)
{
    BlueNoiseSampler2D ret =
    {
        strat.pixel,
        strat.index * offset,
        strat.dimension,
    };
    return ret;
}

#endif // BLUE_NOISE_SAMPLER_HLSL
