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
#pragma once

#include "gpu_shared.h"

#include <dxgiformat.h>
#include <map>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace Capsaicin
{
template<typename T>
requires std::is_enum_v<T>
class BitMask
{
    using UnderlyingT = std::underlying_type_t<T>;

    UnderlyingT mask = 0;

    explicit constexpr BitMask(UnderlyingT o)
        : mask(o)
    {}

public:
    constexpr BitMask() = default;

    constexpr BitMask(BitMask const &other)                = default;
    constexpr BitMask(BitMask &&other) noexcept            = default;
    constexpr BitMask &operator=(BitMask const &other)     = default;
    constexpr BitMask &operator=(BitMask &&other) noexcept = default;

    constexpr ~BitMask() = default;

    constexpr BitMask(T const type) // NOLINT(hicpp-explicit-conversions)
        : mask(static_cast<UnderlyingT>(type))
    {}

    constexpr BitMask operator|(T const type) const { return BitMask(mask | static_cast<UnderlyingT>(type)); }

    constexpr BitMask operator|(BitMask const other) const { return BitMask(mask | other.mask); }

    constexpr BitMask operator^(T const type) const { return BitMask(mask ^ static_cast<UnderlyingT>(type)); }

    constexpr BitMask operator^(BitMask const other) const { return BitMask(mask ^ other.mask); }

    constexpr bool operator&(T const type) const { return (mask & static_cast<UnderlyingT>(type)) != 0; }

    constexpr bool operator==(T const type) const { return mask == static_cast<UnderlyingT>(type); }

    constexpr bool operator!=(T const type) const { return mask != static_cast<UnderlyingT>(type); }
};

/**
 * Type used to pass information about requested shared textures between capsaicin and render techniques.
 */
struct SharedTexture
{
    std::string_view name; /**< The name to identify the shared texture */

    enum class Access : uint8_t
    {
        Read,
        Write,
        ReadWrite,
    };

    BitMask<Access> access = Access::Read; /**< The type of access pattern a render technique requires */

    enum class Flags : uint8_t
    {
        None       = 0,      /**< Use default values */
        Clear      = 1 << 1, /**< Clear texture every frame */
        Accumulate = 1 << 2, /**< Allow the texture to accumulate over frames (this is used for
                                error checking to prevent the frame being cleared) */
        Optional = 1 << 3,   /**< Texture is allowed to not exist, only created if connection is made */
        OptionalDiscard =
            1 << 4, /**< Texture is allowed to not exist, only created if non-optional request is made */
        OptionalKeep = 1 << 5, /**< Texture is allowed to not exist, always created if possible */
    };

    BitMask<Flags> flags = Flags::None;

    DXGI_FORMAT format = DXGI_FORMAT_UNKNOWN; /**< The internal buffer format (If using read then
                                                       format can be set to unknown to use auto setup) */
    uint2 dimensions = uint2(
        0, 0); /**< The width and height of texture (if any are set to 0 then will b sized to the window) */
    bool             mips = false; /**< True to allocate space for mip maps */
    std::string_view backup_name =
        ""; /**< The name to identify the texture backup (blank if no backup required) */
    std::string_view require = ""; /**< Conditional string used to specify requirements for creation (only
                                      effects Optional textures) */

    bool operator==(SharedTexture const &other) const noexcept { return name == other.name; }
};

constexpr BitMask<SharedTexture::Flags> operator|(
    SharedTexture::Flags const flags, SharedTexture::Flags const flags2) noexcept
{
    return BitMask(flags) | flags2;
}

constexpr BitMask<SharedTexture::Flags> operator^(
    SharedTexture::Flags const flags, SharedTexture::Flags const flags2) noexcept
{
    return BitMask(flags) ^ flags2;
}

using SharedTextureList = std::vector<SharedTexture>;

using DebugViewList = std::vector<std::string_view>;

/**
 * Type used to pass information about requested shared buffers between capsaicin and render techniques.
 */
struct SharedBuffer
{
    std::string_view name; /**< The name to identify the buffer */

    enum class Access : uint8_t
    {
        Read,
        Write,
        ReadWrite,
    };

    BitMask<Access> access = Access::Read; /**< The type of access pattern a render technique requires */

    enum class Flags : uint8_t
    {
        None       = 0,      /**< Use default values */
        Clear      = 1 << 1, /**< Clear buffer every frame */
        Accumulate = 1 << 2, /**< Allow the buffer to accumulate over frames (this is used for
                                error checking to prevent the frame being cleared) */
        Optional = 1 << 3,   /**< Buffer is allowed to not exist, only created if connection is made */
        OptionalDiscard =
            1 << 4, /**< Buffer is allowed to not exist, only created if non-optional request is made */
        OptionalKeep = 1 << 5, /**< Buffer is allowed to not exist, always created if possible */
        Allocate     = 1 << 6, /**< Current technique/component will be responsible for allocating buffer */
    };

    BitMask<Flags> flags = Flags::None;

    size_t           size    = 0;  /**< The size of the buffer in Bytes */
    uint32_t         stride  = 0;  /**< The size in bytes of each element to be held in the buffer */
    std::string_view require = ""; /**< Conditional string used to specify requirements for creation (only
                                      effects Optional buffers) */

    bool operator==(SharedBuffer const &other) const noexcept { return name == other.name; }
};

constexpr BitMask<SharedBuffer::Flags> operator|(
    SharedBuffer::Flags const flags, SharedBuffer::Flags const flags2) noexcept
{
    return BitMask(flags) | flags2;
}

constexpr BitMask<SharedBuffer::Flags> operator^(
    SharedBuffer::Flags const flags, SharedBuffer::Flags const flags2) noexcept
{
    return BitMask(flags) ^ flags2;
}

using SharedBufferList = std::vector<SharedBuffer>;

using ComponentList = std::vector<std::string_view>;

/**
 * A macro for easy creation of render options from a struct.
 * @param  variable The member variable name.
 * @param  default  The instance of the struct containing default values.
 */
#define RENDER_OPTION_MAKE(variable, default) #variable, default.variable

/**
 * A macro for easy conversion of render options back to a struct.
 * @param  variable The member variable name.
 * @param  ret      The instance of the struct to return values in.
 * @param  options  The instance of render options to read value from.
 */
#define RENDER_OPTION_GET(variable, ret, options) \
    ret.variable = *std::get_if<decltype((ret).variable)>(&(options).at(#variable));

#define COMPONENT_MAKE(type) ComponentFactory::Registrar<type>::registeredName<>

using Option           = std::variant<bool, uint32_t, int32_t, uint8_t, float, std::string>;
using RenderOptionList = std::map<std::string_view, Option>;
} // namespace Capsaicin
