#pragma once

#include "albedo/color/rgb.hpp"
#include "albedo/core/point2.hpp"
#include "albedo/core/real.hpp"
#include "albedo/core/vector3.hpp"
#include "albedo/scattering/scatter_sample.hpp"

#include <optional>

namespace albedo {

/// Ideal diffuse reflection: light leaves equally in every direction of the
/// hemisphere it arrived from, scaled by `albedo`.
///
/// Like every lobe it works in local space, where the surface normal is +z.
/// `wo` points from the surface towards the viewer and `wi` towards the light;
/// both are unit vectors. Only directions with z > 0 on both sides reflect.
struct LambertLobe {
    /// Fraction of incident light reflected, per channel. Values in [0, 1]
    /// conserve energy; nothing enforces that.
    Rgb albedo{};

    /// The scattering function f(wo, wi): albedo / pi for reflection, black otherwise.
    [[nodiscard]] Rgb evaluate(Vector3r wo, Vector3r wi) const noexcept;

    /// A cosine-distributed `wi` for the random point `u` in [0, 1]^2, or nothing
    /// when `wo` is not above the surface or `u` maps into the tangent plane (which
    /// only a boundary `u` can do). The weight is exactly `albedo`, since
    /// f * cos(theta) / pdf cancels analytically; it is not computed by dividing
    /// by the pdf, which becomes tiny near grazing.
    [[nodiscard]] std::optional<ScatterSample> sample(Vector3r wo, Point2r u) const noexcept;

    /// Density with which `sample` draws `wi` given `wo`: cos(theta_i) / pi for
    /// reflection, zero otherwise.
    [[nodiscard]] static Real pdf(Vector3r wo, Vector3r wi) noexcept;
};

} // namespace albedo
