#pragma once

#include "albedo/color/rgb.hpp"
#include "albedo/core/real.hpp"
#include "albedo/scene/hit_record.hpp"

namespace albedo {

/// A solid texture of Perlin noise between two colors, looked up by the hit's
/// world-space position, so it needs no texture coordinates and flows across
/// the seams of a mesh. Without veins it is cloudy: the blend weight is the
/// turbulence at `scale` times the position, clamped to 1. With veins it is
/// marble: the weight is (1 + sin(scale * x + veins * turbulence)) / 2, bands
/// across x warped by the turbulence.
/// Precondition: `scale` positive and finite, `veins` finite and non-negative.
struct PerlinTexture {
    /// The color where the weight is 0.
    Rgb low{};
    /// The color where the weight is 1.
    Rgb high{1, 1, 1};
    /// Noise cells per world unit: larger values give a finer pattern.
    Real scale{1};
    /// How strongly the turbulence warps the marble bands; 0 gives clouds.
    Real veins{0};

    /// The color at the hit's position. A non-finite position, or one so large
    /// that scaling overflows, gives the low color for clouds and the midpoint
    /// of the two colors for marble.
    [[nodiscard]] Rgb evaluate(const HitRecord& hit) const noexcept;
};

} // namespace albedo
