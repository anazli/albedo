#pragma once

#include "albedo/core/bounds3.hpp"
#include "albedo/core/interval.hpp"
#include "albedo/core/normal3.hpp"
#include "albedo/core/point2.hpp"
#include "albedo/core/point3.hpp"
#include "albedo/core/ray.hpp"
#include "albedo/core/real.hpp"
#include "albedo/geometry/shape_hit.hpp"

#include <array>
#include <optional>

namespace albedo {

/// Texture coordinates of a triangle's vertices when none are given: the
/// triangle maps onto the lower right half of the unit square.
inline constexpr std::array<Point2r, 3> default_triangle_texture_coordinates{
    {{0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}}};

/// A flat triangle given by three vertices, each with texture coordinates. Its
/// front side is the one from which the vertices appear counter-clockwise; the
/// normal points out of that side.
class Triangle {
public:
    /// Throws `std::invalid_argument` unless every coordinate, texture coordinates
    /// included, is finite and the triangle has an area whose normal can be
    /// computed: the vertices must not be collinear, and the squared length of the
    /// edge cross product must be a normal floating-point number (edges roughly
    /// between 1e-77 and 1e77 for double).
    Triangle(Point3r p0, Point3r p1, Point3r p2,
             const std::array<Point2r, 3>& uv = default_triangle_texture_coordinates);

    /// The vertices in the order given.
    [[nodiscard]] const std::array<Point3r, 3>& vertices() const noexcept { return corners; }

    /// The texture coordinates of each vertex, in vertex order.
    [[nodiscard]] const std::array<Point2r, 3>& textureCoordinates() const noexcept
    {
        return corner_uv;
    }

    /// Unit normal on the counter-clockwise side.
    [[nodiscard]] Normal3r normal() const noexcept { return front_normal; }

    /// The nearest hit with `t` strictly inside `t_range`, if any, from either side;
    /// the hit's normal is always `normal()`. Rays through a shared edge or vertex
    /// hit at least one of the triangles sharing it, provided they share bitwise
    /// identical vertex values, as triangles built from one indexed mesh do. A ray
    /// whose direction has a zero, subnormal or non-finite squared length never hits,
    /// and neither does a ray with a non-finite origin. The hit's texture
    /// coordinates interpolate those of the vertices; its shading normal is
    /// `normal()`.
    [[nodiscard]] std::optional<ShapeHit> intersect(const Ray& ray,
                                                    Interval t_range) const noexcept;

    /// Texture coordinates of `hit`, which `intersect` already interpolated.
    [[nodiscard]] static Point2r textureCoordinatesAt(const ShapeHit& hit) noexcept
    {
        return hit.uv;
    }

    /// The axis-aligned box enclosing the triangle.
    [[nodiscard]] Bounds3r bounds() const noexcept;

    /// The weights of the vertices, in vertex order and summing to 1 up to
    /// rounding, that place `point` on the triangle; for a point off its plane,
    /// those of the point's projection onto it. Points outside the triangle get
    /// a negative weight.
    [[nodiscard]] std::array<Real, 3> barycentricCoordinatesOf(Point3r point) const noexcept;

private:
    // Declared before `corner_uv` so bad vertices are reported before bad uv.
    std::array<Point3r, 3> corners;
    Normal3r front_normal;
    std::array<Point2r, 3> corner_uv;
};

/// True when `Triangle{p0, p1, p2}` would not throw: every coordinate is finite and
/// the triangle has a computable area. Lets callers skip degenerate input cheaply.
[[nodiscard]] bool formsTriangle(Point3r p0, Point3r p1, Point3r p2) noexcept;

} // namespace albedo
