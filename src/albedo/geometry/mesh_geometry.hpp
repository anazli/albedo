#pragma once

#include "albedo/accel/bvh.hpp"
#include "albedo/core/bounds3.hpp"
#include "albedo/core/interval.hpp"
#include "albedo/core/ray.hpp"
#include "albedo/geometry/shape_hit.hpp"
#include "albedo/geometry/triangle.hpp"

#include <cstddef>
#include <optional>
#include <vector>

namespace albedo {

class TriangleMesh;

/// The triangles of a mesh and a bounding volume hierarchy over them, built once
/// in the mesh's own space so any number of `MeshInstance`s can share them.
class MeshGeometry {
public:
    /// Builds the geometry from `mesh.triangles()`. Throws `std::invalid_argument`
    /// if the mesh has no triangle with a computable area (an instance needs a
    /// non-empty box), and `std::bad_alloc` if memory runs out.
    explicit MeshGeometry(const TriangleMesh& mesh);

    /// Number of triangles kept.
    [[nodiscard]] std::size_t triangleCount() const noexcept { return triangle_list.size(); }

    /// The axis-aligned box enclosing every triangle.
    [[nodiscard]] Bounds3r bounds() const noexcept { return box; }

    /// The nearest hit on any triangle with `t` strictly inside `t_range`, if any,
    /// as `Triangle::intersect` reports it. Throws only if the hierarchy's depth
    /// bound is violated (see `Bvh`).
    [[nodiscard]] std::optional<ShapeHit> intersect(const Ray& ray, Interval t_range) const;

private:
    std::vector<Triangle> triangle_list;
    Bounds3r box;
    Bvh hierarchy;
};

} // namespace albedo
