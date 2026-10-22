#pragma once

#include "albedo/color/rgb.hpp"
#include "albedo/core/point2.hpp"
#include "albedo/core/point3.hpp"
#include "albedo/core/real.hpp"
#include "albedo/core/vector3.hpp"
#include "albedo/light/light_sample.hpp"
#include "albedo/sampling/warps.hpp"

#include <optional>

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

    /// A direction uniform over the whole sphere for the random point `u` in
    /// [0, 1]^2, with the sky radiance seen along it; `point` does not matter for
    /// light from infinitely far away. Uniform sampling ignores the gradient, a
    /// simple choice that suits the smooth skies used so far.
    [[nodiscard]] std::optional<LightSample> sampleIncidentLight(Point3r point,
                                                                 Point2r u) const noexcept;

    /// Density of `sampleIncidentLight`: 1 / (4 pi) for every direction.
    [[nodiscard]] static constexpr Real incidentLightPdf(Point3r /*point*/,
                                                         Vector3r /*direction*/) noexcept
    {
        return uniformSpherePdf();
    }
};

} // namespace albedo
