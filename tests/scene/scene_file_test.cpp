#include "albedo/scene/scene_file.hpp"

#include "albedo/light/light.hpp"
#include "support/expect_near.hpp"
#include "support/require_value.hpp"
#include "support/temporary_directory.hpp"
#include "support/temporary_file.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <filesystem>
#include <fstream>
#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>

namespace albedo {
namespace {

using test_support::CurrentDirectory;
using test_support::default_tolerance;
using test_support::exact_tolerance;
using test_support::expectNear;
using test_support::requireValue;
using test_support::TemporaryDirectory;
using test_support::TemporaryFile;

SceneDescription readSceneText(std::string_view text, const SceneLimits& limits = {})
{
    std::istringstream input{std::string{text}};
    return readScene(input, limits);
}

// The line a SceneParseError reports, or a value no file has when parsing
// unexpectedly succeeds.
constexpr std::size_t no_error{std::numeric_limits<std::size_t>::max()};

std::size_t errorLine(std::string_view text, const SceneLimits& limits = {})
{
    try {
        static_cast<void>(readSceneText(text, limits));
    } catch (const SceneParseError& error) {
        return error.line();
    }
    return no_error;
}

// The message of the SceneParseError `text` causes, or empty if none.
std::string errorMessage(std::string_view text)
{
    try {
        static_cast<void>(readSceneText(text));
    } catch (const SceneParseError& error) {
        return error.what();
    }
    return {};
}

constexpr std::string_view camera_line{"camera from 0 0 0 at 0 0 -1\n"};

// `camera_line` followed by `rest`, so the statements under test start on line 2.
std::string withCamera(std::string_view rest)
{
    return std::string{camera_line} + std::string{rest};
}

constexpr Interval positive_t{.min = 0.0, .max = std::numeric_limits<Real>::infinity()};
constexpr Ray forward_ray{.origin = {0.0, 0.0, 0.0}, .direction = {0.0, 0.0, -1.0}};
const Vector3r back_towards_origin{0.0, 0.0, 1.0};

// How the surface first hit along -z from the origin reflects light straight back.
Rgb reflectanceAhead(const Scene& scene)
{
    const HitRecord hit{requireValue(scene.intersect(forward_ray, positive_t))};
    return scene.scatteringAt(hit).evaluate(back_towards_origin, back_towards_origin);
}

// --- camera

TEST(SceneFileTest, aSceneNeedsOnlyACamera)
{
    const SceneDescription description{readSceneText(camera_line)};

    EXPECT_EQ(description.scene.shapeCount(), 0U);
    expectNear(description.camera.look_from, Point3r{0.0, 0.0, 0.0}, exact_tolerance);
    expectNear(description.camera.look_at, Point3r{0.0, 0.0, -1.0}, exact_tolerance);
}

TEST(SceneFileTest, cameraUpAndFieldOfViewHaveDefaults)
{
    const CameraSettings camera{readSceneText(camera_line).camera};

    expectNear(camera.up, CameraSettings{}.up, exact_tolerance);
    EXPECT_DOUBLE_EQ(camera.vertical_fov_degrees, CameraSettings{}.vertical_fov_degrees);
}

TEST(SceneFileTest, cameraFieldsMayComeInAnyOrder)
{
    const CameraSettings camera{
        readSceneText("camera fov 38 up 0 0 1 at 1 2 3 from 4 5 6\n").camera};

    expectNear(camera.look_from, Point3r{4.0, 5.0, 6.0}, exact_tolerance);
    expectNear(camera.look_at, Point3r{1.0, 2.0, 3.0}, exact_tolerance);
    expectNear(camera.up, Vector3r{0.0, 0.0, 1.0}, exact_tolerance);
    EXPECT_DOUBLE_EQ(camera.vertical_fov_degrees, 38.0);
}

TEST(SceneFileTest, rejectsASceneWithoutACamera)
{
    EXPECT_EQ(errorLine("sky 0.1 0.1 0.1\n"), 0U);
    EXPECT_EQ(errorLine(""), 0U);
}

TEST(SceneFileTest, rejectsASecondCamera)
{
    EXPECT_EQ(errorLine(withCamera("camera from 1 0 0 at 0 0 0\n")), 2U);
}

TEST(SceneFileTest, rejectsACameraWithoutFromOrAt)
{
    EXPECT_EQ(errorLine("camera at 0 0 -1\n"), 1U);
    EXPECT_EQ(errorLine("camera from 0 0 0\n"), 1U);
}

TEST(SceneFileTest, rejectsACameraThatCannotBeBuilt)
{
    // Looking at its own position, an out-of-range field of view, and up along
    // the view direction.
    EXPECT_EQ(errorLine("camera from 1 1 1 at 1 1 1\n"), 1U);
    EXPECT_EQ(errorLine("camera from 0 0 0 at 0 0 -1 fov 180\n"), 1U);
    EXPECT_EQ(errorLine("camera from 0 0 0 at 0 0 -1 up 0 0 1\n"), 1U);
}

// --- sky

TEST(SceneFileTest, skyIsBlackUnlessGiven)
{
    const SkyLight sky{readSceneText(camera_line).scene.sky()};

    EXPECT_TRUE(isBlack(sky.nadir));
    EXPECT_TRUE(isBlack(sky.zenith));
}

TEST(SceneFileTest, readsAUniformSky)
{
    const SkyLight sky{readSceneText(withCamera("sky 0.05 0.06 0.08\n")).scene.sky()};

    expectNear(sky.nadir, Rgb{0.05, 0.06, 0.08}, exact_tolerance);
    expectNear(sky.zenith, Rgb{0.05, 0.06, 0.08}, exact_tolerance);
}

TEST(SceneFileTest, readsASkyGradient)
{
    const SkyLight sky{readSceneText(withCamera("sky zenith 0.5 0.7 1 nadir 1 1 1\n")).scene.sky()};

    expectNear(sky.nadir, Rgb{1.0, 1.0, 1.0}, exact_tolerance);
    expectNear(sky.zenith, Rgb{0.5, 0.7, 1.0}, exact_tolerance);
}

TEST(SceneFileTest, rejectsASecondSky)
{
    EXPECT_EQ(errorLine(withCamera("sky 0 0 0\nsky 1 1 1\n")), 3U);
}

TEST(SceneFileTest, rejectsAUniformSkyWithExtraWords)
{
    EXPECT_EQ(errorLine(withCamera("sky 1 1 1 extra\n")), 2U);
}

TEST(SceneFileTest, rejectsAnUnknownSkyField)
{
    EXPECT_EQ(errorLine(withCamera("sky nadir 1 1 1 zenith 1 1 1 horizon 1 1 1\n")), 2U);
    EXPECT_EQ(errorLine(withCamera("sky colour 1 1 1\n")), 2U);
}

TEST(SceneFileTest, nonFiniteUniformSkyIsReportedAsSuch)
{
    EXPECT_NE(errorMessage(withCamera("sky inf 0 0\n")).find("finite number"), std::string::npos);
}

TEST(SceneFileTest, rejectsAnIncompleteSkyGradient)
{
    EXPECT_EQ(errorLine(withCamera("sky zenith 1 1 1\n")), 2U);
}

// --- materials and spheres

TEST(SceneFileTest, readsASphereMadeOfADiffuseMaterial)
{
    const SceneDescription description{
        readSceneText(withCamera("material red diffuse color 0.6 0.1 0.1\n"
                                 "sphere 0 0 -5 radius 1 material red\n"))};

    const HitRecord hit{requireValue(description.scene.intersect(forward_ray, positive_t))};

    EXPECT_EQ(description.scene.shapeCount(), 1U);
    EXPECT_NEAR(hit.t, 4.0, default_tolerance);
    expectNear(reflectanceAhead(description.scene), Rgb{0.6, 0.1, 0.1} * inv_pi);
}

TEST(SceneFileTest, sphereFieldsMayComeInAnyOrder)
{
    const SceneDescription description{
        readSceneText(withCamera("material red diffuse color 0.6 0.1 0.1\n"
                                 "sphere 0 0 -5 material red radius 2\n"))};

    const HitRecord hit{requireValue(description.scene.intersect(forward_ray, positive_t))};

    EXPECT_NEAR(hit.t, 3.0, default_tolerance);
}

TEST(SceneFileTest, readsACheckerMaterial)
{
    // The ray hits the sphere at +z, where u = 0.25 and v = 0.5: with two squares
    // per unit that is square (0, 1), an odd one.
    const SceneDescription description{readSceneText(
        withCamera("material tiles checker even 0.9 0.9 0.9 odd 0.1 0.2 0.3 frequency 2\n"
                   "sphere 0 0 -5 radius 1 material tiles\n"))};

    expectNear(reflectanceAhead(description.scene), Rgb{0.1, 0.2, 0.3} * inv_pi);
}

TEST(SceneFileTest, readsAConductorMaterial)
{
    // A smooth conductor mirrors: the sample from straight on comes straight back.
    const SceneDescription description{readSceneText(
        withCamera("material gold conductor eta 0.16 0.42 1.38 k 3.6 2.47 1.92 roughness 0\n"
                   "sphere 0 0 -5 radius 1 material gold\n"))};
    const HitRecord hit{requireValue(description.scene.intersect(forward_ray, positive_t))};

    const ScatterSample sample{requireValue(
        description.scene.scatteringAt(hit).sample(back_towards_origin, 0.5, {0.5, 0.5}))};

    expectNear(sample.direction, back_towards_origin);
}

// The direction the surface first hit along -z from the origin scatters light
// coming straight back, for random numbers away from the centre (a rough
// surface would scatter them elsewhere).
Vector3r scatteredOffCentre(const Scene& scene, Real u_choice)
{
    const HitRecord hit{requireValue(scene.intersect(forward_ray, positive_t))};
    return requireValue(scene.scatteringAt(hit).sample(back_towards_origin, u_choice, {0.9, 0.2}))
        .direction;
}

TEST(SceneFileTest, conductorRoughnessDefaultsToZero)
{
    const SceneDescription description{
        readSceneText(withCamera("material gold conductor eta 0.16 0.42 1.38 k 3.6 2.47 1.92\n"
                                 "sphere 0 0 -5 radius 1 material gold\n"))};

    expectNear(scatteredOffCentre(description.scene, 0.5), back_towards_origin);
}

TEST(SceneFileTest, dielectricRoughnessDefaultsToZero)
{
    const SceneDescription description{
        readSceneText(withCamera("material glass dielectric ior 1.5\n"
                                 "sphere 0 0 -5 radius 1 material glass\n"))};

    expectNear(scatteredOffCentre(description.scene, 0.99), Vector3r{0.0, 0.0, -1.0});
}

TEST(SceneFileTest, readsADielectricMaterial)
{
    // Smooth glass seen straight on transmits most light straight through.
    const SceneDescription description{
        readSceneText(withCamera("material glass dielectric ior 1.5\n"
                                 "sphere 0 0 -5 radius 1 material glass\n"))};
    const HitRecord hit{requireValue(description.scene.intersect(forward_ray, positive_t))};

    const ScatterSample sample{requireValue(
        description.scene.scatteringAt(hit).sample(back_towards_origin, 0.99, {0.5, 0.5}))};

    expectNear(sample.direction, Vector3r{0.0, 0.0, -1.0});
}

TEST(SceneFileTest, materialsMustBeDefinedBeforeUse)
{
    EXPECT_EQ(errorLine(withCamera("sphere 0 0 -5 radius 1 material red\n"
                                   "material red diffuse color 1 0 0\n")),
              2U);
}

TEST(SceneFileTest, rejectsAMaterialNamedTwice)
{
    EXPECT_EQ(errorLine(withCamera("material red diffuse color 1 0 0\n"
                                   "material red diffuse color 0.5 0 0\n")),
              3U);
}

TEST(SceneFileTest, rejectsMaterialNamesThatAreNotIdentifiers)
{
    for (const std::string_view name : {"1red", "-red", "red!", "r.ed"}) {
        EXPECT_EQ(errorLine(withCamera("material " + std::string{name} + " diffuse color 1 0 0\n")),
                  2U)
            << "name: " << name;
    }
}

TEST(SceneFileTest, acceptsIdentifierNames)
{
    EXPECT_EQ(errorLine(withCamera("material _Red-2 diffuse color 1 0 0\n"
                                   "sphere 0 0 -5 radius 1 material _Red-2\n")),
              no_error);
}

TEST(SceneFileTest, rejectsUnknownMaterialTypes)
{
    EXPECT_EQ(errorLine(withCamera("material red plastic color 1 0 0\n")), 2U);
}

TEST(SceneFileTest, rejectsMaterialValuesOutOfRange)
{
    for (const std::string_view statement :
         {"material m diffuse color -0.1 0 0",
          "material m checker even 1 1 1 odd 0 0 0 frequency 0",
          "material m checker even 1 1 1 odd 0 0 0 frequency -2",
          "material m conductor eta 0 1 1 k 1 1 1", "material m conductor eta 1 1 1 k -1 1 1",
          "material m conductor eta 1 1 1 k 1 1 1 roughness -0.5", "material m dielectric ior 0",
          "material m dielectric ior 1.5 roughness -1"}) {
        EXPECT_EQ(errorLine(withCamera(std::string{statement} + "\n")), 2U)
            << "statement: " << statement;
    }
}

TEST(SceneFileTest, rejectsValuesThatWouldBreakRendering)
{
    // Roughness beyond 1 overflows the GGX width; extreme indices make the
    // Fresnel terms non-finite; reflectances above 1 add energy.
    for (const std::string_view statement :
         {"material m conductor eta 1 1 1 k 1 1 1 roughness 1.5",
          "material m conductor eta 1 1 1 k 1 1 1 roughness 1e200",
          "material m dielectric ior 1.5 roughness 1e200", "material m dielectric ior 1e-310",
          "material m dielectric ior 0.001", "material m dielectric ior 1e300",
          "material m conductor eta 1e-310 1 1 k 1 1 1",
          "material m conductor eta 1e300 1 1 k 1 1 1",
          "material m conductor eta 1 1 1 k 1e300 1 1", "material m diffuse color 1.1 0.5 0.5",
          "material m checker even 1 1 1 odd 2 0 0 frequency 1"}) {
        EXPECT_EQ(errorLine(withCamera(std::string{statement} + "\n")), 2U)
            << "statement: " << statement;
    }
}

TEST(SceneFileTest, rejectsAConductorChannelIndistinguishableFromAir)
{
    EXPECT_EQ(errorLine(withCamera("material m conductor eta 0.5 1 0.5 k 1 0 1\n")), 2U);
}

TEST(SceneFileTest, acceptsValuesAtTheEdgesOfTheirRanges)
{
    EXPECT_EQ(errorLine(withCamera("material a conductor eta 0.001 1000 1 k 0 1000 1 roughness 1\n"
                                   "material b dielectric ior 0.01 roughness 0\n"
                                   "material c dielectric ior 100\n"
                                   "material d diffuse color 0 1 1\n"
                                   "material e checker even 1 1 1 odd 0 0 0 frequency 1\n"
                                   "sky 1e6 1e6 1e6\n"
                                   "point_light 0 0 0 intensity 1e6 0 1e6\n")),
              no_error);
}

TEST(SceneFileTest, rejectsEmittedLightAboveTheLimit)
{
    EXPECT_EQ(errorLine(withCamera("point_light 0 0 0 intensity 1e7 1 1\n")), 2U);
    EXPECT_EQ(errorLine(withCamera("sky 1 2e6 1\n")), 2U);
}

TEST(SceneFileTest, rejectsMaterialsMissingANameOrType)
{
    EXPECT_EQ(errorLine(withCamera("material\n")), 2U);
    EXPECT_EQ(errorLine(withCamera("material m\n")), 2U);
}

TEST(SceneFileTest, rejectsAFieldRepeatedInAMaterial)
{
    EXPECT_EQ(errorLine(withCamera("material m dielectric ior 1.5 ior 1.3\n")), 2U);
}

TEST(SceneFileTest, rejectsMaterialsMissingARequiredField)
{
    for (const std::string_view statement :
         {"material m diffuse", "material m checker even 1 1 1 odd 0 0 0",
          "material m conductor eta 1 1 1", "material m dielectric roughness 0"}) {
        EXPECT_EQ(errorLine(withCamera(std::string{statement} + "\n")), 2U)
            << "statement: " << statement;
    }
}

TEST(SceneFileTest, rejectsSpheresThatCannotBeBuilt)
{
    EXPECT_EQ(errorLine(withCamera("material m diffuse color 1 1 1\n"
                                   "sphere 0 0 -5 radius 0 material m\n")),
              3U);
}

TEST(SceneFileTest, rejectsSpheresMissingAField)
{
    EXPECT_EQ(errorLine(withCamera("material m diffuse color 1 1 1\nsphere 0 0 -5 radius 1\n")),
              3U);
    EXPECT_EQ(errorLine(withCamera("material m diffuse color 1 1 1\nsphere 0 0 -5 material m\n")),
              3U);
}

// --- lights

TEST(SceneFileTest, readsAPointLight)
{
    const SceneDescription description{
        readSceneText(withCamera("point_light 0.8 4.2 3.2 intensity 1.2 2 3.6\n"))};

    ASSERT_EQ(description.scene.lights().size(), 1U);
    const PointLight light{std::get<PointLight>(description.scene.lights()[0])};
    expectNear(light.position, Point3r{0.8, 4.2, 3.2}, exact_tolerance);
    expectNear(light.intensity, Rgb{1.2, 2.0, 3.6}, exact_tolerance);
}

TEST(SceneFileTest, readsAnAreaLightThatIsBlackByDefault)
{
    const SceneDescription description{
        readSceneText(withCamera("area_light 0 0 -5 radius 1 radiance 9 7.6 5.8\n"))};

    ASSERT_EQ(description.scene.lights().size(), 1U);
    EXPECT_EQ(description.scene.shapeCount(), 1U);
    expectNear(std::get<AreaLight>(description.scene.lights()[0]).radiance, Rgb{9.0, 7.6, 5.8},
               exact_tolerance);
    EXPECT_TRUE(isBlack(reflectanceAhead(description.scene)));
}

TEST(SceneFileTest, areaLightMayHaveAMaterial)
{
    const SceneDescription description{
        readSceneText(withCamera("material grey diffuse color 0.5 0.5 0.5\n"
                                 "area_light 0 0 -5 radius 1 radiance 1 1 1 material grey\n"))};

    expectNear(reflectanceAhead(description.scene), Rgb{0.5, 0.5, 0.5} * inv_pi);
}

TEST(SceneFileTest, rejectsAreaLightsMissingAFieldOrWithABadOne)
{
    for (const std::string_view statement :
         {"area_light 0 0 -5 radiance 1 1 1", "area_light 0 0 -5 radius 1",
          "area_light 0 0 -5 radius 0 radiance 1 1 1",
          "area_light 0 0 -5 radius 1 radiance 1 1 1 material missing"}) {
        EXPECT_EQ(errorLine(withCamera(std::string{statement} + "\n")), 2U)
            << "statement: " << statement;
    }
}

TEST(SceneFileTest, areaLightsCountTowardsTheObjectLimit)
{
    EXPECT_EQ(errorLine(withCamera("point_light 0 0 0 intensity 1 1 1\n"
                                   "area_light 0 0 -5 radius 1 radiance 1 1 1\n"),
                        {.max_objects = 1}),
              3U);
}

TEST(SceneFileTest, rejectsNegativeLightValues)
{
    EXPECT_EQ(errorLine(withCamera("point_light 0 0 0 intensity 1 -1 1\n")), 2U);
    EXPECT_EQ(errorLine(withCamera("area_light 0 0 -5 radius 1 radiance -1 1 1\n")), 2U);
    EXPECT_EQ(errorLine(withCamera("sky -0.1 0 0\n")), 2U);
}

// --- syntax

TEST(SceneFileTest, ignoresCommentsAndBlankLines)
{
    const SceneDescription description{
        readSceneText("# a scene\n"
                      "\n"
                      "camera from 0 0 0 at 0 0 -1  # looking down -z\n"
                      "   \t\n"
                      "point_light 0 1 0 intensity 1 1 1 # a lamp\n")};

    EXPECT_EQ(description.scene.lights().size(), 1U);
}

TEST(SceneFileTest, acceptsWindowsLineEndingsAndNoFinalNewline)
{
    EXPECT_EQ(errorLine("camera from 0 0 0 at 0 0 -1\r\npoint_light 0 1 0 intensity 1 1 1"),
              no_error);
}

TEST(SceneFileTest, rejectsUnknownStatements)
{
    EXPECT_EQ(errorLine(withCamera("cube 0 0 0\n")), 2U);
}

TEST(SceneFileTest, rejectsUnknownFields)
{
    EXPECT_EQ(errorLine(withCamera("point_light 0 0 0 intensity 1 1 1 colour 1 1 1\n")), 2U);
}

TEST(SceneFileTest, rejectsAFieldGivenTwice)
{
    EXPECT_EQ(errorLine(withCamera("point_light 0 0 0 intensity 1 1 1 intensity 2 2 2\n")), 2U);
}

TEST(SceneFileTest, rejectsMalformedAndNonFiniteNumbers)
{
    for (const std::string_view value : {"x", "1.5.2", "nan", "inf", "1e999", "0x10", "--1"}) {
        EXPECT_EQ(
            errorLine(withCamera("point_light 0 " + std::string{value} + " 0 intensity 1 1 1\n")),
            2U)
            << "value: " << value;
    }
}

TEST(SceneFileTest, rejectsStatementsCutShort)
{
    EXPECT_EQ(errorLine(withCamera("point_light 0 0\n")), 2U);
    EXPECT_EQ(errorLine(withCamera("point_light 0 0 0 intensity 1 1\n")), 2U);
}

TEST(SceneFileTest, errorsNameTheCoordinateAtFault)
{
    const std::string message{errorMessage("camera from 0 x 0 at 0 0 -1\n")};

    EXPECT_NE(message.find("the y coordinate of the camera position"), std::string::npos)
        << message;
}

TEST(SceneFileTest, errorsQuoteInputSafely)
{
    // An escape sequence and a long word: the message must escape the control
    // byte and cut the word short.
    const std::string message{
        errorMessage(withCamera("\x1B[2Jcube" + std::string(200, 'x') + "\n"))};

    EXPECT_EQ(message.find('\x1B'), std::string::npos) << message;
    EXPECT_NE(message.find("\\x1B[2Jcube"), std::string::npos) << message;
    EXPECT_LT(message.size(), 100U) << message;
    EXPECT_NE(message.find("'..."), std::string::npos) << message;
}

TEST(SceneFileTest, errorsNameTheLine)
{
    const std::string message{errorMessage(withCamera("cube 0 0 0\n"))};

    EXPECT_NE(message.find("scene line 2"), std::string::npos) << message;
    EXPECT_NE(message.find("cube"), std::string::npos) << message;
}

// --- limits

TEST(SceneFileTest, rejectsLinesOverTheLengthLimit)
{
    EXPECT_EQ(errorLine(withCamera("point_light 0 0 0 intensity 1 1 1\n"), {.max_line_length = 30}),
              2U);
}

TEST(SceneFileTest, acceptsObjectsUpToTheLimit)
{
    EXPECT_EQ(errorLine(withCamera("point_light 0 0 0 intensity 1 1 1\n"
                                   "point_light 1 0 0 intensity 1 1 1\n"),
                        {.max_objects = 2}),
              no_error);
}

TEST(SceneFileTest, rejectsMoreObjectsThanTheLimit)
{
    EXPECT_EQ(errorLine(withCamera("material m diffuse color 1 1 1\n"
                                   "point_light 0 0 0 intensity 1 1 1\n"
                                   "sphere 0 0 -5 radius 1 material m\n"),
                        {.max_objects = 1}),
              4U);
}

TEST(SceneFileTest, acceptsMaterialsUpToTheLimit)
{
    EXPECT_EQ(errorLine(withCamera("material a diffuse color 1 1 1\n"
                                   "material b diffuse color 1 1 1\n"),
                        {.max_materials = 2}),
              no_error);
}

TEST(SceneFileTest, rejectsMoreMaterialsThanTheLimit)
{
    EXPECT_EQ(errorLine(withCamera("material a diffuse color 1 1 1\n"
                                   "material b diffuse color 1 1 1\n"),
                        {.max_materials = 1}),
              3U);
}

// --- files

TEST(SceneFileTest, readsAFile)
{
    const TemporaryFile file{"albedo_scene_file_test_reads.scene"};
    std::ofstream{file.path()} << camera_line << "point_light 0 1 0 intensity 1 1 1\n";

    const SceneDescription description{readScene(file.path())};

    EXPECT_EQ(description.scene.lights().size(), 1U);
}

TEST(SceneFileTest, missingFileThrowsNamingThePath)
{
    try {
        static_cast<void>(readScene(std::filesystem::path{"albedo_no_such_scene_file.scene"}));
        FAIL() << "expected std::runtime_error";
    } catch (const SceneParseError&) {
        FAIL() << "a missing file is not a parse error";
    } catch (const std::runtime_error& error) {
        EXPECT_NE(std::string_view{error.what()}.find("albedo_no_such_scene_file.scene"),
                  std::string_view::npos);
    }
}

// --- meshes and instances

// A 2 x 2 square in the plane z = 0, facing +z, with texture coordinates
// spanning the unit square.
constexpr std::string_view square_obj{"v -1 -1 0\nv 1 -1 0\nv 1 1 0\nv -1 1 0\n"
                                      "vt 0 0\nvt 1 0\nvt 1 1\nvt 0 1\n"
                                      "f 1/1 2/2 3/3 4/4\n"};

// The camera, a red material and the square as mesh `sq`, then `rest`, so
// the statements under test start on line 4.
std::string withSquare(std::string_view rest)
{
    return withCamera("material red diffuse color 0.6 0.1 0.1\nmesh sq square.obj\n") +
           std::string{rest};
}

// A directory holding square.obj, unique to the calling test.
class SquareDirectory {
public:
    explicit SquareDirectory(std::string_view test_name)
        : directory{std::string{"albedo_scene_file_test_"} + std::string{test_name}}
    {
        static_cast<void>(directory.write("square.obj", square_obj));
    }

