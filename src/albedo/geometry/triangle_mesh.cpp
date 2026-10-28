#include "albedo/geometry/triangle_mesh.hpp"

#include "albedo/core/vector3.hpp"

#include <array>
#include <cassert>
#include <cmath>
#include <format>
#include <span>
#include <stdexcept>
#include <string_view>
#include <utility>

namespace albedo {

namespace {

void checkPositions(std::span<const Point3r> positions)
{
    for (std::size_t index{0}; index < positions.size(); ++index) {
        if (!isFinite(positions[index])) {
            throw std::invalid_argument{
                std::format("mesh position {} has a non-finite coordinate", index)};
        }
    }
}

// `kind` names what the faces index, in the singular, for the message.
void checkFaces(std::span<const TriangleMesh::Face> faces, std::size_t count, std::string_view kind)
{
    for (std::size_t face{0}; face < faces.size(); ++face) {
        for (const std::size_t index : faces[face]) {
            if (index >= count) {
                throw std::out_of_range{
                    std::format("mesh face {} uses {} {}, but the mesh has {} {}s", face, kind,
                                index, count, kind)};
            }
        }
    }
}

void checkTextureCoordinates(std::span<const Point2r> texture_coordinates)
{
    for (std::size_t index{0}; index < texture_coordinates.size(); ++index) {
        const Point2r uv{texture_coordinates[index]};
        if (!std::isfinite(uv.x) || !std::isfinite(uv.y)) {
            throw std::invalid_argument{
                std::format("mesh texture coordinate {} is not finite", index)};
        }
    }
}

} // namespace

TriangleMesh::TriangleMesh(std::vector<Point3r> positions, std::vector<Face> faces)
    : TriangleMesh{std::move(positions), std::move(faces), {}, {}}
{
}

TriangleMesh::TriangleMesh(std::vector<Point3r> positions, std::vector<Face> faces,
                           std::vector<Point2r> texture_coordinates,
                           std::vector<Face> texture_faces)
    : vertex_positions{std::move(positions)}, face_list{std::move(faces)},
      vertex_uv{std::move(texture_coordinates)}, texture_face_list{std::move(texture_faces)}
{
    checkPositions(vertex_positions);
    checkFaces(face_list, vertex_positions.size(), "position");
    checkTextureCoordinates(vertex_uv);
    if (!texture_face_list.empty() && texture_face_list.size() != face_list.size()) {
        throw std::invalid_argument{std::format("mesh has {} texture faces for {} faces",
                                                texture_face_list.size(), face_list.size())};
    }
    checkFaces(texture_face_list, vertex_uv.size(), "texture coordinate");
}

std::vector<Triangle> TriangleMesh::triangles() const
{
    std::vector<Triangle> result;
    result.reserve(face_list.size());
    for (std::size_t face{0}; face < face_list.size(); ++face) {
        const auto& [i0, i1, i2]{face_list[face]};
        const Point3r p0{vertex_positions[i0]};
        const Point3r p1{vertex_positions[i1]};
        const Point3r p2{vertex_positions[i2]};
        if (formsTriangle(p0, p1, p2)) {
            result.emplace_back(p0, p1, p2, textureCoordinatesOf(face));
        }
    }
    return result;
}

std::vector<std::array<Normal3r, 3>> TriangleMesh::smoothNormals() const
{
    // The cross product of two edges has twice the triangle's area as its length,
    // so summing them unnormalized weighs each triangle by its area.
    std::vector<Vector3r> sums(vertex_positions.size());
    for (const auto& [i0, i1, i2] : face_list) {
        const Point3r p0{vertex_positions[i0]};
        const Point3r p1{vertex_positions[i1]};
        const Point3r p2{vertex_positions[i2]};
        if (formsTriangle(p0, p1, p2)) {
            const Vector3r area_normal{cross(p1 - p0, p2 - p0)};
            sums[i0] += area_normal;
            sums[i1] += area_normal;
            sums[i2] += area_normal;
        }
    }
    std::vector<std::array<Normal3r, 3>> result;
    result.reserve(face_list.size());
    for (const auto& [i0, i1, i2] : face_list) {
        const Point3r p0{vertex_positions[i0]};
        const Point3r p1{vertex_positions[i1]};
        const Point3r p2{vertex_positions[i2]};
        if (!formsTriangle(p0, p1, p2)) {
            continue;
        }
        const Normal3r face_normal{asNormal(normalized(cross(p1 - p0, p2 - p0)))};
        const auto corner_normal{[&](std::size_t index) {
            const Vector3r sum{sums[index]};
            return std::isnormal(lengthSquared(sum)) ? asNormal(normalized(sum)) : face_normal;
        }};
        result.push_back({corner_normal(i0), corner_normal(i1), corner_normal(i2)});
    }
    return result;
}

std::array<Point2r, 3> TriangleMesh::textureCoordinatesOf(std::size_t face) const noexcept
{
    assert(face < face_list.size() && "face index out of range");
    assert((texture_face_list.empty() || texture_face_list.size() == face_list.size()) &&
           "texture faces must match the faces");
    if (texture_face_list.empty()) {
        return default_triangle_texture_coordinates;
    }
    const auto& [t0, t1, t2]{texture_face_list[face]};
    return {vertex_uv[t0], vertex_uv[t1], vertex_uv[t2]};
}

} // namespace albedo
