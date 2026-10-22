#pragma once

#include "albedo/color/rgb.hpp"
#include "albedo/core/point2.hpp"
#include "albedo/core/real.hpp"
#include "albedo/core/vector3.hpp"
#include "albedo/scattering/scatter_sample.hpp"

#include <optional>

namespace albedo {

/// Reflection and refraction at the boundary of a transparent medium such as
/// glass: a GGX microfacet model with the dielectric Fresnel equations (Walter et
/// al., "Microfacet Models for Refraction through Rough Surfaces", EGSR 2007), or
/// a perfectly smooth boundary when `alpha` is (nearly) zero.
///
/// Works in local space with the normal +z on `wo`'s side; `wi` may lie on
/// either side. Transmission is the BTDF for radiance: a path from `wo` entering
/// a medium of relative index eta is weighted 1 / eta^2, since radiance leaving
/// that medium towards `wo` shrinks by that factor. Matched indices (eta = 1)
/// count as smooth at any roughness. Single scattering only, as for
/// `ConductorLobe`. Precondition: `eta` positive and finite; `alpha` finite and
/// non-negative.
struct DielectricLobe {
    /// Index of refraction on the far side divided by the index on `wo`'s side.
    Real eta{1.5};
    /// GGX width; below `smooth_alpha` the boundary is treated as perfectly smooth.
    Real alpha{};

    /// Widths below this give a smooth boundary, as for `ConductorLobe`.
    static constexpr Real smooth_alpha{1e-4};

    /// f(wo, wi) for reflection (wi.z > 0) or transmission (wi.z < 0). Black for
    /// a smooth boundary, which scatters into single directions only.
    [[nodiscard]] Rgb evaluate(Vector3r wo, Vector3r wi) const noexcept;

    /// A reflected or transmitted direction, or nothing when `wo` is not above
    /// the surface or the sampled microfacet sends light the wrong way.
    /// Reflection is chosen when `u_choice` is below the Fresnel reflectance of
    /// the sampled microfacet, so each is picked in proportion to the energy it
    /// carries; `u` places the microfacet normal. Smooth samples are `specular`.
    [[nodiscard]] std::optional<ScatterSample> sample(Vector3r wo, Real u_choice,
                                                      Point2r u) const noexcept;

    /// Density with which `sample` draws `wi` given `wo`, including the
    /// probability of choosing reflection or transmission; zero when smooth.
    [[nodiscard]] Real pdf(Vector3r wo, Vector3r wi) const noexcept;
};

} // namespace albedo
