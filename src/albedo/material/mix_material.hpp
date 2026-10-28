#pragma once

#include "albedo/core/real.hpp"
#include "albedo/material/basic_material.hpp"
#include "albedo/scattering/scattering.hpp"
#include "albedo/scene/hit_record.hpp"
#include "albedo/texture/perlin_pattern.hpp"

namespace albedo {

/// Two materials laid out over a surface by a Perlin pattern: wherever the
/// pattern's weight at the hit is below `threshold` the surface is `first`,
/// elsewhere `second`. Each point is wholly one material, so a metal keeps its
/// full shine between the veins of another; the edges between them are
/// smoothed by pixel sampling like any other edge. Holding both materials by
/// value makes `Material`, and so every `SceneObject`, about twice as large;
/// sharing materials by index in `Scene` would remove that cost.
/// Precondition: `threshold` in [0, 1], and the pattern's own preconditions.
struct MixMaterial {
    /// The material where the weight is below `threshold`.
    BasicMaterial first;
    /// The material everywhere else.
    BasicMaterial second;
    /// Where each material lies; see `PerlinPattern`.
    PerlinPattern pattern;
    /// The weight at which the surface turns from `first` to `second`: 0 makes
    /// it all `second`, and higher values give `second` less of the surface.
    /// Even 1 leaves `second` wherever the weight reaches 1.
    Real threshold{0.5};

    /// The scattering of whichever material lies at the hit. Throws only what
    /// `scatteringAt(const MaterialVariant<...>&, ...)` throws.
    [[nodiscard]] Scattering scatteringAt(const HitRecord& hit) const;
};

} // namespace albedo
