#pragma once

#include "albedo/core/interval.hpp"
#include "albedo/core/ray.hpp"
#include "albedo/geometry/shape.hpp"
#include "albedo/scene/hit_record.hpp"

#include <cstddef>
#include <optional>
#include <vector>

namespace albedo {

/// The objects a ray can hit. Intersection tests every shape; an acceleration
/// structure replaces the linear search in a later step.
class Scene {
public:
    /// Adds `shape` to the scene.
    void add(const Shape& shape);

    /// Number of shapes in the scene.
    [[nodiscard]] std::size_t shapeCount() const noexcept { return shapes.size(); }

    /// The nearest hit with `t` strictly inside `t_range`, if any. The record's
    /// normal faces against the ray. Throws `std::bad_variant_access` only if a
    /// stored shape is valueless (see `intersect(const Shape&, ...)`).
    [[nodiscard]] std::optional<HitRecord> intersect(const Ray& ray, Interval t_range) const;

private:
    std::vector<Shape> shapes;
};

} // namespace albedo
