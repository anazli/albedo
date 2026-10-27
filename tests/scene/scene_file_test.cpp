#include "albedo/scene/scene_file.hpp"

#include "albedo/light/light.hpp"
#include "support/expect_near.hpp"
#include "support/require_value.hpp"
#include "support/temporary_file.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <variant>

namespace albedo {
namespace {

using test_support::default_tolerance;
using test_support::exact_tolerance;
using test_support::expectNear;
using test_support::requireValue;
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

} // namespace
} // namespace albedo
