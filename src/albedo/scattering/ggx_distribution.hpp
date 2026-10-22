#pragma once

#include "albedo/core/point2.hpp"
#include "albedo/core/real.hpp"
#include "albedo/core/vector3.hpp"

namespace albedo {

/// The isotropic GGX (Trowbridge-Reitz) distribution of microfacet normals,
/// with Smith's masking-shadowing for it.
///
/// Works in local space, where the macrosurface normal is +z. `alpha` is the
/// width of the distribution: small values give near-mirror surfaces, 1 a very
/// rough one. Directions are unit vectors; `m` is a microfacet normal.
class GgxDistribution {
public:
    /// Precondition: `alpha` is positive and finite.
    explicit GgxDistribution(Real alpha) noexcept;

    /// The distribution's width.
    [[nodiscard]] Real alpha() const noexcept { return width; }

    /// D(m): density of microfacet normals per unit solid angle, zero for normals
    /// below the surface. The projected density D(m) m.z integrates to 1.
    /// Precondition: `m` has unit length.
    [[nodiscard]] Real density(Vector3r m) const noexcept;

    /// G1(w): fraction of the microfacets facing `w` that are visible from it,
    /// for `w` on either side of the surface. Zero at grazing.
    [[nodiscard]] Real masking(Vector3r w) const noexcept;

    /// G2(wo, wi): fraction visible from both directions, with the
    /// height-correlated form of Smith's model for reflection, where `wo` and
    /// `wi` lie on the same side of the surface. Transmission needs a different
    /// height-correlated form; do not use this for it. Callers check that the
    /// microfacet faces each direction as their geometry requires.
    [[nodiscard]] Real maskingShadowing(Vector3r wo, Vector3r wi) const noexcept;

    /// A microfacet normal visible from `wo`, distributed with density
    /// `visibleNormalPdf(wo, m)`, for the random point `u` in [0, 1]^2.
    /// Precondition: wo.z > 0.
    [[nodiscard]] Vector3r sampleVisibleNormal(Vector3r wo, Point2r u) const noexcept;

    /// Density of normals visible from `wo`: G1(wo) max(0, wo.m) D(m) / wo.z.
    /// Zero when `wo` or `m` is not above the surface.
    [[nodiscard]] Real visibleNormalPdf(Vector3r wo, Vector3r m) const noexcept;

private:
    // Smith's auxiliary function Lambda(w), from which the masking terms follow.
    [[nodiscard]] Real lambda(Vector3r w) const noexcept;

    Real width;
};

} // namespace albedo
