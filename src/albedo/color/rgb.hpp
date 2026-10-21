#pragma once

#include "albedo/core/real.hpp"

namespace albedo {

/// Linear RGB radiance or reflectance, with the sRGB / Rec. 709 primaries.
///
/// Values are unbounded and linear in light energy; conversion to display
/// values happens only at output (see `srgb.hpp`).
struct Rgb {
    Real r{};
    Real g{};
    Real b{};

    /// Exact per-channel equality, for exactly constructed colors; compare
    /// computed colors with a tolerance instead.
    [[nodiscard]] constexpr bool operator==(const Rgb&) const noexcept = default;

    /// Adds `other` per channel.
    constexpr Rgb& operator+=(Rgb other) noexcept
    {
        r += other.r;
        g += other.g;
        b += other.b;
        return *this;
    }

    /// Multiplies by `other` per channel, e.g. light filtered by a surface color.
    constexpr Rgb& operator*=(Rgb other) noexcept
    {
        r *= other.r;
        g *= other.g;
        b *= other.b;
        return *this;
    }

    /// Scales every channel by `scale`.
    constexpr Rgb& operator*=(Real scale) noexcept
    {
        r *= scale;
        g *= scale;
        b *= scale;
        return *this;
    }

    /// Divides every channel by `divisor`.
    constexpr Rgb& operator/=(Real divisor) noexcept
    {
        r /= divisor;
        g /= divisor;
        b /= divisor;
        return *this;
    }
};

/// Per-channel sum.
[[nodiscard]] constexpr Rgb operator+(Rgb a, Rgb b) noexcept
{
    return a += b;
}

/// Per-channel product.
[[nodiscard]] constexpr Rgb operator*(Rgb a, Rgb b) noexcept
{
    return a *= b;
}

/// Scales every channel by `scale`.
[[nodiscard]] constexpr Rgb operator*(Rgb color, Real scale) noexcept
{
    return color *= scale;
}

/// Scales every channel by `scale`.
[[nodiscard]] constexpr Rgb operator*(Real scale, Rgb color) noexcept
{
    return color *= scale;
}

/// Divides every channel by `divisor`.
[[nodiscard]] constexpr Rgb operator/(Rgb color, Real divisor) noexcept
{
    return color /= divisor;
}

/// Relative luminance (perceived brightness) using the Rec. 709 weights
/// from ITU-R BT.709, which match the sRGB primaries.
[[nodiscard]] constexpr Real luminance(Rgb color) noexcept
{
    return 0.2126 * color.r + 0.7152 * color.g + 0.0722 * color.b;
}

/// True when every channel is exactly zero, e.g. a path that can carry no more light.
[[nodiscard]] constexpr bool isBlack(Rgb color) noexcept
{
    return color.r == 0 && color.g == 0 && color.b == 0;
}

} // namespace albedo
