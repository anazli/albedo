#pragma once

#include "albedo/geometry/triangle_mesh.hpp"

#include <cstddef>
#include <filesystem>
#include <iosfwd>
#include <stdexcept>
#include <string_view>

namespace albedo {

/// Bounds on what `readObj` accepts, so a hostile or corrupt file cannot make it
/// exhaust memory. The defaults allow meshes far larger than this renderer can
/// trace in reasonable time.
struct ObjLimits {
    /// Longest line accepted, in bytes, not counting the line terminator. `readObj`
    /// throws `std::invalid_argument` if a buffer one line long cannot even be
    /// sized (near the largest `std::streamsize`); merely large values can still
    /// fail to allocate.
    std::size_t max_line_length{65536};
    /// Most `v` statements accepted.
    std::size_t max_vertices{std::size_t{1} << 24U};
    /// Most `vt` statements accepted.
    std::size_t max_texture_coordinates{std::size_t{1} << 24U};
    /// Most triangles accepted, counting each polygon as its fan of triangles.
    std::size_t max_triangles{std::size_t{1} << 24U};
};

/// What `readObj` throws for input it cannot accept, naming the line at fault.
class ObjParseError : public std::runtime_error {
public:
    /// `problem` describes what is wrong; the message adds the line number.
    ObjParseError(std::size_t line, std::string_view problem);

    /// The 1-based number of the offending line.
    [[nodiscard]] std::size_t line() const noexcept { return line_number; }

private:
    std::size_t line_number;
};

/// Reads the geometry of a Wavefront OBJ file: `v x y z` positions (anything
/// after the third coordinate, such as a weight or a vertex color, is ignored),
/// `vt u [v [w]]` texture coordinates (v defaults to 0; w and anything after it
/// are ignored) and `f` faces whose corners are written `i`, `i/t`, `i//n` or
/// `i/t/n`. The position index `i` and texture index `t` are used; normal
/// indices must be well formed but are ignored, and texture and normal indices
/// may be left empty (`i/`, `i//`) as some exporters write them. Indices count
/// from 1, or back from the latest `v` or `vt` when negative, and must refer to
/// one already read. Polygons become a fan of triangles around their first
/// corner. The mesh has texture faces only when every corner of every face has
/// a texture index; otherwise it keeps the texture coordinates read but has no
/// texture faces (see `TriangleMesh`). Ignored numbers must still be finite.
/// Comments (from `#` to the end of a line), blank lines and every other
/// statement are ignored; line continuations with a trailing backslash are not
/// supported.
///
/// Throws `ObjParseError` for malformed or non-finite numbers, faces with fewer
/// than three corners, invalid indices, and input exceeding `limits`;
/// `std::runtime_error` if the stream is already failed or reading it fails;
/// `std::invalid_argument` for an unusable `limits.max_line_length`;
/// `std::bad_alloc` if memory runs out. A stream with failbit in its
/// `exceptions()` mask throws `std::ios_base::failure` wherever reading a line
/// sets failbit, including at the end of input and for an overlong line.
[[nodiscard]] TriangleMesh readObj(std::istream& input, const ObjLimits& limits = {});

/// Reads the OBJ file at `path` as `readObj(std::istream&, ...)` does. Throws as
/// that does, and `std::runtime_error` naming the path if the file cannot be opened.
[[nodiscard]] TriangleMesh readObj(const std::filesystem::path& path, const ObjLimits& limits = {});

} // namespace albedo
