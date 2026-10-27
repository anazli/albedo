#include "albedo/geometry/mesh_instance.hpp"

#include "albedo/core/normal3.hpp"
#include "albedo/core/point3.hpp"
#include "albedo/core/vector3.hpp"
#include "albedo/geometry/mesh_geometry.hpp"

#include <cmath>
#include <stdexcept>
#include <utility>

namespace albedo {

MeshInstance::MeshInstance(std::shared_ptr<const MeshGeometry> mesh,
                           const Transform& object_to_world)
{
    if (!mesh) {
        throw std::invalid_argument{"mesh instance needs a mesh"};
    }
    const Bounds3r world_bounds{object_to_world.apply(mesh->bounds())};
    // A non-finite box would be rejected by the scene's hierarchy anyway; failing
    // here names the cause.
    if (!isFinite(world_bounds.min) || !isFinite(world_bounds.max)) {
        throw std::invalid_argument{"mesh instance transform takes the mesh out of range"};
    }
    placement = std::make_shared<const Placement>(Placement{.mesh = std::move(mesh),
                                                            .to_world = object_to_world,
                                                            .to_object = object_to_world.inverse(),
                                                            .world_bounds = world_bounds});
}

std::optional<ShapeHit> MeshInstance::intersect(const Ray& ray, Interval t_range) const
{
    const auto hit{placement->mesh->intersect(placement->to_object.apply(ray), t_range)};
    if (!hit) {
        return std::nullopt;
    }
    const Vector3r normal{asVector(placement->to_world.apply(hit->normal))};
    if (!std::isnormal(lengthSquared(normal))) {
        return std::nullopt;
    }
    return ShapeHit{.t = hit->t,
                    .point = placement->to_world.apply(hit->point),
                    .normal = asNormal(normalized(normal)),
                    .uv = hit->uv};
}

} // namespace albedo
