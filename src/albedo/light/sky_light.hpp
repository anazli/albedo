#pragma once

#include "albedo/color/rgb.hpp"
#include "albedo/core/vector3.hpp"

namespace albedo {

/// Light arriving from infinitely far away in every direction, blended linearly
/// with the direction's height from `nadir` (straight down, -y) to `zenith`
/// (straight up, +y). The default sky is black.
struct SkyLight {
    /// Radiance from straight below.
    Rgb nadir{};
    /// Radiance from straight above.
    Rgb zenith{};

    /// A sky of the same radiance in every direction, as a white furnace test needs.
    [[nodiscard]] static constexpr SkyLight uniform(Rgb radiance) noexcept
    {
        return {.nadir = radiance, .zenith = radiance};
    }

    /// Radiance seen by a ray travelling in `direction` that leaves the scene.
    /// Precondition: `direction` has unit length.
    [[nodiscard]] Rgb emittedRadiance(Vector3r direction) const noexcept;
};

} // namespace albedo
