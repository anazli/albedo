#pragma once

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
class TriangleMesh {
public:
    /// Indices into `positions()` of one triangle's corners, counter-clockwise seen
    /// from its front.
    using Face = std::array<std::size_t, 3>;

    /// Throws `std::invalid_argument` if a position has a non-finite coordinate,
    /// and `std::out_of_range` if a face index is not below `positions.size()`.
    /// Positions no face uses are allowed.
    TriangleMesh(std::vector<Point3r> positions, std::vector<Face> faces);

    /// The vertex positions in the order given.
    [[nodiscard]] std::span<const Point3r> positions() const noexcept { return vertex_positions; }

    /// The faces in the order given, including any without area.
    [[nodiscard]] std::span<const Face> faces() const noexcept { return face_list; }

    /// One triangle per face, in face order, skipping faces without a computable
    /// area (see `formsTriangle`): exporters often write such slivers, and they
    /// could never be hit anyway. Throws `std::bad_alloc` if memory runs out.
    [[nodiscard]] std::vector<Triangle> triangles() const;

private:
    std::vector<Point3r> vertex_positions;
    std::vector<Face> face_list;
};

} // namespace albedo
