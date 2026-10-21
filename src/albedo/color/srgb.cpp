#include "albedo/color/srgb.hpp"

#include <cmath>

namespace albedo {

namespace {

// Constants of the piecewise sRGB transfer function, IEC 61966-2-1:1999.
constexpr Real linear_segment_end{0.0031308};
constexpr Real linear_slope{12.92};
constexpr Real power_scale{1.055};
constexpr Real power_offset{0.055};
constexpr Real inverse_gamma{1 / 2.4};

} // namespace

Real encodeSrgb(Real linear) noexcept
{
    if (linear <= linear_segment_end) {
        return linear_slope * linear;
    }
    return power_scale * std::pow(linear, inverse_gamma) - power_offset;
}

std::uint8_t toByte(Real encoded) noexcept
{
    // `!(encoded > 0)` is also true for NaN.
    if (!(encoded > 0)) {
        return 0;
    }
    if (encoded >= 1) {
        return 255;
    }
    return static_cast<std::uint8_t>(std::lround(encoded * 255));
}

} // namespace albedo
