#include "albedo/geometry/mesh_geometry.hpp"

#include "albedo/geometry/triangle_mesh.hpp"

#include <stdexcept>

namespace albedo {

namespace {

// The triangles of `mesh`, of which there must be at least one.
std::vector<Triangle> trianglesOf(const TriangleMesh& mesh)
{
    std::vector<Triangle> triangles{mesh.triangles()};
    if (triangles.empty()) {
        throw std::invalid_argument{"mesh geometry needs at least one triangle with area"};
    }
    return triangles;
}

std::vector<Bounds3r> boundsOf(const std::vector<Triangle>& triangles)
{
    std::vector<Bounds3r> result;
    result.reserve(triangles.size());
    for (const Triangle& triangle : triangles) {
        result.push_back(triangle.bounds());
    }
    return result;
}

Bounds3r enclosing(const std::vector<Triangle>& triangles) noexcept
{
    Bounds3r result{};
    for (const Triangle& triangle : triangles) {
        result = merged(result, triangle.bounds());
    }
    return result;
}

} // namespace

MeshGeometry::MeshGeometry(const TriangleMesh& mesh)
    : triangle_list{trianglesOf(mesh)}, box{enclosing(triangle_list)},
      hierarchy{boundsOf(triangle_list)}
{
}

std::optional<ShapeHit> MeshGeometry::intersect(const Ray& ray, Interval t_range) const
{
    // The hierarchy shrinks the range to each hit, so the last hit kept is the nearest.
    std::optional<ShapeHit> nearest;
    static_cast<void>(hierarchy.nearestHit(
        ray, t_range, [&](std::size_t index, Interval range) -> std::optional<Real> {
            const auto hit{triangle_list[index].intersect(ray, range)};
            if (!hit) {
                return std::nullopt;
            }
            nearest = hit;
            return hit->t;
        }));
    return nearest;
}

} // namespace albedo
