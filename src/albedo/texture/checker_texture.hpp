#pragma once

#include "albedo/color/rgb.hpp"
#include "albedo/core/real.hpp"
#include "albedo/scene/hit_record.hpp"

namespace albedo {

/// Squares of two alternating colors laid out in texture space, like a
/// checkerboard. Square (i, j) covers u in [i, i + 1) / frequency and v in
/// [j, j + 1) / frequency, up to the rounding of u * frequency and v * frequency;
/// it is even when i + j is even, and the square at the origin is even.
/// Precondition: `frequency` is positive and finite.
struct CheckerTexture {
    /// Color of the even squares.
    Rgb even{};
    /// Color of the odd squares.
    Rgb odd{};
    /// Squares per unit of u, and per unit of v.
    Real frequency{1};

    /// The color of the square holding the hit's texture coordinates. Where
    /// u * frequency or v * frequency is too large to have a fractional part, the
    /// squares no longer alternate, but a color is still returned; a NaN
    /// coordinate, or a product overflowing to infinity, gives the even color.
    [[nodiscard]] Rgb evaluate(const HitRecord& hit) const noexcept;
};

} // namespace albedo
