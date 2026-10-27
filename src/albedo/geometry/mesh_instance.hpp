#pragma once

#include "albedo/core/bounds3.hpp"
#include "albedo/core/interval.hpp"
#include "albedo/core/point2.hpp"
#include "albedo/core/ray.hpp"
#include "albedo/core/transform.hpp"
#include "albedo/geometry/shape_hit.hpp"

#include <memory>
#include <optional>

namespace albedo {

class MeshGeometry;

/// A mesh placed in the scene by a transform. Instances of one `MeshGeometry`
/// share its triangles and hierarchy, so a mesh repeated many times costs its
/// memory once. Copies share their placement too, which keeps the type as small
/// as a pointer pair.
class MeshInstance {
public:
    /// Places `mesh` by `object_to_world`. Throws `std::invalid_argument` if `mesh`
    /// is null or the placed mesh's bounding box has a non-finite coordinate, and
    /// `std::bad_alloc` if memory runs out.
    MeshInstance(std::shared_ptr<const MeshGeometry> mesh, const Transform& object_to_world);

    /// The geometry placed.
    [[nodiscard]] const MeshGeometry& mesh() const noexcept { return *placement->mesh; }

    /// The nearest hit with `t` strictly inside `t_range`, if any. The ray is taken
    /// into the mesh's space without normalizing its direction, so `t` and the
    /// range mean the same in both spaces. The hit's point and normal are in world
    /// space; the normal is unit length and lies on the side of the surface it did
    /// in the mesh, even under a mirroring transform. Texture coordinates are the
    /// mesh's. A normal too short or too long to normalize counts as a miss.
    /// Throws only what `MeshGeometry::intersect` throws.
    [[nodiscard]] std::optional<ShapeHit> intersect(const Ray& ray, Interval t_range) const;

    /// Texture coordinates of `hit`, which `intersect` took from the mesh.
    [[nodiscard]] static Point2r textureCoordinatesAt(const ShapeHit& hit) noexcept
    {
        return hit.uv;
    }

    /// The axis-aligned box enclosing the placed mesh.
    [[nodiscard]] Bounds3r bounds() const noexcept { return placement->world_bounds; }

private:
    struct Placement {
        std::shared_ptr<const MeshGeometry> mesh;
        Transform to_world;
        // The inverse, stored so each ray uses it in place rather than copying it
        // out of `to_world.inverse()`.
        Transform to_object;
        Bounds3r world_bounds;
    };

    std::shared_ptr<const Placement> placement;
};

} // namespace albedo
