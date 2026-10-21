#pragma once

#include "albedo/core/bounds3.hpp"
#include "albedo/core/interval.hpp"
#include "albedo/core/point3.hpp"
#include "albedo/core/ray.hpp"
#include "albedo/core/real.hpp"
#include "albedo/geometry/shape_hit.hpp"

#include <optional>

namespace albedo {

/// A sphere given by its center and radius.
class Sphere {
public:
    /// Throws `std::invalid_argument` unless `radius` is positive and its square is a
    /// normal floating-point number (roughly 1.5e-154 to 1.3e154 for double).
    Sphere(Point3r center, Real radius);

    /// Center of the sphere.
    [[nodiscard]] Point3r center() const noexcept { return sphere_center; }

    /// Radius of the sphere; always positive.
    [[nodiscard]] Real radius() const noexcept { return sphere_radius; }

    /// The nearest hit with `t` strictly inside `t_range`, if any. A ray starting
    /// inside the sphere hits the far side. A ray whose direction has a zero,
    /// subnormal or non-finite squared length never hits, and neither does a sphere
    /// too small to have surface points distinct from its center in floating point.
    [[nodiscard]] std::optional<ShapeHit> intersect(const Ray& ray,
                                                    Interval t_range) const noexcept;

    /// The axis-aligned box enclosing the sphere.
    [[nodiscard]] Bounds3r bounds() const noexcept;

private:
    Point3r sphere_center;
    Real sphere_radius;
};

} // namespace albedo
