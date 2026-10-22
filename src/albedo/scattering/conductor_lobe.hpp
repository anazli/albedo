#pragma once

#include "albedo/color/rgb.hpp"
#include "albedo/core/point2.hpp"
#include "albedo/core/real.hpp"
#include "albedo/core/vector3.hpp"
#include "albedo/scattering/scatter_sample.hpp"

#include <optional>

namespace albedo {

/// Reflection from a metal surface: a GGX microfacet model with the conductor
/// Fresnel equations, or a perfect mirror when `alpha` is (nearly) zero.
///
/// Works in local space like every lobe: normal +z, `wo` towards the viewer and
/// `wi` towards the light, both unit vectors. Only directions with z > 0 on both
/// sides reflect. Single scattering only: light that would bounce between
/// microfacets more than once is lost, so very rough metals look slightly dark.
/// Precondition: `eta` positive and `k` non-negative per channel; `alpha`
/// finite and non-negative.
struct ConductorLobe {
    /// Real part of the metal's refractive index relative to the outside, per channel.
    Rgb eta{1, 1, 1};
    /// Imaginary part (absorption) of the index, per channel.
    Rgb k{};
    /// GGX width; below `smooth_alpha` the surface is treated as a perfect mirror.
    Real alpha{};

    /// Widths below this give a mirror: GGX that narrow is numerically unusable
    /// and visually indistinguishable from one.
    static constexpr Real smooth_alpha{1e-4};

    /// f(wo, wi). Black for a mirror, which reflects into a single direction only.
    [[nodiscard]] Rgb evaluate(Vector3r wo, Vector3r wi) const noexcept;

    /// A reflected direction for the random point `u` in [0, 1]^2, or nothing when
    /// `wo` is not above the surface or the sampled microfacet reflects below it.
    /// Mirror samples are marked `specular`.
    [[nodiscard]] std::optional<ScatterSample> sample(Vector3r wo, Point2r u) const noexcept;

    /// Density with which `sample` draws `wi` given `wo`; zero for a mirror.
    [[nodiscard]] Real pdf(Vector3r wo, Vector3r wi) const noexcept;
};

} // namespace albedo
