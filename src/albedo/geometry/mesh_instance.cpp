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
    const Normal3r world_normal{asNormal(normalized(normal))};
    return ShapeHit{.t = hit->t,
                    .point = placement->to_world.apply(hit->point),
                    .normal = world_normal,
                    .shading_normal = worldShadingNormal(*hit, world_normal),
                    .uv = hit->uv};
}

Normal3r MeshInstance::worldShadingNormal(const ShapeHit& hit, Normal3r world_normal) const noexcept
{
    // A flat hit shades with its geometric normal, already transformed.
    if (hit.shading_normal == hit.normal) {
        return world_normal;
    }
    // The shading normal transforms as the geometric one does; if it alone cannot
    // be normalized, the geometric normal shades no worse than a miss would.
    const Vector3r shading{asVector(placement->to_world.apply(hit.shading_normal))};
    return std::isnormal(lengthSquared(shading)) ? asNormal(normalized(shading)) : world_normal;
}

} // namespace albedo
