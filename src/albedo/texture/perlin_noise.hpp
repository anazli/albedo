#pragma once

#include "albedo/core/point3.hpp"
#include "albedo/core/real.hpp"

namespace albedo {

/// Number of octaves `turbulence` sums.
inline constexpr int turbulence_octaves{7};

/// Ken Perlin's improved gradient noise ("Improving Noise", SIGGRAPH 2002) at
/// `point`: smooth, in [-1, 1], zero at every integer point, and repeating
/// every 256 units along each axis. Deterministic, so every thread and every
/// run sees the same pattern. A non-finite coordinate gives 0.
[[nodiscard]] Real perlinNoise(Point3r point) noexcept;

/// The sum of |noise| over `turbulence_octaves` octaves, each at twice the
/// frequency and half the weight of the one before: in [0, 2), and zero at
/// every integer point. A non-finite coordinate gives 0.
[[nodiscard]] Real turbulence(Point3r point) noexcept;

} // namespace albedo
