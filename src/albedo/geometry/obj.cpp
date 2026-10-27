#include "albedo/geometry/obj.hpp"

#include "albedo/core/point2.hpp"
#include "albedo/core/point3.hpp"
#include "albedo/core/real.hpp"
#include "albedo/io/line_reader.hpp"
#include "albedo/io/path_text.hpp"
#include "albedo/io/tokens.hpp"

#include <array>
#include <cstddef>
#include <format>
#include <fstream>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <utility>
#include <vector>

namespace albedo {

namespace {

// One face corner: indices into the positions and, if given, the texture coordinates.
struct Corner {
    std::size_t position{};
    std::optional<std::size_t> texture;
};

// The texture indices of a triangle, if each corner has one.
std::optional<TriangleMesh::Face> textureFace(const Corner& a, const Corner& b,
                                              const Corner& c) noexcept
{
    if (!a.texture || !b.texture || !c.texture) {
        return std::nullopt;
    }
    return TriangleMesh::Face{*a.texture, *b.texture, *c.texture};
}

// Accumulates the mesh line by line and knows where it is, for error messages.
class ObjReader {
public:
    explicit ObjReader(const ObjLimits& reader_limits) noexcept : limits{reader_limits} {}

    void readLine(std::string_view line)
    {
        ++line_number;
        Tokens tokens{line.substr(0, line.find('#'))};
        const auto keyword{tokens.next()};
        if (keyword == "v") {
            readVertex(tokens);
        } else if (keyword == "vt") {
            readTextureCoordinate(tokens);
        } else if (keyword == "f") {
            readFace(tokens);
        }
    }

    [[nodiscard]] TriangleMesh finish() &&
    {
        return TriangleMesh{std::move(positions), std::move(faces), std::move(texture_coordinates),
                            std::move(texture_faces)};
    }

private:
    [[noreturn]] void fail(std::string_view problem) const
    {
        throw ObjParseError{line_number, problem};
    }

    void readVertex(Tokens& tokens)
    {
        if (positions.size() >= limits.max_vertices) {
            fail(std::format("more than {} vertices", limits.max_vertices));
        }
        std::array<Real, 3> xyz{};
        for (Real& coordinate : xyz) {
            const auto token{tokens.next()};
            if (!token) {
                fail("a vertex needs three coordinates");
            }
            coordinate = coordinateFrom(*token);
        }
        // A weight or vertex color may follow; it is ignored but must be a number.
        skipNumbers(tokens);
        positions.push_back({xyz[0], xyz[1], xyz[2]});
    }

    void readTextureCoordinate(Tokens& tokens)
    {
        if (texture_coordinates.size() >= limits.max_texture_coordinates) {
            fail(std::format("more than {} texture coordinates", limits.max_texture_coordinates));
        }
        const auto u{tokens.next()};
        if (!u) {
            fail("a texture coordinate needs at least u");
        }
        Point2r uv{.x = coordinateFrom(*u), .y = 0.0};
        if (const auto v{tokens.next()}) {
            uv.y = coordinateFrom(*v);
        }
        // w, for 3D textures, is ignored but must be a number.
        skipNumbers(tokens);
        texture_coordinates.push_back(uv);
    }

    // Checks that the remaining tokens are numbers, which are then ignored.
    void skipNumbers(Tokens& tokens) const
    {
        while (const auto extra{tokens.next()}) {
            static_cast<void>(coordinateFrom(*extra));
        }
    }

    [[nodiscard]] Real coordinateFrom(std::string_view token) const
    {
        const auto value{parseReal(token)};
        if (!value) {
            fail("a coordinate is not a finite number");
        }
        return *value;
    }

    void readFace(Tokens& tokens)
    {
        corners.clear();
        while (const auto token{tokens.next()}) {
            corners.push_back(cornerFrom(*token));
        }
        if (corners.size() < 3) {
            fail("a face needs at least three corners");
        }
        const std::size_t triangle_count{corners.size() - 2};
        if (triangle_count > limits.max_triangles - faces.size()) {
            fail(std::format("more than {} triangles", limits.max_triangles));
        }
        // Every corner is in some triangle of the fan, so one lacking a texture
        // index leaves some triangle without a texture face.
        for (std::size_t corner{1}; corner + 1 < corners.size(); ++corner) {
            const Corner& a{corners[0]};
            const Corner& b{corners[corner]};
            const Corner& c{corners[corner + 1]};
            faces.push_back({a.position, b.position, c.position});
            addTextureFace(textureFace(a, b, c));
        }
    }