    [[nodiscard]] const TemporaryDirectory& get() const noexcept { return directory; }

    [[nodiscard]] SceneDescription read(std::string_view text, const SceneLimits& limits = {}) const
    {
        std::istringstream input{std::string{text}};
        return readScene(input, directory.path(), limits);
    }

    [[nodiscard]] std::size_t errorLine(std::string_view text, const SceneLimits& limits = {}) const
    {
        try {
            static_cast<void>(read(text, limits));
        } catch (const SceneParseError& error) {
            return error.line();
        }
        return no_error;
    }

    [[nodiscard]] std::string errorMessage(std::string_view text) const
    {
        try {
            static_cast<void>(read(text));
        } catch (const SceneParseError& error) {
            return error.what();
        }
        return {};
    }

private:
    TemporaryDirectory directory;
};

// The t at which `ray` first hits `scene`, if it does.
std::optional<Real> hitDistance(const Scene& scene, const Ray& ray)
{
    const auto hit{scene.intersect(ray, positive_t)};
    return hit ? std::optional<Real>{hit->t} : std::nullopt;
}

TEST(SceneFileTest, meshStatementsNeedABaseDirectory)
{
    EXPECT_EQ(errorLine(withCamera("mesh sq square.obj\n")), 2U);
}

TEST(SceneFileTest, readsAMeshAndPlacesAnInstanceWithItsMaterial)
{
    const SquareDirectory directory{"places_instance"};

    const SceneDescription description{
        directory.read(withSquare("instance sq material red translate 0 0 -5\n"))};

    EXPECT_EQ(description.scene.shapeCount(), 1U);
    EXPECT_NEAR(requireValue(hitDistance(description.scene, forward_ray)), 5.0, default_tolerance);
    expectNear(reflectanceAhead(description.scene), Rgb{0.6, 0.1, 0.1} * inv_pi);
}

TEST(SceneFileTest, instancesKeepTheMeshTextureCoordinates)
{
    const SquareDirectory directory{"texture_coordinates"};
    const SceneDescription description{
        directory.read(withSquare("instance sq material red translate 0 0 -5\n"))};
    const Ray ray{.origin = {0.5, -0.5, 0.0}, .direction = {0.0, 0.0, -1.0}};

    const HitRecord hit{requireValue(description.scene.intersect(ray, positive_t))};

    expectNear(hit.uv, Point2r{0.75, 0.25});
}

TEST(SceneFileTest, instanceOperationsApplyInTheOrderWritten)
{
    const SquareDirectory directory{"operation_order"};

    const SceneDescription scaled_then_moved{
        directory.read(withSquare("instance sq material red scale 2 translate 0 0 -5\n"))};
    const SceneDescription moved_then_scaled{
        directory.read(withSquare("instance sq material red translate 0 0 -5 scale 2\n"))};

    EXPECT_NEAR(requireValue(hitDistance(scaled_then_moved.scene, forward_ray)), 5.0,
                default_tolerance);
    EXPECT_NEAR(requireValue(hitDistance(moved_then_scaled.scene, forward_ray)), 10.0,
                default_tolerance);
}

TEST(SceneFileTest, scaleTakesOneOrThreeFactors)
{
    const SquareDirectory directory{"scale_factors"};
    const SceneDescription description{
        directory.read(withSquare("instance sq material red scale 2 1 1 translate 0 0 -5\n"))};
    const Ray inside_along_x{.origin = {1.5, 0.0, 0.0}, .direction = {0.0, 0.0, -1.0}};
    const Ray outside_along_y{.origin = {0.0, 1.5, 0.0}, .direction = {0.0, 0.0, -1.0}};

    EXPECT_TRUE(hitDistance(description.scene, inside_along_x).has_value());
    EXPECT_FALSE(hitDistance(description.scene, outside_along_y).has_value());
}

TEST(SceneFileTest, rotateTurnsAboutAnAxisInDegrees)
{
    // A quarter turn about +y takes the square's normal from +z to +x.
    const SquareDirectory directory{"rotate"};
    const SceneDescription description{
        directory.read(withSquare("instance sq material red rotate 0 1 0 90 translate -5 0 0\n"))};
    const Ray towards_minus_x{.origin = {0.0, 0.0, 0.0}, .direction = {-1.0, 0.0, 0.0}};

    EXPECT_NEAR(requireValue(hitDistance(description.scene, towards_minus_x)), 5.0,
                default_tolerance);
}

TEST(SceneFileTest, fitCentresAndScalesTheInstanceAsPlacedSoFar)
{
    // Wherever it was moved first, fit 4 centres the square and makes it 4 wide.
    const SquareDirectory directory{"fit"};
    const SceneDescription description{directory.read(
        withSquare("instance sq material red translate 10 10 10 fit 4 translate 0 0 -5\n"))};
    const Ray inside_after_fit{.origin = {1.5, 1.5, 0.0}, .direction = {0.0, 0.0, -1.0}};

    EXPECT_NEAR(requireValue(hitDistance(description.scene, inside_after_fit)), 5.0,
                default_tolerance);
}

TEST(SceneFileTest, groundStandsTheInstanceOnTheFloor)
{
    // Laid flat at y = -3, then lifted so its lowest corner is at y = 0.
    const SquareDirectory directory{"ground"};
    const SceneDescription description{directory.read(
        withSquare("instance sq material red rotate 1 0 0 90 translate 0 -3 0 ground\n"))};
    const Ray down{.origin = {0.5, 5.0, 0.5}, .direction = {0.0, -1.0, 0.0}};

    EXPECT_NEAR(requireValue(hitDistance(description.scene, down)), 5.0, default_tolerance);
}

TEST(SceneFileTest, instancesOfOneMeshShareItsGeometry)
{
    const SquareDirectory directory{"shared"};
    const SceneDescription description{
        directory.read(withSquare("instance sq material red translate 0 0 -5\n"
                                  "instance sq material red translate 0 0 -8\n"))};

    EXPECT_EQ(description.scene.shapeCount(), 2U);
}

TEST(SceneFileTest, readsMeshesFromSubdirectories)
{
    const SquareDirectory directory{"subdirectory"};
    static_cast<void>(directory.get().write("meshes/also_square.obj", square_obj));

    EXPECT_EQ(directory.errorLine(withCamera("mesh sq meshes/also_square.obj\n")), no_error);
}

TEST(SceneFileTest, rejectsMeshPathsLeavingTheSceneDirectory)
{
    const SquareDirectory directory{"escaping_paths"};
    static_cast<void>(directory.get().write("sub/square.obj", square_obj));
    const std::string absolute{(directory.get().path() / "square.obj").generic_string()};

    for (const std::string& path :
         {std::string{"../square.obj"}, std::string{"sub/../square.obj"},
          std::string{"./../square.obj"}, std::string{"/square.obj"}, absolute}) {
        const std::string message{directory.errorMessage(withCamera("mesh sq " + path + "\n"))};
        EXPECT_NE(message.find("scene line 2"), std::string::npos) << message;
        EXPECT_NE(message.find("must be relative"), std::string::npos) << message;
    }
}

TEST(SceneFileTest, rejectsMeshPathsWithControlCharactersOrInvalidUtf8)
{
    const SquareDirectory directory{"bad_bytes"};
    // A control byte, a byte that never starts UTF-8, an overlong '/', a lead
    // byte cut short, a surrogate, a three-byte overlong form and a code point
    // past U+10FFFF.
    for (const std::string_view path :
         {"sq\x01.obj", "\xFF.obj", "\xC0\xAF.obj", "a\xE2\x82", "\xED\xA0\x80.obj",
          "\xE0\x80\xAF.obj", "\xF4\x90\x80\x80.obj"}) {
        const std::string message{
            directory.errorMessage(withCamera("mesh sq " + std::string{path} + "\n"))};
        EXPECT_NE(message.find("must be UTF-8"), std::string::npos) << message;
    }
}

TEST(SceneFileTest, acceptsMeshPathsInUtf8)
{
    const SquareDirectory directory{"utf8_path"};
    static_cast<void>(directory.get().write(std::filesystem::path{u8"caf\u00e9.obj"}, square_obj));

    EXPECT_EQ(directory.errorLine(withCamera("mesh sq caf\xC3\xA9.obj\n")), no_error);
}

TEST(SceneFileTest, rejectsMeshPathsNamingWindowsDevices)
{
    const SquareDirectory directory{"device_names"};
    for (const std::string_view path : {"nul", "CON", "sub/aux.obj", "com1.obj", "Lpt9",
                                        "nul.tar.obj", "com1.a.obj", "nul:x", "COM\xC2\xB9"}) {
        const std::string message{
            directory.errorMessage(withCamera("mesh sq " + std::string{path} + "\n"))};
        EXPECT_NE(message.find("names a device"), std::string::npos) << message;
    }
}

TEST(SceneFileTest, acceptsNamesThatOnlyResembleDevices)
{
    const SquareDirectory directory{"device_lookalikes"};
    static_cast<void>(directory.get().write("console.obj", square_obj));
    static_cast<void>(directory.get().write("com10.obj", square_obj));

    EXPECT_EQ(directory.errorLine(withCamera("mesh a console.obj\nmesh b com10.obj\n")), no_error);
}

TEST(SceneFileTest, rejectsMeshPathsThatAreNotRegularFiles)
{
    const SquareDirectory directory{"not_regular"};
    std::filesystem::create_directories(directory.get().path() / "folder.obj");

    const std::string message{directory.errorMessage(withCamera("mesh sq folder.obj\n"))};

    EXPECT_NE(message.find("not a regular file"), std::string::npos) << message;
}

TEST(SceneFileTest, emptyBaseDirectoryMeansTheCurrentDirectory)
{
    const SquareDirectory directory{"empty_base"};
    const CurrentDirectory inside{directory.get().path()};
    std::istringstream input{withCamera("mesh sq square.obj\n")};

    EXPECT_NO_THROW(static_cast<void>(readScene(input, std::filesystem::path{})));
}

TEST(SceneFileTest, emptyBaseDirectoryStillStopsLinksLeavingIt)
{
    const SquareDirectory directory{"empty_base_link"};
    const TemporaryDirectory outside{"albedo_scene_file_test_empty_base_link_target"};
    static_cast<void>(outside.write("square.obj", square_obj));
    std::error_code error;
    std::filesystem::create_directory_symlink(outside.path(), directory.get().path() / "linked",
                                              error);
    if (error) {
        GTEST_SKIP() << "cannot create symbolic links here: " << error.message();
    }
    const CurrentDirectory inside{directory.get().path()};
    std::istringstream input{withCamera("mesh sq linked/square.obj\n")};

    try {
        static_cast<void>(readScene(input, std::filesystem::path{}));
        FAIL() << "expected a SceneParseError";
    } catch (const SceneParseError& error) {
        EXPECT_NE(std::string_view{error.what()}.find("leads outside"), std::string_view::npos)
            << error.what();
    }
}

TEST(SceneFileTest, sceneFileNamedWithoutADirectoryFindsMeshesBesideIt)
{
    const SquareDirectory directory{"bare_scene_name"};
    static_cast<void>(directory.get().write(
        "room.scene", withSquare("instance sq material red translate 0 0 -5\n")));
    const CurrentDirectory inside{directory.get().path()};

    EXPECT_EQ(readScene(std::filesystem::path{"room.scene"}).scene.shapeCount(), 1U);
}

TEST(SceneFileTest, rejectsMeshPathsLeavingTheSceneDirectoryThroughALink)
{
    const SquareDirectory directory{"escaping_link"};
    const TemporaryDirectory outside{"albedo_scene_file_test_escaping_link_target"};
    static_cast<void>(outside.write("square.obj", square_obj));
    std::error_code error;
    std::filesystem::create_directory_symlink(outside.path(), directory.get().path() / "linked",
                                              error);
    if (error) {
        GTEST_SKIP() << "cannot create symbolic links here: " << error.message();
    }

    const std::string message{directory.errorMessage(withCamera("mesh sq linked/square.obj\n"))};

    EXPECT_NE(message.find("leads outside"), std::string::npos) << message;
}

TEST(SceneFileTest, rejectsAMissingMeshFile)
{
    const SquareDirectory directory{"missing_mesh"};

    EXPECT_EQ(directory.errorLine(withCamera("mesh sq no_such.obj\n")), 2U);
    EXPECT_NE(directory.errorMessage(withCamera("mesh sq no_such.obj\n")).find("does not exist"),
              std::string::npos);
}

TEST(SceneFileTest, reportsMeshFileErrorsAtTheMeshStatement)
{
    const SquareDirectory directory{"broken_mesh"};
    static_cast<void>(directory.get().write("broken.obj", "v 0 0 0\nf 1 2 3\n"));

    const std::string message{directory.errorMessage(withCamera("mesh b broken.obj\n"))};

    EXPECT_NE(message.find("scene line 2"), std::string::npos) << message;
    EXPECT_NE(message.find("OBJ line 2"), std::string::npos) << message;
}

TEST(SceneFileTest, rejectsAMeshWithoutTriangles)
{
    const SquareDirectory directory{"empty_mesh"};
    static_cast<void>(directory.get().write("empty.obj", "v 0 0 0\nv 1 0 0\nv 2 0 0\nf 1 2 3\n"));

    EXPECT_EQ(directory.errorLine(withCamera("mesh e empty.obj\n")), 2U);
}

TEST(SceneFileTest, rejectsAMeshNamedTwice)
{
    const SquareDirectory directory{"mesh_twice"};

    EXPECT_EQ(directory.errorLine(withSquare("mesh sq square.obj\n")), 4U);
}

TEST(SceneFileTest, rejectsInstancesOfUndefinedMeshes)
{
    const SquareDirectory directory{"undefined_mesh"};

    EXPECT_EQ(directory.errorLine(withSquare("instance other material red\n")), 4U);
}

TEST(SceneFileTest, rejectsInstancesWithoutOneMaterial)
{
    const SquareDirectory directory{"instance_material"};

    EXPECT_EQ(directory.errorLine(withSquare("instance sq translate 0 0 -5\n")), 4U);
    EXPECT_EQ(directory.errorLine(withSquare("instance sq material red material red\n")), 4U);
    EXPECT_EQ(directory.errorLine(withSquare("instance sq material blue\n")), 4U);
}

TEST(SceneFileTest, rejectsUnknownAndInvalidInstanceOperations)
{
    const SquareDirectory directory{"bad_operations"};
    for (const std::string_view operations :
         {"shear 1 2", "scale 0", "scale 1 0 1", "scale 2 2", "rotate 0 0 0 30", "rotate 0 1 0",
          "fit 0", "fit -1", "translate 0 0", "scale 1e300 scale 1e300", "ground 1"}) {
        EXPECT_EQ(directory.errorLine(
                      withSquare("instance sq material red " + std::string{operations} + "\n")),
                  4U)
            << "operations: " << operations;
    }
}

TEST(SceneFileTest, limitsTheOperationsPerInstance)
{
    const SquareDirectory directory{"operation_limit"};
    std::string operations;
    for (int count{0}; count < 16; ++count) {
        operations += " ground";
    }

    EXPECT_EQ(directory.errorLine(withSquare("instance sq material red" + operations + "\n")),
              no_error);
    EXPECT_EQ(
        directory.errorLine(withSquare("instance sq material red" + operations + " ground\n")), 4U);
}

TEST(SceneFileTest, instancesCountTowardsTheObjectLimit)
{
    const SquareDirectory directory{"instance_limit"};

    EXPECT_EQ(
        directory.errorLine(withSquare("instance sq material red\ninstance sq material red\n"),
                            {.max_objects = 1}),
        5U);
}

TEST(SceneFileTest, limitsTheNumberOfMeshes)
{
    const SquareDirectory directory{"mesh_limit"};

    EXPECT_EQ(directory.errorLine(withCamera("mesh a square.obj\nmesh b square.obj\n"),
                                  {.max_meshes = 1}),
              3U);
}

TEST(SceneFileTest, meshLimitsBoundAllMeshesTogether)
{
    // Each square has two triangles: two of them fit a budget of four, three do not.
    const SquareDirectory directory{"mesh_budget"};
    const SceneLimits limits{.mesh_limits = {.max_triangles = 4}};

    EXPECT_EQ(directory.errorLine(withCamera("mesh a square.obj\nmesh b square.obj\n"), limits),
              no_error);
    EXPECT_EQ(directory.errorLine(
                  withCamera("mesh a square.obj\nmesh b square.obj\nmesh c square.obj\n"), limits),
              4U);
}

TEST(SceneFileTest, meshLimitsBoundVerticesOfAllMeshesTogether)
{
    // Each square has four vertices and four texture coordinates.
    const SquareDirectory directory{"vertex_budget"};
    const std::string two{withCamera("mesh a square.obj\nmesh b square.obj\n")};
    const std::string three{two + "mesh c square.obj\n"};

    EXPECT_EQ(directory.errorLine(two, {.mesh_limits = {.max_vertices = 8}}), no_error);
    EXPECT_EQ(directory.errorLine(three, {.mesh_limits = {.max_vertices = 8}}), 4U);
    EXPECT_EQ(directory.errorLine(two, {.mesh_limits = {.max_texture_coordinates = 8}}), no_error);
    EXPECT_EQ(directory.errorLine(three, {.mesh_limits = {.max_texture_coordinates = 8}}), 4U);
}

TEST(SceneFileTest, groundOperationsShareOneBudgetOfPoints)
{
    // The square's triangles use four points: two grounds fit eight, a third does not.
    const SquareDirectory directory{"ground_budget"};
    const SceneLimits limits{.max_ground_points = 8};

    EXPECT_EQ(directory.errorLine(withSquare("instance sq material red ground\n"
                                             "instance sq material red ground\n"),
                                  limits),
              no_error);
    EXPECT_EQ(directory.errorLine(withSquare("instance sq material red ground ground\n"
                                             "instance sq material red ground\n"),
                                  limits),
              5U);
}

TEST(SceneFileTest, uniformScaleMayBeFollowedByTheMaterial)
{
    const SquareDirectory directory{"scale_then_material"};

    EXPECT_EQ(directory.errorLine(withSquare("instance sq scale 2 material red\n")), no_error);
}

TEST(SceneFileTest, malformedSecondScaleFactorIsReportedAsSuch)
{
    const SquareDirectory directory{"scale_message"};

    const std::string message{
        directory.errorMessage(withSquare("instance sq material red scale 2 inf 1\n"))};

    EXPECT_NE(message.find("y scale factor"), std::string::npos) << message;
}

TEST(SceneFileTest, sceneFilesFindMeshesNextToThem)
{
    const SquareDirectory directory{"file_relative"};
    const std::filesystem::path scene{directory.get().write(
        "room.scene", withSquare("instance sq material red translate 0 0 -5\n"))};

    EXPECT_EQ(readScene(scene).scene.shapeCount(), 1U);
}

} // namespace
} // namespace albedo
