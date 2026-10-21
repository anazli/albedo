#pragma once

#include "albedo/core/point2.hpp"
#include "albedo/core/real.hpp"
#include "albedo/core/vector3.hpp"

namespace albedo {

/// Maps the unit square onto the unit disk, preserving relative area and keeping
/// nearby inputs nearby. Precondition: `u` lies in [0, 1]^2.
[[nodiscard]] Point2r sampleConcentricDisk(Point2r u) noexcept;

/// A direction in the hemisphere around +z, distributed with density
/// cos(theta) / pi, as an ideal diffuse surface scatters light.
/// Precondition: `u` lies in [0, 1]^2. Postcondition: z >= 0. When `u` lies on the
/// square's boundary (a generator returning exactly 0 can produce that), z is 0 or,
/// after rounding, within about 1e-8 of it, and so is `cosineHemispherePdf(z)`.
/// Callers should not divide by that pdf; for a cosine-weighted integrand, let the
/// cosine cancel analytically instead.
[[nodiscard]] Vector3r sampleCosineHemisphere(Point2r u) noexcept;

/// Density of `sampleCosineHemisphere` for a direction with the given cos(theta)
/// to +z; zero below the surface.
[[nodiscard]] constexpr Real cosineHemispherePdf(Real cos_theta) noexcept
{
    return cos_theta > 0 ? cos_theta * inv_pi : Real{0};
}

/// A direction distributed uniformly over the whole sphere.
/// Precondition: `u` lies in [0, 1]^2.
[[nodiscard]] Vector3r sampleUniformSphere(Point2r u) noexcept;

/// Density of `sampleUniformSphere`: 1 / (4 pi) for every direction.
[[nodiscard]] constexpr Real uniformSpherePdf() noexcept
{
    return inv_pi / 4;
}

} // namespace albedo
