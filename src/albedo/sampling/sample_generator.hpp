#pragma once

#include "albedo/core/point2.hpp"
#include "albedo/core/real.hpp"
#include "albedo/sampling/pcg32.hpp"

#include <cassert>
#include <cstdint>

namespace albedo {

namespace detail {

/// The SplitMix64 output function (Steele, Lea and Flood, "Fast Splittable
/// Pseudorandom Number Generators", OOPSLA 2014; constants as in Vigna's
/// splitmix64.c). Scrambles nearby integers into unrelated 64-bit values.
[[nodiscard]] constexpr std::uint64_t mixBits(std::uint64_t value) noexcept
{
    value = (value ^ (value >> 30U)) * 0xbf58476d1ce4e5b9ULL;
    value = (value ^ (value >> 27U)) * 0x94d049bb133111ebULL;
    return value ^ (value >> 31U);
}

} // namespace detail

/// Supplies the random numbers one pixel's samples consume.
///
/// Each pixel gets its own PCG32 stream, chosen from its position, so the
/// numbers a pixel sees do not depend on the order in which pixels are
/// rendered; images are therefore identical however the work is split up.
class SampleGenerator {
public:
    /// The generator for `pixel` of an image `image_width` pixels wide, under the
    /// render-wide `seed`. Precondition: 0 <= pixel.x < image_width and pixel.y >= 0.
    [[nodiscard]] static constexpr SampleGenerator forPixel(std::uint64_t seed, Point2i pixel,
                                                            int image_width) noexcept
    {
        assert(pixel.x >= 0 && pixel.x < image_width && pixel.y >= 0 && "pixel outside the image");
        const auto pixel_index{static_cast<std::uint64_t>(pixel.y) *
                                   static_cast<std::uint64_t>(image_width) +
                               static_cast<std::uint64_t>(pixel.x)};
        // PCG streams with closely related selectors can be correlated (Vigna's
        // analysis of PCG), so consecutive pixel indices are hashed first.
        return SampleGenerator{Pcg32{{.seed = seed, .stream = detail::mixBits(pixel_index)}}};
    }

    /// A uniformly distributed value in [0, 1).
    [[nodiscard]] constexpr Real next1D() noexcept { return random.nextReal(); }

    /// A uniformly distributed point in [0, 1)^2; x is drawn before y.
    [[nodiscard]] constexpr Point2r next2D() noexcept
    {
        const Real x{random.nextReal()};
        return {.x = x, .y = random.nextReal()};
    }

private:
    explicit constexpr SampleGenerator(Pcg32 generator) noexcept : random{generator} {}

    Pcg32 random;
};

} // namespace albedo
