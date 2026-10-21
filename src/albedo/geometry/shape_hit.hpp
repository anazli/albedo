#pragma once

#include "albedo/core/normal3.hpp"
#include "albedo/core/point3.hpp"
#include "albedo/core/real.hpp"

namespace albedo {

/// Where a ray hits a shape, as reported by the shape itself.
struct ShapeHit {
    /// Ray parameter of the hit.
    Real t{};
    /// Hit position in world space.
    Point3r point{};
    /// Unit normal pointing out of the shape, whichever side the ray came from.
    Normal3r normal{};
};

} // namespace albedo
