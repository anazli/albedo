#include "albedo/scene/scene_file.hpp"

#include "albedo/color/rgb.hpp"
#include "albedo/core/point3.hpp"
#include "albedo/core/real.hpp"
#include "albedo/core/transform.hpp"
#include "albedo/core/vector3.hpp"
#include "albedo/geometry/mesh_geometry.hpp"
#include "albedo/geometry/mesh_instance.hpp"
#include "albedo/geometry/obj.hpp"
#include "albedo/geometry/sphere.hpp"
#include "albedo/geometry/triangle.hpp"
#include "albedo/geometry/triangle_mesh.hpp"
#include "albedo/io/line_reader.hpp"
#include "albedo/io/path_text.hpp"
#include "albedo/io/tokens.hpp"
#include "albedo/light/point_light.hpp"
#include "albedo/light/sky_light.hpp"
#include "albedo/material/basic_material.hpp"
#include "albedo/material/material.hpp"
#include "albedo/material/mix_material.hpp"
#include "albedo/texture/checker_texture.hpp"
#include "albedo/texture/constant_texture.hpp"
#include "albedo/texture/perlin_pattern.hpp"
#include "albedo/texture/perlin_texture.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <concepts>
#include <cstdint>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <optional>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <type_traits>
#include <utility>
#include <vector>

