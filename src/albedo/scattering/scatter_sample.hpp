#pragma once

#include "albedo/color/rgb.hpp"
#include "albedo/core/real.hpp"
#include "albedo/core/vector3.hpp"

namespace albedo {

/// An incident direction drawn from a scattering function, with what an
/// estimator needs to use it.
struct ScatterSample {
    /// Unit incident direction, pointing away from the surface (local or world
    /// space, matching the function that produced it).
    Vector3r direction{};
    /// f(wo, wi) * |cos(theta_i)| / pdf, the factor by which the sample scales
    /// the light arriving along `direction`.
    Rgb weight{};
    /// Solid-angle density with which `direction` was drawn; always positive. For
    /// a `specular` sample, the probability of the discrete choice instead.
    Real pdf{};
    /// True when `direction` is the only one the function scatters into (a mirror
    /// or perfect refraction), so no density describes it and the function's
    /// `evaluate` and `pdf` are zero for it.
    bool specular{};
};

} // namespace albedo
