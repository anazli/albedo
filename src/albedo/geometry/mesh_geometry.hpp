#pragma once

#include "albedo/accel/bvh.hpp"
#include "albedo/core/bounds3.hpp"
#include "albedo/core/interval.hpp"
#include "albedo/core/normal3.hpp"
#include "albedo/core/ray.hpp"
#include "albedo/geometry/shape_hit.hpp"
#include "albedo/geometry/triangle.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <vector>

namespace albedo {

class TriangleMesh;

/// How a mesh's hits are shaded.
enum class MeshShading : std::uint8_t {
    /// Each triangle with its own normal, so the facets show.
    Flat,
    /// With normals interpolated across each triangle from `TriangleMesh::smoothNormals`.
    Smooth,
};

/// The triangles of a mesh and a bounding volume hierarchy over them, built once
/// in the mesh's own space so any number of `MeshInstance`s can share them.
class MeshGeometry {
public:
    /// Builds the geometry from `mesh.triangles()`, shaded as `shading` says.
    /// Throws `std::invalid_argument` if the mesh has no triangle with a computable
    /// area (an instance needs a non-empty box), and `std::bad_alloc` if memory
    /// runs out.
    explicit MeshGeometry(const TriangleMesh& mesh, MeshShading shading = MeshShading::Flat);

    /// Number of triangles kept.
    [[nodiscard]] std::size_t triangleCount() const noexcept { return triangle_list.size(); }

    /// The axis-aligned box enclosing every triangle.
    [[nodiscard]] Bounds3r bounds() const noexcept { return box; }

    /// How hits are shaded.
    [[nodiscard]] MeshShading shading() const noexcept
    {
        return corner_normals.empty() ? MeshShading::Flat : MeshShading::Smooth;
    }

    /// The nearest hit on any triangle with `t` strictly inside `t_range`, if any,
    /// as `Triangle::intersect` reports it. When smooth, the hit's shading normal
    /// interpolates its triangle's corner normals instead, falling back to the
    /// triangle's own normal where they nearly cancel out. Throws only if the
    /// hierarchy's depth bound is violated (see `Bvh`).
    [[nodiscard]] std::optional<ShapeHit> intersect(const Ray& ray, Interval t_range) const;

private:
    std::vector<Triangle> triangle_list;
    // One entry per triangle when smooth, none when flat. Kept apart from the
    // triangles so flat meshes, and the hierarchy's walk over triangles, do not
    // pay for them.
    std::vector<std::array<Normal3r, 3>> corner_normals;
    Bounds3r box;
    Bvh hierarchy;
};

} // namespace albedo
