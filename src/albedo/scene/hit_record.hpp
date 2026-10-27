#pragma once

#include "albedo/core/normal3.hpp"
#include "albedo/core/point2.hpp"
#include "albedo/core/point3.hpp"
#include "albedo/core/real.hpp"

#include <cstddef>

namespace albedo {

/// The nearest surface a ray hits in a scene, as shading code needs it.
struct HitRecord {
    /// Ray parameter of the hit.
    Real t{};
    /// Hit position in world space.
    Point3r point{};
    /// Unit normal facing against the incoming ray, whichever side was hit.
    Normal3r normal{};
    /// True when the ray hit the outside of the surface (its normal pointed at the ray).
    bool front_face{};
    /// Texture coordinates of the hit position, as the shape hit defines them.
    Point2r uv{};
    /// Index of the object hit, in the order objects were added to the scene. Only
    /// meaningful for records from `Scene::intersect`; the default 0 names a real
    /// object in any non-empty scene.
    std::size_t object{};
};

} // namespace albedo
