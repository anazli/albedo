#include "albedo/geometry/mesh_geometry.hpp"

#include "albedo/core/point3.hpp"
#include "albedo/core/real.hpp"
#include "albedo/core/vector3.hpp"
#include "albedo/geometry/triangle_mesh.hpp"

#include <array>
#include <cassert>
#include <cstddef>
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

// Below this squared length, blended unit normals have cancelled so far that
// rounding (about 1e-16 per component) could turn them noticeably; for
// sensible meshes blends stay far longer than this.
constexpr Real min_blend_length_squared{1e-12};

// The corner normals blended with the weights that place `point` on `triangle`,
// or the triangle's own normal where they cancel out.
Normal3r blendedNormal(const Triangle& triangle, const std::array<Normal3r, 3>& normals,
                       Point3r point) noexcept
{
    const auto [b0, b1, b2]{triangle.barycentricCoordinatesOf(point)};
    const Vector3r blended{asVector(normals[0]) * b0 + asVector(normals[1]) * b1 +
                           asVector(normals[2]) * b2};
    // Written as a test for long enough so that a NaN blend falls back too.
    if (!(lengthSquared(blended) >= min_blend_length_squared)) {
        return triangle.normal();
    }
    return asNormal(normalized(blended));
}

} // namespace

MeshGeometry::MeshGeometry(const TriangleMesh& mesh, MeshShading shading)
    : triangle_list{trianglesOf(mesh)},
      corner_normals{shading == MeshShading::Smooth ? mesh.smoothNormals()
                                                    : std::vector<std::array<Normal3r, 3>>{}},
      box{enclosing(triangle_list)}, hierarchy{boundsOf(triangle_list)}
{
    assert(corner_normals.empty() || corner_normals.size() == triangle_list.size());
}

std::optional<ShapeHit> MeshGeometry::intersect(const Ray& ray, Interval t_range) const
{
    // The hierarchy shrinks the range to each hit, so the last hit kept is the nearest.
    std::optional<ShapeHit> nearest;
    std::size_t nearest_index{};
    static_cast<void>(hierarchy.nearestHit(
        ray, t_range, [&](std::size_t index, Interval range) -> std::optional<Real> {
            const auto hit{triangle_list[index].intersect(ray, range)};
            if (!hit) {
                return std::nullopt;
            }
            nearest = hit;
            nearest_index = index;
            return hit->t;
        }));
    // Only the hit finally kept pays for interpolating normals.
    if (nearest && !corner_normals.empty()) {
        nearest->shading_normal = blendedNormal(triangle_list[nearest_index],
                                                corner_normals[nearest_index], nearest->point);
    }
    return nearest;
}

} // namespace albedo
