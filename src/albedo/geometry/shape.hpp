#pragma once

#include "albedo/core/bounds3.hpp"
#include "albedo/core/interval.hpp"
#include "albedo/core/point2.hpp"
#include "albedo/core/ray.hpp"
#include "albedo/geometry/mesh_instance.hpp"
#include "albedo/geometry/shape_hit.hpp"
#include "albedo/geometry/sphere.hpp"
#include "albedo/geometry/triangle.hpp"

#include <concepts>
#include <optional>
#include <variant>

namespace albedo {

/// What every shape alternative must provide.
template <typename S>
concept ShapeLike =
    requires(const S& shape, const Ray& ray, Interval t_range, const ShapeHit& hit) {
        { shape.intersect(ray, t_range) } -> std::same_as<std::optional<ShapeHit>>;
        { shape.bounds() } -> std::same_as<Bounds3r>;
        { shape.textureCoordinatesAt(hit) } -> std::same_as<Point2r>;
    };

/// A variant whose alternatives must all satisfy `ShapeLike`.
template <ShapeLike... Shapes>
using ShapeVariant = std::variant<Shapes...>;

/// Any shape the renderer supports. A closed set: adding a shape means adding
/// an alternative here, and `ShapeVariant` rejects one missing required members.
using Shape = ShapeVariant<Sphere, Triangle, MeshInstance>;

/// The nearest hit of `ray` on `shape` with `t` strictly inside `t_range`, if any.
/// Its `uv` may be unset; use `textureCoordinatesAt` for the hit's texture coordinates.
/// Not `noexcept`: `std::visit` throws `std::bad_variant_access` on a valueless variant.
[[nodiscard]] inline std::optional<ShapeHit> intersect(const Shape& shape, const Ray& ray,
                                                       Interval t_range)
{
    return std::visit([&](const auto& alternative) { return alternative.intersect(ray, t_range); },
                      shape);
}

/// Texture coordinates of `hit`, a hit `intersect` found on `shape`. Some shapes
/// compute them only here, so call it for the hit finally kept rather than for
/// every candidate. Not `noexcept`: `std::visit` throws `std::bad_variant_access`
/// on a valueless variant.
[[nodiscard]] inline Point2r textureCoordinatesAt(const Shape& shape, const ShapeHit& hit)
{
    return std::visit(
        [&](const auto& alternative) { return alternative.textureCoordinatesAt(hit); }, shape);
}

/// The axis-aligned box enclosing `shape`.
/// Not `noexcept`: `std::visit` throws `std::bad_variant_access` on a valueless variant.
[[nodiscard]] inline Bounds3r bounds(const Shape& shape)
{
    return std::visit([](const auto& alternative) { return alternative.bounds(); }, shape);
}

} // namespace albedo
