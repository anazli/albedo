#include "albedo/scene/scene_file.hpp"

#include "albedo/color/rgb.hpp"
#include "albedo/core/point3.hpp"
#include "albedo/core/real.hpp"
#include "albedo/core/vector3.hpp"
#include "albedo/geometry/sphere.hpp"
#include "albedo/io/line_reader.hpp"
#include "albedo/io/tokens.hpp"
#include "albedo/light/point_light.hpp"
#include "albedo/light/sky_light.hpp"
#include "albedo/material/material.hpp"
#include "albedo/texture/checker_texture.hpp"
#include "albedo/texture/constant_texture.hpp"

#include <algorithm>
#include <array>
#include <concepts>
#include <format>
#include <fstream>
#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>
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

// Builds the scene statement by statement.
class SceneReader {
public:
    explicit SceneReader(const SceneLimits& reader_limits) : limits{reader_limits} {}

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
            {"area_light", &SceneReader::readAreaLight}};
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
        materials.emplace(std::string{name}, materialOfType(statement, type));
    }

    static Material materialOfType(Statement& statement, std::string_view type)
    {
        if (type == "diffuse") {
            return readDiffuse(statement);
        }
        if (type == "checker") {
            return readChecker(statement);
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

    SceneLimits limits;
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

SceneDescription readScene(std::istream& input, const SceneLimits& limits)
{
    LineReader lines{input, limits.max_line_length, "scene"};
    SceneReader reader{limits};
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

SceneDescription readScene(const std::filesystem::path& path, const SceneLimits& limits)
{
    std::ifstream file{path, std::ios::binary};
    if (!file) {
        throw std::runtime_error{std::format("cannot open scene file '{}'", path.string())};
    }
    return readScene(file, limits);
}

} // namespace albedo
