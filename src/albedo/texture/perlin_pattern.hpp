#pragma once

#include "albedo/core/point3.hpp"
#include "albedo/core/real.hpp"

namespace albedo {

/// A weight in [0, 1] that varies over space by Perlin noise, for blending
/// colors or choosing materials. Without veins it is cloudy: the turbulence at
/// `scale` times the point, clamped to 1. With veins it is marble:
/// (1 + sin(scale * x + veins * turbulence)) / 2, bands across x warped by the
/// turbulence.
/// Precondition: `scale` positive and finite, `veins` finite and non-negative.
struct PerlinPattern {
    /// Noise cells per world unit: larger values give a finer pattern.
    Real scale{1};
    /// How strongly the turbulence warps the marble bands; 0 gives clouds.
    Real veins{0};

    /// The weight at `point`. A non-finite point, or one so large that scaling
    /// overflows, gives 0 for clouds and 0.5 for marble.
    [[nodiscard]] Real weightAt(Point3r point) const noexcept;
};

} // namespace albedo
