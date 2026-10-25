#include "albedo/geometry/triangle_mesh.hpp"

#include <format>
#include <span>
#include <stdexcept>
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

void checkFaces(std::span<const TriangleMesh::Face> faces, std::size_t position_count)
{
    for (std::size_t face{0}; face < faces.size(); ++face) {
        for (const std::size_t index : faces[face]) {
            if (index >= position_count) {
                throw std::out_of_range{
                    std::format("mesh face {} uses position {}, but the mesh has {} positions",
                                face, index, position_count)};
            }
        }
    }
}

} // namespace

TriangleMesh::TriangleMesh(std::vector<Point3r> positions, std::vector<Face> faces)
    : vertex_positions{std::move(positions)}, face_list{std::move(faces)}
{
    checkPositions(vertex_positions);
    checkFaces(face_list, vertex_positions.size());
}

std::vector<Triangle> TriangleMesh::triangles() const
{
    std::vector<Triangle> result;
    result.reserve(face_list.size());
    for (const auto& [i0, i1, i2] : face_list) {
        const Point3r p0{vertex_positions[i0]};
        const Point3r p1{vertex_positions[i1]};
        const Point3r p2{vertex_positions[i2]};
        if (formsTriangle(p0, p1, p2)) {
            result.emplace_back(p0, p1, p2);
        }
    }
    return result;
}

} // namespace albedo
