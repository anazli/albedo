#pragma once

#include "albedo/core/normal3.hpp"
#include "albedo/core/point2.hpp"
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
    /// Texture coordinates of the hit position, filled in only by shapes that find
    /// them while intersecting; others compute them on request, so only the hit
    /// finally kept pays for them. Code outside the shapes reads them through
    /// `textureCoordinatesAt(const Shape&, ...)` rather than from here: for some
    /// shapes this stays zero, which is also a valid texture coordinate.
    Point2r uv{};
};

} // namespace albedo
