#pragma once

#include "albedo/color/rgb.hpp"
#include "albedo/core/point2.hpp"
#include "albedo/core/point3.hpp"
#include "albedo/core/real.hpp"
#include "albedo/core/vector3.hpp"
#include "albedo/light/light_sample.hpp"

#include <optional>

namespace albedo {

/// An infinitely small light radiating equally in every direction. Rays can
/// never hit it, so it is only found by sampling it.
struct PointLight {
    /// Where the light is.
    Point3r position{};
    /// Radiant intensity (power per unit solid angle), per channel.
    Rgb intensity{};

    /// The direction towards the light from `point`, with the inverse-square
    /// falloff applied; `u` is unused. Nothing when `point` is at the light.
    [[nodiscard]] std::optional<LightSample> sampleIncidentLight(Point3r point,
                                                                 Point2r u) const noexcept;

    /// Always zero: no direction found by other means hits a point exactly.
    [[nodiscard]] static constexpr Real incidentLightPdf(Point3r /*point*/,
                                                         Vector3r /*direction*/) noexcept
    {
        return 0;
    }
};

} // namespace albedo