    // Keeps `texture_face` while every face so far has one. The first face
    // without one means the mesh cannot have texture faces; what was collected
    // is freed.
    void addTextureFace(const std::optional<TriangleMesh::Face>& texture_face)
    {
        if (!texture_face) {
            every_face_textured = false;
            texture_faces = {};
        } else if (every_face_textured) {
            texture_faces.push_back(*texture_face);
        }
    }

    // The indices of a face corner `i`, `i/t`, `i//n` or `i/t/n`.
    [[nodiscard]] Corner cornerFrom(std::string_view token) const
    {
        const std::size_t first_slash{token.find('/')};
        const auto position{parseInteger(token.substr(0, first_slash))};
        if (!position) {
            fail("a face corner is not a valid index");
        }
        Corner corner{.position = resolved(*position, "vertex", positions.size()),
                      .texture = std::nullopt};
        if (first_slash != std::string_view::npos) {
            const std::string_view rest{token.substr(first_slash + 1)};
            const std::size_t second_slash{rest.find('/')};
            if (const auto texture{optionalIndex(rest.substr(0, second_slash))}) {
                corner.texture =
                    resolved(*texture, "texture coordinate", texture_coordinates.size());
            }
            if (second_slash != std::string_view::npos) {
                static_cast<void>(optionalIndex(rest.substr(second_slash + 1)));
            }
        }
        return corner;
    }

    // A texture or normal index, which may be absent but not malformed.
    [[nodiscard]] std::optional<long long> optionalIndex(std::string_view part) const
    {
        if (part.empty()) {
            return std::nullopt;
        }
        const auto index{parseInteger(part)};
        if (!index) {
            fail("a face corner is not a valid index");
        }
        return index;
    }

    // The 0-based index for an OBJ index into `count` items of `kind`: 1-based,
    // or counting back from the latest item when negative.
    [[nodiscard]] std::size_t resolved(long long index, std::string_view kind,
                                       std::size_t count) const
    {
        if (index > 0) {
            const auto position{static_cast<unsigned long long>(index)};
            if (position > count) {
                fail(std::format("a face refers to a {} not yet defined", kind));
            }
            return static_cast<std::size_t>(position - 1);
        }
        if (index < 0) {
            // -(index + 1) cannot overflow, unlike -index for the smallest value.
            const auto back{static_cast<unsigned long long>(-(index + 1)) + 1};
            if (back > count) {
                fail(std::format("a face refers to a {} before the first one", kind));
            }
            return count - static_cast<std::size_t>(back);
        }
        fail("a face uses index 0; OBJ indices start at 1");
    }

    ObjLimits limits;
    std::size_t line_number{0};
    std::vector<Point3r> positions;
    std::vector<Point2r> texture_coordinates;
    std::vector<TriangleMesh::Face> faces;
    // One per face while `every_face_textured`, empty once it is false.
    std::vector<TriangleMesh::Face> texture_faces;
    bool every_face_textured{true};
    // Reused across faces so long files do not allocate per line.
    std::vector<Corner> corners;
};

} // namespace

ObjParseError::ObjParseError(std::size_t line, std::string_view problem)
    : std::runtime_error{std::format("OBJ line {}: {}", line, problem)}, line_number{line}
{
}

TriangleMesh readObj(std::istream& input, const ObjLimits& limits)
{
    LineReader lines{input, limits.max_line_length, "OBJ"};
    ObjReader reader{limits};
    try {
        while (const auto line{lines.next()}) {
            reader.readLine(*line);
        }
    } catch (const LineTooLongError& error) {
        throw ObjParseError{error.line(), std::format("line longer than {} bytes", error.limit())};
    }
    return std::move(reader).finish();
}

TriangleMesh readObj(const std::filesystem::path& path, const ObjLimits& limits)
{
    std::ifstream file{path, std::ios::binary};
    if (!file) {
        throw std::runtime_error{std::format("cannot open OBJ file '{}'", pathText(path))};
    }
    return readObj(file, limits);
}

} // namespace albedo
