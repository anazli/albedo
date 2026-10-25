#pragma once

#include "albedo/core/bounds3.hpp"
#include "albedo/core/interval.hpp"
#include "albedo/core/normal3.hpp"
#include "albedo/core/point3.hpp"
#include "albedo/core/ray.hpp"
#include "albedo/geometry/shape_hit.hpp"

#include <array>
#include <optional>

namespace albedo {

/// A flat triangle given by three vertices. Its front side is the one from which
/// the vertices appear counter-clockwise; the normal points out of that side.
class Triangle {
public:
    /// Throws `std::invalid_argument` unless every coordinate is finite and the
    /// triangle has an area whose normal can be computed: the vertices must not be
    /// collinear, and the squared length of the edge cross product must be a normal
    /// floating-point number (edges roughly between 1e-77 and 1e77 for double).
    Triangle(Point3r p0, Point3r p1, Point3r p2);

    /// The vertices in the order given.
    [[nodiscard]] const std::array<Point3r, 3>& vertices() const noexcept { return corners; }

    /// Unit normal on the counter-clockwise side.
    [[nodiscard]] Normal3r normal() const noexcept { return front_normal; }

    /// The nearest hit with `t` strictly inside `t_range`, if any, from either side;
    /// the hit's normal is always `normal()`. Rays through a shared edge or vertex
    /// hit at least one of the triangles sharing it, provided they share bitwise
    /// identical vertex values, as triangles built from one indexed mesh do. A ray
    /// whose direction has a zero, subnormal or non-finite squared length never hits,
    /// and neither does a ray with a non-finite origin.
    [[nodiscard]] std::optional<ShapeHit> intersect(const Ray& ray,
                                                    Interval t_range) const noexcept;

    /// The axis-aligned box enclosing the triangle.
    [[nodiscard]] Bounds3r bounds() const noexcept;

private:
    std::array<Point3r, 3> corners;
    Normal3r front_normal;
};

} // namespace albedo
