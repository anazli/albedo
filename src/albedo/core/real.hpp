#pragma once

#include <numbers>

namespace albedo {

/// Floating-point type used for all geometry and radiometry.
/// Double precision keeps self-intersection and accumulation error small
/// without the explicit error bounds a float-based renderer would need.
using Real = double;

/// The constant pi.
inline constexpr Real pi{std::numbers::pi_v<Real>};

/// The reciprocal of pi, common in normalised scattering and sampling terms.
inline constexpr Real inv_pi{std::numbers::inv_pi_v<Real>};

} // namespace albedo
