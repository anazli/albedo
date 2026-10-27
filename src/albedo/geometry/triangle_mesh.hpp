#pragma once

#include "albedo/core/point2.hpp"
#include "albedo/core/point3.hpp"
#include "albedo/geometry/triangle.hpp"

#include <array>
#include <cstddef>
#include <span>
#include <vector>

namespace albedo {

/// Triangles that share vertex positions, each face naming its three corners by
/// index. Sharing gives neighbouring triangles bitwise identical vertices, which
/// the watertight triangle test needs to leave no gaps along shared edges.
/// Texture coordinates, when present, are indexed separately, one texture face
/// per face, as in OBJ files: a position can then take different coordinates in
/// different faces, as it does along a texture seam.
class TriangleMesh {
public:
    /// Indices into `positions()` of one triangle's corners, counter-clockwise seen
    /// from its front.
    using Face = std::array<std::size_t, 3>;

    /// Throws `std::invalid_argument` if a position has a non-finite coordinate,
    /// and `std::out_of_range` if a face index is not below `positions.size()`.
    /// Positions no face uses are allowed. The mesh has no texture coordinates.
    TriangleMesh(std::vector<Point3r> positions, std::vector<Face> faces);

    /// A mesh whose face `i` takes its corners' texture coordinates from
    /// `texture_faces[i]`, indices into `texture_coordinates`. Throws as the
    /// two-argument constructor does, `std::invalid_argument` if a texture
    /// coordinate is not finite or `texture_faces` is not empty and not as long as
    /// `faces`, and `std::out_of_range` if a texture index is not below
    /// `texture_coordinates.size()`. Empty `texture_faces` means no texture
    /// coordinates, as for the two-argument constructor.
    TriangleMesh(std::vector<Point3r> positions, std::vector<Face> faces,
                 std::vector<Point2r> texture_coordinates, std::vector<Face> texture_faces);

    /// The vertex positions in the order given.
    [[nodiscard]] std::span<const Point3r> positions() const noexcept { return vertex_positions; }

    /// The faces in the order given, including any without area.
    [[nodiscard]] std::span<const Face> faces() const noexcept { return face_list; }

    /// The texture coordinates in the order given; may be empty.
    [[nodiscard]] std::span<const Point2r> textureCoordinates() const noexcept { return vertex_uv; }

    /// Per face, the indices into `textureCoordinates()` of its corners; empty
    /// when the mesh has no texture coordinates.
    [[nodiscard]] std::span<const Face> textureFaces() const noexcept { return texture_face_list; }

    /// One triangle per face, in face order, skipping faces without a computable
    /// area (see `formsTriangle`): exporters often write such slivers, and they
    /// could never be hit anyway. Triangles take their texture face's coordinates,
    /// or `default_triangle_texture_coordinates` when the mesh has none. Throws
    /// `std::bad_alloc` if memory runs out.
    [[nodiscard]] std::vector<Triangle> triangles() const;

private:
    // The texture coordinates of face `face`'s corners. Precondition: `face` is
    // below `faces().size()`.
    [[nodiscard]] std::array<Point2r, 3> textureCoordinatesOf(std::size_t face) const noexcept;

    std::vector<Point3r> vertex_positions;
    std::vector<Face> face_list;
    std::vector<Point2r> vertex_uv;
    std::vector<Face> texture_face_list;
};

} // namespace albedo
