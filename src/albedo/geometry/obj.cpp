#include "albedo/geometry/obj.hpp"

#include "albedo/core/point3.hpp"
#include "albedo/core/real.hpp"

#include <algorithm>
#include <array>
#include <charconv>
#include <cmath>
#include <cstddef>
#include <format>
#include <fstream>
#include <istream>
#include <limits>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace albedo {

namespace {

// The words of a line, split at spaces and tabs.
class Tokens {
public:
    explicit Tokens(std::string_view line) noexcept : rest{line} {}

    // The next word, if any.
    [[nodiscard]] std::optional<std::string_view> next() noexcept
    {
        constexpr std::string_view whitespace{" \t\r\v\f"};
        const std::size_t start{rest.find_first_not_of(whitespace)};
        if (start == std::string_view::npos) {
            rest = {};
            return std::nullopt;
        }
        rest.remove_prefix(start);
        const std::size_t end{std::min(rest.find_first_of(whitespace), rest.size())};
        const std::string_view token{rest.substr(0, end)};
        rest.remove_prefix(end);
        return token;
    }

private:
    std::string_view rest;
};

// A whole token as a finite number, or nothing. A leading '+' is allowed, which
// std::from_chars itself rejects.
std::optional<Real> parseCoordinate(std::string_view token) noexcept
{
    if (token.starts_with('+')) {
        token.remove_prefix(1);
        if (token.starts_with('+') || token.starts_with('-')) {
            return std::nullopt;
        }
    }
    Real value{};
    const auto [end, error]{std::from_chars(token.data(), token.data() + token.size(), value)};
    if (error != std::errc{} || end != token.data() + token.size() || !std::isfinite(value)) {
        return std::nullopt;
    }
    return value;
}

// A whole token as a signed decimal integer, or nothing (also when out of range).
std::optional<long long> parseInteger(std::string_view token) noexcept
{
    long long value{};
    const auto [end, error]{std::from_chars(token.data(), token.data() + token.size(), value)};
    if (error != std::errc{} || end != token.data() + token.size() || token.empty()) {
        return std::nullopt;
    }
    return value;
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
        } else if (keyword == "f") {
            readFace(tokens);
        }
    }

    // Reports a line too long to read.
    [[noreturn]] void failOverlongLine() const
    {
        throw ObjParseError{line_number + 1,
                            std::format("line longer than {} bytes", limits.max_line_length)};
    }

    [[nodiscard]] TriangleMesh finish() &&
    {
        return TriangleMesh{std::move(positions), std::move(faces)};
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
        while (const auto extra{tokens.next()}) {
            static_cast<void>(coordinateFrom(*extra));
        }
        positions.push_back({xyz[0], xyz[1], xyz[2]});
    }

    [[nodiscard]] Real coordinateFrom(std::string_view token) const
    {
        const auto value{parseCoordinate(token)};
        if (!value) {
            fail("a vertex coordinate is not a finite number");
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
        for (std::size_t corner{1}; corner + 1 < corners.size(); ++corner) {
            faces.push_back({corners[0], corners[corner], corners[corner + 1]});
        }
    }

    // The position index of a face corner `i`, `i/t`, `i//n` or `i/t/n`.
    [[nodiscard]] std::size_t cornerFrom(std::string_view token) const
    {
        const std::size_t first_slash{token.find('/')};
        const std::string_view position{token.substr(0, first_slash)};
        if (first_slash != std::string_view::npos) {
            const std::string_view rest{token.substr(first_slash + 1)};
            const std::size_t second_slash{rest.find('/')};
            checkOptionalIndex(rest.substr(0, second_slash));
            if (second_slash != std::string_view::npos) {
                checkOptionalIndex(rest.substr(second_slash + 1));
            }
        }
        const auto index{parseInteger(position)};
        if (!index) {
            fail("a face corner is not a valid index");
        }
        return resolved(*index);
    }

    // A texture or normal index, which may be absent but not malformed.
    void checkOptionalIndex(std::string_view part) const
    {
        if (!part.empty() && !parseInteger(part)) {
            fail("a face corner is not a valid index");
        }
    }

    // The 0-based position index for an OBJ index: 1-based, or counting back from
    // the latest vertex when negative.
    [[nodiscard]] std::size_t resolved(long long index) const
    {
        const std::size_t count{positions.size()};
        if (index > 0) {
            const auto position{static_cast<unsigned long long>(index)};
            if (position > count) {
                fail("a face refers to a vertex not yet defined");
            }
            return static_cast<std::size_t>(position - 1);
        }
        if (index < 0) {
            // -(index + 1) cannot overflow, unlike -index for the smallest value.
            const auto back{static_cast<unsigned long long>(-(index + 1)) + 1};
            if (back > count) {
                fail("a face refers to a vertex before the first one");
            }
            return count - static_cast<std::size_t>(back);
        }
        fail("a face uses index 0; OBJ indices start at 1");
    }

    ObjLimits limits;
    std::size_t line_number{0};
    std::vector<Point3r> positions;
    std::vector<TriangleMesh::Face> faces;
    // Reused across faces so long files do not allocate per line.
    std::vector<std::size_t> corners;
};

// Rejects a stream that cannot be read and a line limit too large to buffer.
void checkReadable(const std::istream& input, const ObjLimits& limits)
{
    if (input.fail()) {
        throw std::runtime_error{"OBJ input stream is not readable"};
    }
    // The buffer adds two bytes, and its size must fit std::streamsize.
    constexpr auto largest_buffer{
        static_cast<std::size_t>(std::numeric_limits<std::streamsize>::max())};
    if (limits.max_line_length > largest_buffer - 2) {
        throw std::invalid_argument{"OBJ line length limit is too large"};
    }
}

} // namespace

ObjParseError::ObjParseError(std::size_t line, std::string_view problem)
    : std::runtime_error{std::format("OBJ line {}: {}", line, problem)}, line_number{line}
{
}

TriangleMesh readObj(std::istream& input, const ObjLimits& limits)
{
    checkReadable(input, limits);
    ObjReader reader{limits};
    // One byte more than the limit for a '\r' before the '\n', and one for the
    // terminating null that getline writes.
    std::vector<char> buffer(limits.max_line_length + 2);
    const auto capacity{static_cast<std::streamsize>(buffer.size())};
    while (true) {
        input.getline(buffer.data(), capacity);
        if (input.bad()) {
            throw std::runtime_error{"failed to read OBJ input"};
        }
        const std::streamsize extracted{input.gcount()};
        if (input.fail()) {
            if (input.eof() && extracted == 0) {
                break;
            }
            // getline stops with failbit set, but not eofbit, when the buffer fills.
            reader.failOverlongLine();
        }
        // The count includes the '\n' unless the input ended without one.
        auto length{static_cast<std::size_t>(input.eof() ? extracted : extracted - 1)};
        if (length > 0 && buffer[length - 1] == '\r') {
            --length;
        }
        if (length > limits.max_line_length) {
            reader.failOverlongLine();
        }
        reader.readLine({buffer.data(), length});
        if (input.eof()) {
            break;
        }
    }
    return std::move(reader).finish();
}

TriangleMesh readObj(const std::filesystem::path& path, const ObjLimits& limits)
{
    std::ifstream file{path, std::ios::binary};
    if (!file) {
        throw std::runtime_error{std::format("cannot open OBJ file '{}'", path.string())};
    }
    return readObj(file, limits);
}

} // namespace albedo