namespace albedo {

namespace {

// True for a letter or `_`, then letters, digits, `_` or `-`, in ASCII whatever
// the locale.
bool isIdentifier(std::string_view name) noexcept
{
    const auto letter{[](char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'); }};
    const auto digit{[](char c) { return c >= '0' && c <= '9'; }};
    if (name.empty() || (!letter(name.front()) && name.front() != '_')) {
        return false;
    }
    return std::ranges::all_of(
        name, [&](char c) { return letter(c) || digit(c) || c == '_' || c == '-'; });
}

// Bounds on values, so that nothing a scene file says can break the
// preconditions of the types it builds or overflow while rendering. Roughness
// and reflectances are fractions; the indices span every real material with
// room to spare.
constexpr Real max_reflectance{1};
constexpr Real max_emission{1e6};
constexpr Real max_roughness{1};
constexpr Real min_ior{1e-2};
constexpr Real max_ior{1e2};
constexpr Real min_conductor_eta{1e-3};
constexpr Real max_conductor_eta{1e3};
constexpr Real max_conductor_k{1e3};
// Beyond this the marble bands warp into noise finer than any pixel.
constexpr Real max_perlin_veins{1e2};

// Longest piece of input an error message repeats.
constexpr std::size_t max_quoted_length{32};

// `token` fit for an error message: in quotes, cut to `max_quoted_length`
// bytes, and with every byte outside printable ASCII written as \xNN, so a
// hostile file cannot fill a terminal or send it control sequences.
std::string quoted(std::string_view token)
{
    std::string result{"'"};
    for (const char c : token.substr(0, max_quoted_length)) {
        const auto byte{static_cast<unsigned char>(c)};
        if (byte >= 0x20 && byte <= 0x7E) {
            result += c;
        } else {
            result += std::format("\\x{:02X}", byte);
        }
    }
    result += token.size() > max_quoted_length ? "'..." : "'";
    return result;
}

// One statement, read word by word; every error names its line.
class Statement {
public:
    Statement(std::string_view text, std::size_t line) noexcept : tokens{text}, line_number{line} {}

    [[noreturn]] void fail(std::string_view problem) const
    {
        throw SceneParseError{line_number, problem};
    }

    [[nodiscard]] std::optional<std::string_view> next() noexcept { return tokens.next(); }

    // The next word, left to be read again.
    [[nodiscard]] std::optional<std::string_view> peek() const noexcept
    {
        Tokens rest{tokens};
        return rest.next();
    }

    // The next word; `what` describes it if it is missing.
    [[nodiscard]] std::string_view word(std::string_view what)
    {
        const auto token{next()};
        if (!token) {
            fail(std::format("expected {}", what));
        }
        return *token;
    }

    [[nodiscard]] Real number(std::string_view what)
    {
        const std::string_view token{word(what)};
        const auto value{parseReal(token)};
        if (!value) {
            fail(std::format("{} must be a finite number, got {}", what, quoted(token)));
        }
        return *value;
    }

    // A number in [low, high].
    [[nodiscard]] Real inRange(std::string_view what, Real low, Real high)
    {
        const Real value{number(what)};
        // parseReal gives finite numbers only, so no NaN slips between these.
        if (value < low || value > high) {
            fail(std::format("{} must be between {} and {}, got {}", what, low, high, value));
        }
        return value;
    }

    [[nodiscard]] Real positive(std::string_view what)
    {
        const Real value{number(what)};
        if (!(value > 0)) {
            fail(std::format("{} must be positive, got {}", what, value));
        }
        return value;
    }

    [[nodiscard]] Point3r point(std::string_view what)
    {
        const Real x{number(std::format("the x coordinate of {}", what))};
        const Real y{number(std::format("the y coordinate of {}", what))};
        return {x, y, number(std::format("the z coordinate of {}", what))};
    }

    [[nodiscard]] Vector3r vector(std::string_view what)
    {
        const Point3r components{point(what)};
        return {components.x, components.y, components.z};
    }

    // Three channels, each in [low, high].
    [[nodiscard]] Rgb color(std::string_view what, Real low, Real high)
    {
        const Real r{inRange(std::format("the red channel of {}", what), low, high)};
        const Real g{inRange(std::format("the green channel of {}", what), low, high)};
        return {r, g, inRange(std::format("the blue channel of {}", what), low, high)};
    }

    // A reflectance: three channels in [0, 1].
    [[nodiscard]] Rgb reflectance(std::string_view what) { return color(what, 0, max_reflectance); }

    // Emitted light: three channels in [0, `max_emission`].
    [[nodiscard]] Rgb emission(std::string_view what) { return color(what, 0, max_emission); }

    // An identifier naming something.
    [[nodiscard]] std::string_view name(std::string_view what)
    {
        const std::string_view token{word(what)};
        if (!isIdentifier(token)) {
            fail(std::format("{} is not a valid name", quoted(token)));
        }
        return token;
    }

    // Reads `name values...` fields to the end of the statement, calling
    // `read(name)` for each; `read` returns false for a name it does not know.
    template <std::predicate<std::string_view> Read>
    void readFields(Read read)
    {
        // Holds known names only: an unknown one fails at once.
        std::vector<std::string_view> seen;
        while (const auto field{next()}) {
            if (std::ranges::find(seen, *field) != seen.end()) {
                fail(std::format("{} is given twice", quoted(*field)));
            }
            seen.push_back(*field);
            if (!read(*field)) {
                fail(std::format("unknown field {}", quoted(*field)));
            }
        }
    }

    // `value`, or a failure saying the field `what` is missing.
    template <typename T>
    [[nodiscard]] T required(const std::optional<T>& value, std::string_view what) const
    {
        if (!value) {
            fail(std::format("missing '{}'", what));
        }
        return *value;
    }

    // Fails unless nothing is left on the line.
    void expectEnd()
    {
        if (const auto extra{next()}) {
            fail(std::format("unexpected {}", quoted(*extra)));
        }
    }

private:
    Tokens tokens;
    std::size_t line_number;
};

// A mesh read from a file: its shared geometry, and the positions its
// triangles use, each once, for `ground` to find the lowest one.
struct LoadedMesh {
    std::shared_ptr<const MeshGeometry> geometry;
    std::vector<Point3r> points;
};

// The shape of a UTF-8 sequence, from its lead byte: how many bytes it has,
// and the range its second byte must lie in to rule out overlong forms,
// surrogates and code points past U+10FFFF (RFC 3629, table 3-7 of Unicode).
struct Utf8Sequence {
    std::size_t length{1};
    unsigned char second_low{0x80};
    unsigned char second_high{0xBF};
};

std::optional<Utf8Sequence> utf8Sequence(unsigned char lead) noexcept
{
    if (lead < 0x80) {
        return Utf8Sequence{};
    }
    if (lead >= 0xC2 && lead <= 0xDF) {
        return Utf8Sequence{.length = 2};
    }
    if (lead >= 0xE0 && lead <= 0xEF) {
        return Utf8Sequence{.length = 3,
                            .second_low = static_cast<unsigned char>(lead == 0xE0 ? 0xA0 : 0x80),
                            .second_high = static_cast<unsigned char>(lead == 0xED ? 0x9F : 0xBF)};
    }
    if (lead >= 0xF0 && lead <= 0xF4) {
        return Utf8Sequence{.length = 4,
                            .second_low = static_cast<unsigned char>(lead == 0xF0 ? 0x90 : 0x80),
                            .second_high = static_cast<unsigned char>(lead == 0xF4 ? 0x8F : 0xBF)};
    }
    return std::nullopt;
}

// True when `text` is well-formed UTF-8.
bool isValidUtf8(std::string_view text) noexcept
{
    std::size_t index{0};
    while (index < text.size()) {
        const auto sequence{utf8Sequence(static_cast<unsigned char>(text[index]))};
        if (!sequence || text.size() - index < sequence->length) {
            return false;
        }
        for (std::size_t offset{1}; offset < sequence->length; ++offset) {
            const auto byte{static_cast<unsigned char>(text[index + offset])};
            const unsigned char low{offset == 1 ? sequence->second_low : std::uint8_t{0x80}};
            const unsigned char high{offset == 1 ? sequence->second_high : std::uint8_t{0xBF}};
            if (byte < low || byte > high) {
                return false;
            }
        }
        index += sequence->length;
    }
    return true;
}

// True when `name` (any part of a path) is a device on Windows, such as `nul`
// or `com1.obj`: those names are reserved in every directory there, so scenes
// avoid them everywhere and behave the same on every platform.
bool isDeviceName(const std::filesystem::path& name)
{
    // Windows looks only at the text before the first '.' or ':', ignoring
    // trailing spaces, so `nul.tar.obj` and `nul:x` are devices too.
    std::string base{pathText(name)};
    base.erase(std::min(base.find_first_of(".:"), base.size()));
    while (!base.empty() && base.back() == ' ') {
        base.pop_back();
    }
    std::ranges::transform(base, base.begin(), [](char c) {
        return c >= 'a' && c <= 'z' ? static_cast<char>(c - 'a' + 'A') : c;
    });
    constexpr std::array<std::string_view, 4> plain{"CON", "PRN", "AUX", "NUL"};
    if (std::ranges::find(plain, base) != plain.end()) {
        return true;
    }
    if (!base.starts_with("COM") && !base.starts_with("LPT")) {
        return false;
    }
    // A digit 1 to 9, or a superscript 1, 2 or 3 (in UTF-8), which Windows
    // also reserves.
    const std::string_view number{std::string_view{base}.substr(3)};
    constexpr std::array<std::string_view, 3> superscripts{"\xC2\xB9", "\xC2\xB2", "\xC2\xB3"};
    return (number.size() == 1 && number[0] >= '1' && number[0] <= '9') ||
           std::ranges::find(superscripts, number) != superscripts.end();
}

// The relative path `text` names, or a failure unless it is plain: UTF-8
// without control bytes, no root, no `..` and no device names.
std::filesystem::path relativeMeshPath(const Statement& statement, std::string_view text)
{
    const bool control{std::ranges::any_of(text, [](char c) {
        const auto byte{static_cast<unsigned char>(c)};
        return byte < 0x20 || byte == 0x7F;
    })};
    if (control || !isValidUtf8(text)) {
        statement.fail(
            std::format("mesh path {} must be UTF-8 without control characters", quoted(text)));
    }
    // Scene files are UTF-8 whatever the platform's narrow encoding.
    const std::filesystem::path relative{std::u8string(text.begin(), text.end())};
    const bool rooted{relative.is_absolute() || relative.has_root_name() ||
                      relative.has_root_directory()};
    if (rooted || std::ranges::find(relative, std::filesystem::path{".."}) != relative.end()) {
        statement.fail(std::format("mesh path {} must be relative to the scene and not use '..'",
                                   quoted(text)));
    }
    if (std::ranges::any_of(relative, isDeviceName)) {
        statement.fail(std::format("mesh path {} names a device", quoted(text)));
    }
    return relative;
}

// The regular file `text` names inside `base`, with symbolic links resolved;
// fails unless it lies inside `base` once they are. The file is opened by this
// resolved path, which narrows, but cannot close, the window in which the
// directory could be changed between the check and the open.
std::filesystem::path meshFile(const Statement& statement, const std::filesystem::path& base,
                               std::string_view text)
{
    const std::filesystem::path relative{relativeMeshPath(statement, text)};
    std::error_code error;
    const std::filesystem::path real_base{std::filesystem::canonical(base, error)};
    if (error) {
        statement.fail("the scene's directory cannot be resolved");
    }
    const std::filesystem::path real_file{
        std::filesystem::weakly_canonical(real_base / relative, error)};
    if (error) {
        statement.fail(std::format("cannot resolve mesh path {}", quoted(text)));
    }
    const auto [base_end, file_part]{std::ranges::mismatch(real_base, real_file)};
    if (base_end != real_base.end()) {
        statement.fail(
            std::format("mesh path {} leads outside the scene's directory", quoted(text)));
    }
    const std::filesystem::file_status status{std::filesystem::status(real_file, error)};
    if (status.type() == std::filesystem::file_type::not_found) {
        statement.fail(std::format("mesh file {} does not exist", quoted(text)));
    }
    if (status.type() != std::filesystem::file_type::regular) {
        statement.fail(std::format("mesh path {} is not a regular file", quoted(text)));
    }
    return real_file;
}

// What is left of `budget` once `mesh` is counted against it.
ObjLimits remainingAfter(const ObjLimits& budget, const TriangleMesh& mesh) noexcept
{
    ObjLimits rest{budget};
    rest.max_vertices -= mesh.positions().size();
    rest.max_texture_coordinates -= mesh.textureCoordinates().size();
    rest.max_triangles -= mesh.faces().size();
    return rest;
}

// The positions that the triangles `mesh` keeps refer to, each once.
std::vector<Point3r> usedPositions(const TriangleMesh& mesh)
{
    const std::span<const Point3r> positions{mesh.positions()};
    std::vector<bool> used(positions.size(), false);
    for (const auto& [i0, i1, i2] : mesh.faces()) {
        if (formsTriangle(positions[i0], positions[i1], positions[i2])) {
            used[i0] = true;
            used[i1] = true;
            used[i2] = true;
        }
    }
    std::vector<Point3r> result;
    for (std::size_t index{0}; index < positions.size(); ++index) {
        if (used[index]) {
            result.push_back(positions[index]);
        }
    }
    return result;
}

// The lowest height `transform` puts any of `points` at, or nothing if any
// height is not finite (or there are no points).
std::optional<Real> lowestHeight(const Transform& transform, const std::vector<Point3r>& points)
{
    std::optional<Real> lowest;
    for (const Point3r& point : points) {
        const Real height{transform.apply(point).y};
        if (!std::isfinite(height)) {
            return std::nullopt;
        }
        lowest = lowest ? std::min(*lowest, height) : height;
    }
    return lowest;
}

// The words that start an instance operation, or its material.
bool isInstanceKeyword(std::string_view word) noexcept
{
    constexpr std::array<std::string_view, 6> keywords{"material", "translate", "scale",
                                                       "rotate",   "fit",       "ground"};
    return std::ranges::find(keywords, word) != keywords.end();
}

// Builds the scene statement by statement.
class SceneReader {
public:
    SceneReader(const SceneLimits& reader_limits,
                std::optional<std::filesystem::path> mesh_directory)
        : limits{reader_limits}, base_directory{std::move(mesh_directory)},
          mesh_budget{reader_limits.mesh_limits},
          ground_points_left{reader_limits.max_ground_points}
    {
    }

    void readLine(std::string_view line, std::size_t line_number)
    {
        Statement statement{line.substr(0, line.find('#')), line_number};
        const auto keyword{statement.next()};
        if (!keyword) {
            return;
        }
        const auto handler{handlers().find(*keyword)};
        if (handler == handlers().end()) {
            statement.fail(std::format("unknown statement {}", quoted(*keyword)));
        }
        (this->*(handler->second))(statement);
    }

    [[nodiscard]] SceneDescription finish() &&
    {
        if (!camera) {
            throw SceneParseError{0, "the scene has no camera"};
        }
        return {.scene = std::move(scene), .camera = *camera};
    }

private:
    using Handler = void (SceneReader::*)(Statement&);

    static const std::map<std::string_view, Handler, std::less<>>& handlers()
    {
        static const std::map<std::string_view, Handler, std::less<>> table{
            {"camera", &SceneReader::readCamera},
            {"sky", &SceneReader::readSky},
            {"material", &SceneReader::readMaterial},
            {"sphere", &SceneReader::readSphere},
            {"point_light", &SceneReader::readPointLight},
            {"area_light", &SceneReader::readAreaLight},
            {"mesh", &SceneReader::readMesh},
            {"instance", &SceneReader::readInstance}};
        return table;
    }

    void readCamera(Statement& statement)
    {
        if (camera) {
            statement.fail("the scene already has a camera");
        }
        std::optional<Point3r> from;
        std::optional<Point3r> at;
        CameraSettings settings{};
        statement.readFields([&](std::string_view field) {
            if (field == "from") {
                from = statement.point("the camera position");
            } else if (field == "at") {
                at = statement.point("the point the camera looks at");
            } else if (field == "up") {
                settings.up = statement.vector("the camera's up direction");
            } else if (field == "fov") {
                settings.vertical_fov_degrees = statement.number("the field of view");
            } else {
                return false;
            }
            return true;
        });
        settings.look_from = statement.required(from, "from");
        settings.look_at = statement.required(at, "at");
        checkCamera(statement, settings);
        camera = settings;
    }

    // Fails unless a camera can be built from `settings` at the default aspect
    // ratio; the caller still has to build it at its own.
    static void checkCamera(const Statement& statement, const CameraSettings& settings)
    {
        try {
            static_cast<void>(PerspectiveCamera{settings});
        } catch (const std::invalid_argument& error) {
            statement.fail(error.what());
        }
    }

    void readSky(Statement& statement)
    {
        if (sky_given) {
            statement.fail("the scene already has a sky");
        }
        // Without `nadir` or `zenith` first, the sky is one radiance everywhere.
        const auto first{statement.peek()};
        if (first && (*first == "nadir" || *first == "zenith")) {
            readSkyGradient(statement);
        } else {
            scene.setSky(SkyLight::uniform(statement.emission("the sky's radiance")));
            statement.expectEnd();
        }
        sky_given = true;
    }

    void readSkyGradient(Statement& statement)
    {
        std::optional<Rgb> nadir;
        std::optional<Rgb> zenith;
        statement.readFields([&](std::string_view field) {
            if (field == "nadir") {
                nadir = statement.emission("the radiance from below");
            } else if (field == "zenith") {
                zenith = statement.emission("the radiance from above");
            } else {
                return false;
            }
            return true;
        });
        scene.setSky(SkyLight{.nadir = statement.required(nadir, "nadir"),
                              .zenith = statement.required(zenith, "zenith")});
    }

    void readMaterial(Statement& statement)
    {
        if (materials.size() >= limits.max_materials) {
            statement.fail(std::format("more than {} materials", limits.max_materials));
        }
        const std::string_view name{statement.name("a material name")};
        if (materials.contains(name)) {
            statement.fail(std::format("material {} is already defined", quoted(name)));
        }
        const std::string_view type{statement.word("a material type")};
        materials.emplace(std::string{name},
                          type == "mix" ? readMix(statement) : materialOfType(statement, type));
    }

    static Material materialOfType(Statement& statement, std::string_view type)
    {
        if (type == "diffuse") {
            return readDiffuse(statement);
        }
        if (type == "checker") {
            return readChecker(statement);
        }
        if (type == "perlin") {
            return readPerlin(statement);
        }
        if (type == "conductor") {
            return readConductor(statement);
        }
        if (type == "dielectric") {
            return readDielectric(statement);
        }
        statement.fail(std::format("unknown material type {}", quoted(type)));
    }

    static Material readDiffuse(Statement& statement)
    {
        std::optional<Rgb> color;
        statement.readFields([&](std::string_view field) {
            if (field != "color") {
                return false;
            }
            color = statement.reflectance("the color");
            return true;
        });
        return DiffuseMaterial{.albedo =
                                   ConstantTexture{.color = statement.required(color, "color")}};
    }

    static Material readChecker(Statement& statement)
    {
        std::optional<Rgb> even;
        std::optional<Rgb> odd;
        std::optional<Real> frequency;
        statement.readFields([&](std::string_view field) {
            if (field == "even") {
                even = statement.reflectance("the even color");
            } else if (field == "odd") {
                odd = statement.reflectance("the odd color");
            } else if (field == "frequency") {
                frequency = statement.positive("the checker frequency");
            } else {
                return false;
            }
            return true;
        });
        return DiffuseMaterial{
            .albedo = CheckerTexture{.even = statement.required(even, "even"),
                                     .odd = statement.required(odd, "odd"),
                                     .frequency = statement.required(frequency, "frequency")}};
    }

    static Material readPerlin(Statement& statement)
    {
        std::optional<Rgb> low;
        std::optional<Rgb> high;
        std::optional<Real> scale;
        Real veins{0};
        statement.readFields([&](std::string_view field) {
            if (field == "low") {
                low = statement.reflectance("the low color");
            } else if (field == "high") {
                high = statement.reflectance("the high color");
            } else if (field == "scale") {
                scale = statement.positive("the noise scale");
            } else if (field == "veins") {
                veins = statement.inRange("the veins", 0, max_perlin_veins);
            } else {
                return false;
            }
            return true;
        });
        return DiffuseMaterial{.albedo = PerlinTexture{.low = statement.required(low, "low"),
                                                       .high = statement.required(high, "high"),
                                                       .scale = statement.required(scale, "scale"),
                                                       .veins = veins}};
    }

    static Material readConductor(Statement& statement)
    {
        std::optional<Rgb> eta;
        std::optional<Rgb> k;
        Real roughness{0};
        statement.readFields([&](std::string_view field) {
            if (field == "eta") {
                eta = statement.color("the refractive index", min_conductor_eta, max_conductor_eta);
            } else if (field == "k") {
                k = statement.color("the absorption", 0, max_conductor_k);
            } else if (field == "roughness") {
                roughness = statement.inRange("the roughness", 0, max_roughness);
            } else {
                return false;
            }
            return true;
        });
        const ConductorMaterial material{.eta = statement.required(eta, "eta"),
                                         .k = statement.required(k, "k"),
                                         .roughness = roughness};
        checkConductorChannels(statement, material);
        return material;
    }

    // Fails for a channel with eta 1 and k 0, a conductor indistinguishable from
    // the air around it, for which the Fresnel equations are undefined at
    // grazing incidence.
    static void checkConductorChannels(const Statement& statement,
                                       const ConductorMaterial& material)
    {
        const std::array<Real, 3> eta{material.eta.r, material.eta.g, material.eta.b};
        const std::array<Real, 3> k{material.k.r, material.k.g, material.k.b};
        for (std::size_t channel{0}; channel < eta.size(); ++channel) {
            if (eta.at(channel) == 1 && k.at(channel) == 0) {
                statement.fail("a conductor channel cannot have eta 1 and k 0");
            }
        }
    }

    static Material readDielectric(Statement& statement)
    {
        std::optional<Real> ior;
        Real roughness{0};
        statement.readFields([&](std::string_view field) {
            if (field == "ior") {
                ior = statement.inRange("the index of refraction", min_ior, max_ior);
            } else if (field == "roughness") {
                roughness = statement.inRange("the roughness", 0, max_roughness);
            } else {
                return false;
            }
            return true;
        });
        return DielectricMaterial{.ior = statement.required(ior, "ior"), .roughness = roughness};
    }

    // Mixes two materials defined earlier, neither of them a mix itself.
    [[nodiscard]] Material readMix(Statement& statement) const
    {
        std::optional<BasicMaterial> first;
        std::optional<BasicMaterial> second;
        std::optional<Real> scale;
        Real veins{0};
        Real threshold{0.5};
        statement.readFields([&](std::string_view field) {
            if (field == "first") {
                first = basicMaterialNamed(statement);
            } else if (field == "second") {
                second = basicMaterialNamed(statement);
            } else if (field == "scale") {
                scale = statement.positive("the noise scale");
            } else if (field == "veins") {
                veins = statement.inRange("the veins", 0, max_perlin_veins);
            } else if (field == "threshold") {
                threshold = statement.inRange("the threshold", 0, 1);
            } else {
                return false;
            }
            return true;
        });
        return MixMaterial{.first = statement.required(first, "first"),
                           .second = statement.required(second, "second"),
                           .pattern = {.scale = statement.required(scale, "scale"), .veins = veins},
                           .threshold = threshold};
    }

    // The material named by the next word, which must not be a mix: mixes hold
    // only basic materials.
    [[nodiscard]] BasicMaterial basicMaterialNamed(Statement& statement) const
    {
        const std::string_view name{statement.peek().value_or("")};
        const Material& material{materialNamed(statement)};
        if (std::holds_alternative<MixMaterial>(material)) {
            statement.fail(std::format("material {} is a mix, and a mix cannot hold another mix",
                                       quoted(name)));
        }
        return std::visit(
            [](const auto& alternative) -> BasicMaterial {
                if constexpr (std::is_same_v<std::remove_cvref_t<decltype(alternative)>,
                                             MixMaterial>) {
                    assert(false && "mixes are rejected before the visit");
                    return {};
                } else {
                    return alternative;
                }
            },
            material);
    }

    // The material named by the next word.
    [[nodiscard]] const Material& materialNamed(Statement& statement) const
    {
        const std::string_view name{statement.name("a material name")};
        const auto found{materials.find(name)};
        if (found == materials.end()) {
            statement.fail(std::format("material {} is not defined", quoted(name)));
        }
        return found->second;
    }

    // Counts one more object, failing past the limit.
    void countObject(const Statement& statement)
    {
        if (object_count >= limits.max_objects) {
            statement.fail(std::format("more than {} objects", limits.max_objects));
        }
        ++object_count;
    }

    // A sphere at `center` with the radius `radius`, or a failure saying why not.
    static Sphere sphereAt(const Statement& statement, Point3r center, Real radius)
    {
        try {
            return Sphere{center, radius};
        } catch (const std::invalid_argument& error) {
            statement.fail(error.what());
        }
    }

    void readSphere(Statement& statement)
    {
        countObject(statement);
        const Point3r center{statement.point("the sphere center")};
        std::optional<Real> radius;
        const Material* material{nullptr};
        statement.readFields([&](std::string_view field) {
            if (field == "radius") {
                radius = statement.number("a radius");
            } else if (field == "material") {
                material = &materialNamed(statement);
            } else {
                return false;
            }
            return true;
        });
        const Sphere sphere{sphereAt(statement, center, statement.required(radius, "radius"))};
        if (material == nullptr) {
            statement.fail("missing 'material'");
        }
        scene.add(sphere, *material);
    }

    void readPointLight(Statement& statement)
    {
        countObject(statement);
        const Point3r position{statement.point("the light position")};
        std::optional<Rgb> intensity;
        statement.readFields([&](std::string_view field) {
            if (field != "intensity") {
                return false;
            }
            intensity = statement.emission("the intensity");
            return true;
        });
        scene.addLight(PointLight{.position = position,
                                  .intensity = statement.required(intensity, "intensity")});
    }

    void readAreaLight(Statement& statement)
    {
        countObject(statement);
        const Point3r center{statement.point("the light center")};
        std::optional<Real> radius;
        std::optional<Rgb> radiance;
        Material material{DiffuseMaterial{}};
        statement.readFields([&](std::string_view field) {
            if (field == "radius") {
                radius = statement.number("a radius");
            } else if (field == "radiance") {
                radiance = statement.emission("the radiance");
            } else if (field == "material") {
                material = materialNamed(statement);
            } else {
                return false;
            }
            return true;
        });
        const Sphere sphere{sphereAt(statement, center, statement.required(radius, "radius"))};
        scene.addAreaLight(sphere, statement.required(radiance, "radiance"), material);
    }

    void readMesh(Statement& statement)
    {
        if (!base_directory) {
            statement.fail("meshes can only be read from a scene file, which they sit beside");
        }
        if (meshes.size() >= limits.max_meshes) {
            statement.fail(std::format("more than {} meshes", limits.max_meshes));
        }
        const std::string_view name{statement.name("a mesh name")};
        if (meshes.contains(name)) {
            statement.fail(std::format("mesh {} is already defined", quoted(name)));
        }
        const std::string_view path{statement.word("a mesh file")};
        const bool smooth{statement.peek() == "smooth"};
        if (smooth) {
            static_cast<void>(statement.next());
        }
        statement.expectEnd();
        const TriangleMesh mesh{
            loadObj(statement, meshFile(statement, *base_directory, path), path)};
        std::vector<Point3r> points{usedPositions(mesh)};
        if (points.empty()) {
            statement.fail(std::format("mesh {} has no triangles", quoted(path)));
        }
        std::shared_ptr<const MeshGeometry> geometry;
        try {
            geometry = std::make_shared<const MeshGeometry>(mesh, smooth ? MeshShading::Smooth
                                                                         : MeshShading::Flat);
        } catch (const std::invalid_argument& error) {
            statement.fail(std::format("mesh {}: {}", quoted(path), error.what()));
        }
        mesh_budget = remainingAfter(mesh_budget, mesh);
        meshes.emplace(std::string{name},
                       LoadedMesh{.geometry = std::move(geometry), .points = std::move(points)});
    }

    // The OBJ file at `file`, read within what is left of the mesh budget.
    [[nodiscard]] TriangleMesh loadObj(const Statement& statement,
                                       const std::filesystem::path& file,
                                       std::string_view text) const
    {
        try {
            return readObj(file, mesh_budget);
        } catch (const ObjParseError& error) {
            statement.fail(std::format("mesh {}: {}", quoted(text), error.what()));
        } catch (const std::runtime_error&) {
            statement.fail(std::format("cannot read mesh file {}", quoted(text)));
        }
    }

    // The mesh named by the next word.
    [[nodiscard]] const LoadedMesh& meshNamed(Statement& statement) const
    {
        const std::string_view name{statement.name("a mesh name")};
        const auto found{meshes.find(name)};
        if (found == meshes.end()) {
            statement.fail(std::format("mesh {} is not defined", quoted(name)));
        }
        return found->second;
    }

    void readInstance(Statement& statement)
    {
        countObject(statement);
        const LoadedMesh& mesh{meshNamed(statement)};
        const Material* material{nullptr};
        Transform placement{};
        std::size_t operations{0};
        while (const auto word{statement.next()}) {
            if (*word == "material") {
                if (material != nullptr) {
                    statement.fail("'material' is given twice");
                }
                material = &materialNamed(statement);
                continue;
            }
            if (++operations > limits.max_instance_operations) {
                statement.fail(
                    std::format("more than {} operations", limits.max_instance_operations));
            }
            placement = applied(statement, *word, mesh, placement);
        }
        if (material == nullptr) {
            statement.fail("missing 'material'");
        }
        addInstance(statement, mesh, placement, *material);
    }

    void addInstance(const Statement& statement, const LoadedMesh& mesh, const Transform& placement,
                     const Material& material)
    {
        try {
            scene.add(MeshInstance{mesh.geometry, placement}, material);
        } catch (const std::invalid_argument& error) {
            statement.fail(error.what());
        }
    }

    // `placement` followed by the operation `name`, whose values come next.
    Transform applied(Statement& statement, std::string_view name, const LoadedMesh& mesh,
                      const Transform& placement)
    {
        try {
            if (name == "translate") {
                return Transform::translation(statement.vector("the translation")) * placement;
            }
            if (name == "scale") {
                return scaling(statement) * placement;
            }
            if (name == "rotate") {
                const Vector3r axis{statement.vector("the rotation axis")};
                return Transform::rotation(axis, statement.number("the rotation angle")) *
                       placement;
            }
            if (name == "fit") {
                return fitted(statement, mesh, placement, statement.positive("the fitted size"));
            }
            if (name == "ground") {
                return grounded(statement, mesh, placement);
            }
        } catch (const std::invalid_argument& error) {
            statement.fail(error.what());
        }
        statement.fail(std::format("unknown instance operation {}", quoted(name)));
    }

    // `scale S` or `scale X Y Z`: one factor if the statement ends or another
    // operation (or the material) follows, three otherwise.
    static Transform scaling(Statement& statement)
    {
        const Real first{statement.number("a scale factor")};
        const auto next{statement.peek()};
        if (!next || isInstanceKeyword(*next)) {
            return Transform::scaling({first, first, first});
        }
        const Real y{statement.number("the y scale factor")};
        return Transform::scaling({first, y, statement.number("the z scale factor")});
    }

    // `placement`, then centred on the origin and scaled so the longest side of
    // its box is `size`.
    static Transform fitted(const Statement& statement, const LoadedMesh& mesh,
                            const Transform& placement, Real size)
    {
        const Bounds3r box{placement.apply(mesh.geometry->bounds())};
        if (!isFinite(box.min) || !isFinite(box.max)) {
            statement.fail("the instance is out of range before 'fit'");
        }
        const Vector3r extent{box.extent()};
        const Real scale{size / std::max({extent.x, extent.y, extent.z})};
        if (!std::isfinite(scale) || !(scale > 0)) {
            statement.fail("the instance is too large or too small to fit");
        }
        // Halved before adding, so huge coordinates cannot overflow.
        const Vector3r center{box.min.x / 2 + box.max.x / 2, box.min.y / 2 + box.max.y / 2,
                              box.min.z / 2 + box.max.z / 2};
        return Transform::scaling({scale, scale, scale}) * Transform::translation(-center) *
               placement;
    }

    // `placement`, then lifted so its lowest point is at height zero. Every
    // `ground` in the scene spends from one budget of points to transform.
    Transform grounded(const Statement& statement, const LoadedMesh& mesh,
                       const Transform& placement)
    {
        if (mesh.points.size() > ground_points_left) {
            statement.fail(std::format("'ground' would examine more than {} points in the scene",
                                       limits.max_ground_points));
        }
        ground_points_left -= mesh.points.size();
        const auto lowest{lowestHeight(placement, mesh.points)};
        if (!lowest) {
            statement.fail("the instance is out of range before 'ground'");
        }
        return Transform::translation({0, -*lowest, 0}) * placement;
    }

    SceneLimits limits;
    std::optional<std::filesystem::path> base_directory;
    ObjLimits mesh_budget;
    std::size_t ground_points_left;
    std::map<std::string, LoadedMesh, std::less<>> meshes;
    Scene scene;
    std::optional<CameraSettings> camera;
    bool sky_given{false};
    std::map<std::string, Material, std::less<>> materials;
    std::size_t object_count{0};
};

} // namespace

SceneParseError::SceneParseError(std::size_t line, std::string_view problem)
    : std::runtime_error{line == 0 ? std::format("scene: {}", problem)
                                   : std::format("scene line {}: {}", line, problem)},
      line_number{line}
{
}

namespace {

SceneDescription readSceneFrom(std::istream& input,
                               std::optional<std::filesystem::path> base_directory,
                               const SceneLimits& limits)
{
    LineReader lines{input, limits.max_line_length, "scene"};
    if (base_directory && base_directory->empty()) {
        base_directory = ".";
    }
    SceneReader reader{limits, std::move(base_directory)};
    try {
        while (const auto line{lines.next()}) {
            reader.readLine(*line, lines.lineNumber());
        }
    } catch (const LineTooLongError& error) {
        throw SceneParseError{error.line(),
                              std::format("line longer than {} bytes", error.limit())};
    }
    return std::move(reader).finish();
}

} // namespace

SceneDescription readScene(std::istream& input, const std::filesystem::path& base_directory,
                           const SceneLimits& limits)
{
    return readSceneFrom(input, base_directory, limits);
}

SceneDescription readScene(std::istream& input, const SceneLimits& limits)
{
    return readSceneFrom(input, std::nullopt, limits);
}

SceneDescription readScene(const std::filesystem::path& path, const SceneLimits& limits)
{
    // Checked first: some platforms open a directory as a stream whose reads
    // then fail, which would give an error without the path.
    std::error_code error;
    const std::filesystem::file_type type{std::filesystem::status(path, error).type()};
    // A failed lookup (such as a directory without search permission) falls
    // through to the open, which reports it.
    if (!error && type != std::filesystem::file_type::not_found &&
        type != std::filesystem::file_type::regular) {
        throw std::runtime_error{
            std::format("scene file '{}' is not a regular file", pathText(path))};
    }
    std::ifstream file{path, std::ios::binary};
    if (!file) {
        throw std::runtime_error{std::format("cannot open scene file '{}'", pathText(path))};
    }
    return readScene(file, path.parent_path(), limits);
}

} // namespace albedo
