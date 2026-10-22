#pragma once

#include "albedo/color/rgb.hpp"
#include "albedo/core/real.hpp"

namespace albedo {

/// Fraction of unpolarized light reflected at a smooth boundary between two
/// dielectrics. `cos_theta_i` is the cosine between the incident direction and
/// the normal, positive on the normal's side; `eta` is the index on the far
/// side divided by the index on the normal's side. Light arriving from the
/// other side (negative cosine) sees the inverted ratio. Returns 1 under total
/// internal reflection. Precondition: `eta` > 0 and |cos_theta_i| <= 1.
[[nodiscard]] Real fresnelDielectric(Real cos_theta_i, Real eta) noexcept;

/// Fraction of unpolarized light reflected by a smooth conductor, per channel,
/// with complex refractive index `eta` + i `k` relative to the medium outside.
/// `cos_theta_i` is clamped to [0, 1], absorbing rounding error in dot products.
/// Precondition: `eta` > 0 and `k` >= 0 per channel, and not `eta` = 1 with
/// `k` = 0 (no interface), which is undefined at grazing incidence.
[[nodiscard]] Rgb fresnelConductor(Real cos_theta_i, Rgb eta, Rgb k) noexcept;

} // namespace albedo
