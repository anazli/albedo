#pragma once

#include "albedo/core/point3.hpp"
#include "albedo/core/real.hpp"
#include "albedo/core/vector3.hpp"

namespace albedo {

/// A half-line `origin + t * direction`.
///
/// `direction` does not have to be normalized; `t` is then measured in units of
/// its length. Callers that need distances must normalize it themselves.
struct Ray {
    Point3r origin{};
    Vector3r direction{};

    /// The point at parameter `t` along the ray.
    [[nodiscard]] constexpr Point3r at(Real t) const noexcept { return origin + direction * t; }
};

} // namespace albedo
