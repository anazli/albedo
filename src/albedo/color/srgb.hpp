#pragma once

#include "albedo/core/real.hpp"

#include <cstdint>

namespace albedo {

/// Applies the sRGB transfer function (IEC 61966-2-1) to a linear value in
/// [0, 1], giving the non-linear value displays expect. Values outside [0, 1]
/// follow the same formula; clamp them with `toByte` afterwards.
[[nodiscard]] Real encodeSrgb(Real linear) noexcept;

/// Quantizes an encoded value to a byte: clamps to [0, 1], scales to [0, 255]
/// and rounds to nearest. NaN maps to 0, so a rendering bug produces a
/// deterministic black pixel instead of an unspecified value.
[[nodiscard]] std::uint8_t toByte(Real encoded) noexcept;

} // namespace albedo
