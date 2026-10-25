#include "albedo/scene/scene.hpp"

#include "albedo/geometry/triangle_mesh.hpp"
#include "albedo/material/material.hpp"
#include "support/expect_near.hpp"
#include "support/require_value.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <limits>
#include <optional>
#include <variant>

namespace albedo {
namespace {

using test_support::default_tolerance;
using test_support::expectNear;
using test_support::requireValue;

// Any material will do where only geometry matters.
const Material matte{DiffuseMaterial{.albedo = {0.5, 0.5, 0.5}}};

constexpr Interval positive_t{.min = 0.0, .max = std::numeric_limits<Real>::infinity()};

// Looking down -z from the origin.
constexpr Ray forward_ray{.origin = {0.0, 0.0, 0.0}, .direction = {0.0, 0.0, -1.0}};

TEST(SceneTest, newSceneIsEmpty)
{
    const Scene scene;

    EXPECT_EQ(scene.shapeCount(), 0U);
}

TEST(SceneTest, countsAddedShapes)
{
    Scene scene;

    scene.add(Sphere{Point3r{0.0, 0.0, -5.0}, 1.0}, matte);
    scene.add(Sphere{Point3r{0.0, 0.0, -9.0}, 1.0}, matte);

    EXPECT_EQ(scene.shapeCount(), 2U);
}

TEST(SceneTest, emptySceneHasNoHits)
{
    const Scene scene;

    EXPECT_FALSE(scene.intersect(forward_ray, positive_t).has_value());
}

TEST(SceneTest, reportsNearestOfSeveralShapes)
{
    Scene scene;
    scene.add(Sphere{Point3r{0.0, 0.0, -9.0}, 1.0}, matte);
    scene.add(Sphere{Point3r{0.0, 0.0, -5.0}, 1.0}, matte);
    scene.add(Sphere{Point3r{0.0, 0.0, -20.0}, 1.0}, matte);

    const HitRecord hit{requireValue(scene.intersect(forward_ray, positive_t))};

    EXPECT_NEAR(hit.t, 4.0, default_tolerance);
    expectNear(hit.point, Point3r{0.0, 0.0, -4.0});
}

TEST(SceneTest, respectsTheParameterInterval)
{
    Scene scene;
    scene.add(Sphere{Point3r{0.0, 0.0, -5.0}, 1.0}, matte);

    EXPECT_FALSE(scene.intersect(forward_ray, Interval{.min = 0.0, .max = 3.0}).has_value());
}

TEST(SceneTest, missesWhenNoShapeIsInTheWay)
{
    Scene scene;
    scene.add(Sphere{Point3r{5.0, 0.0, -5.0}, 1.0}, matte);

    EXPECT_FALSE(scene.intersect(forward_ray, positive_t).has_value());
}

TEST(SceneTest, hitFromOutsideIsFrontFaceWithOutwardNormal)
{
    Scene scene;
    scene.add(Sphere{Point3r{0.0, 0.0, -5.0}, 1.0}, matte);

    const HitRecord hit{requireValue(scene.intersect(forward_ray, positive_t))};

    EXPECT_TRUE(hit.front_face);
    expectNear(hit.normal, Normal3r{0.0, 0.0, 1.0});
}

TEST(SceneTest, hitFromInsideIsBackFaceWithNormalFacingTheRay)
{
    Scene scene;
    scene.add(Sphere{Point3r{0.0, 0.0, 0.0}, 2.0}, matte);

    const HitRecord hit{requireValue(scene.intersect(forward_ray, positive_t))};

    EXPECT_FALSE(hit.front_face);
    // The outward normal is -z there; flipped, it faces back against the ray.
    expectNear(hit.normal, Normal3r{0.0, 0.0, 1.0});
}

TEST(SceneTest, normalAlwaysOpposesTheRayDirection)
{
    Scene scene;
    scene.add(Sphere{Point3r{0.0, 0.0, -5.0}, 1.0}, matte);
    const Ray inside_ray{.origin = {0.0, 0.0, -5.0}, .direction = {0.3, -0.4, 0.5}};

    const HitRecord outside_hit{requireValue(scene.intersect(forward_ray, positive_t))};
    const HitRecord inside_hit{requireValue(scene.intersect(inside_ray, positive_t))};

    EXPECT_LT(dot(forward_ray.direction, outside_hit.normal), 0.0);
    EXPECT_LT(dot(inside_ray.direction, inside_hit.normal), 0.0);
}

TEST(SceneTest, hitRecordsWhichObjectWasHit)
{
    // The nearest object is neither first nor last, so recording the last one
    // tested or the first one hit would both show.
    Scene scene;
    scene.add(Sphere{Point3r{0.0, 0.0, -9.0}, 1.0}, matte);
    scene.add(Sphere{Point3r{0.0, 0.0, -5.0}, 1.0}, matte);
    scene.add(Sphere{Point3r{0.0, 0.0, -20.0}, 1.0}, matte);

    const HitRecord hit{requireValue(scene.intersect(forward_ray, positive_t))};

    EXPECT_EQ(hit.object, 1U);
}

TEST(SceneTest, scatteringAtUsesTheMaterialOfTheObjectHit)
{
    constexpr Rgb near_albedo{0.1, 0.6, 0.3};
    Scene scene;
    scene.add(Sphere{Point3r{0.0, 0.0, -9.0}, 1.0}, matte);
    scene.add(Sphere{Point3r{0.0, 0.0, -5.0}, 1.0},
              Material{DiffuseMaterial{.albedo = near_albedo}});
    const HitRecord hit{requireValue(scene.intersect(forward_ray, positive_t))};
    const Vector3r back_towards_origin{0.0, 0.0, 1.0};

    const Scattering scattering{scene.scatteringAt(hit)};

    expectNear(scattering.evaluate(back_towards_origin, back_towards_origin), near_albedo * inv_pi);
}

TEST(SceneTest, skyIsBlackByDefault)
{
    const Scene scene;

    EXPECT_TRUE(isBlack(scene.sky().emittedRadiance({0.0, 1.0, 0.0})));
}

TEST(SceneTest, setSkyReplacesTheSky)
{
    constexpr Rgb radiance{0.4, 0.5, 0.6};
    Scene scene;

    scene.setSky(SkyLight::uniform(radiance));

    expectNear(scene.sky().emittedRadiance({0.0, 1.0, 0.0}), radiance);
}

TEST(SceneDeathTest, scatteringAtRejectsAHitFromAnotherScene)
{
    const Scene scene;
    const HitRecord foreign_hit{.t = 1.0,
                                .point = {0.0, 0.0, -1.0},
                                .normal = {0.0, 0.0, 1.0},
                                .front_face = true,
                                .object = 3};

    EXPECT_DEBUG_DEATH(static_cast<void>(scene.scatteringAt(foreign_hit)), "");
}

TEST(SceneTest, newSceneHasNoLights)
{
    const Scene scene;

    EXPECT_TRUE(scene.lights().empty());
}

TEST(SceneTest, addLightAppendsItAndReturnsItsIndex)
{
    Scene scene;

    const std::size_t first{scene.addLight(PointLight{.position = {0.0, 1.0, 0.0}})};
    const std::size_t second{scene.addLight(PointLight{.position = {0.0, 2.0, 0.0}})};

    EXPECT_EQ(first, 0U);
    EXPECT_EQ(second, 1U);
    EXPECT_EQ(scene.lights().size(), 2U);
}

TEST(SceneTest, settingTheSkyAddsItAsOneLight)
{
    // Setting it again replaces it rather than adding a second sky.
    Scene scene;

    scene.setSky(SkyLight::uniform({0.1, 0.1, 0.1}));
    scene.setSky(SkyLight::uniform({0.4, 0.5, 0.6}));

    ASSERT_EQ(scene.lights().size(), 1U);
    ASSERT_TRUE(std::holds_alternative<SkyLight>(scene.lights()[0]));
    expectNear(scene.sky().emittedRadiance({0.0, 1.0, 0.0}), Rgb{0.4, 0.5, 0.6});
}

TEST(SceneTest, addAreaLightAddsBothAnObjectAndALight)
{
    Scene scene;

    scene.addAreaLight(Sphere{Point3r{0.0, 0.0, -5.0}, 1.0}, {2.0, 2.0, 2.0}, matte);

    EXPECT_EQ(scene.shapeCount(), 1U);
    ASSERT_EQ(scene.lights().size(), 1U);
    EXPECT_TRUE(std::holds_alternative<AreaLight>(scene.lights()[0]));
}

TEST(SceneTest, hitOnAnAreaLightSeesItsRadianceAndIndex)
{
    Scene scene;
    scene.addLight(PointLight{.position = {0.0, 9.0, 0.0}});
    scene.addAreaLight(Sphere{Point3r{0.0, 0.0, -5.0}, 1.0}, {2.0, 3.0, 4.0}, matte);

    const HitRecord hit{requireValue(scene.intersect(forward_ray, positive_t))};

    expectNear(scene.emittedRadiance(hit), Rgb{2.0, 3.0, 4.0});
    EXPECT_EQ(scene.lightIndexOf(hit), std::optional<std::size_t>{1});
}

TEST(SceneTest, ordinaryObjectsEmitNothing)
{
    Scene scene;
    scene.add(Sphere{Point3r{0.0, 0.0, -5.0}, 1.0}, matte);

    const HitRecord hit{requireValue(scene.intersect(forward_ray, positive_t))};

    EXPECT_TRUE(isBlack(scene.emittedRadiance(hit)));
    EXPECT_FALSE(scene.lightIndexOf(hit).has_value());
}

TEST(SceneTest, areaLightSeenFromInsideEmitsNothing)
{
    Scene scene;
    scene.addAreaLight(Sphere{Point3r{0.0, 0.0, 0.0}, 2.0}, {2.0, 3.0, 4.0}, matte);

    const HitRecord hit{requireValue(scene.intersect(forward_ray, positive_t))};

    EXPECT_TRUE(isBlack(scene.emittedRadiance(hit)));
}

TEST(SceneTest, occludedFindsABlockerInsideTheRange)
{
    Scene scene;
    scene.add(Sphere{Point3r{0.0, 0.0, -5.0}, 1.0}, matte);

    EXPECT_TRUE(scene.occluded(forward_ray, {.min = 0.0, .max = 10.0}));
}

TEST(SceneTest, occludedIgnoresBlockersBeyondTheRange)
{
    Scene scene;
    scene.add(Sphere{Point3r{0.0, 0.0, -5.0}, 1.0}, matte);

    EXPECT_FALSE(scene.occluded(forward_ray, {.min = 0.0, .max = 3.0}));
}

TEST(SceneDeathTest, lightIndexOfRejectsAHitFromAnotherScene)
{
#ifdef NDEBUG
    // Without the assert the call would read past the end of the object list.
    GTEST_SKIP() << "precondition asserts are compiled out";
#endif
    const Scene scene;
    const HitRecord foreign_hit{.t = 1.0,
                                .point = {0.0, 0.0, -1.0},
                                .normal = {0.0, 0.0, 1.0},
                                .front_face = true,
                                .object = 3};

    EXPECT_DEATH(static_cast<void>(scene.lightIndexOf(foreign_hit)), "");
}

TEST(SceneTest, settingTheSkyAfterOtherLightsReplacesItInPlace)
{
    Scene scene;
    scene.addLight(PointLight{.position = {0.0, 1.0, 0.0}});

    scene.setSky(SkyLight::uniform({0.1, 0.1, 0.1}));
    scene.setSky(SkyLight::uniform({0.4, 0.5, 0.6}));

    ASSERT_EQ(scene.lights().size(), 2U);
    EXPECT_TRUE(std::holds_alternative<PointLight>(scene.lights()[0]));
    EXPECT_TRUE(std::holds_alternative<SkyLight>(scene.lights()[1]));
    expectNear(scene.sky().emittedRadiance({0.0, 1.0, 0.0}), Rgb{0.4, 0.5, 0.6});
}

template <typename LightType>
concept CanAddLight = requires(Scene scene, LightType light) { scene.addLight(light); };

TEST(SceneTest, addLightTakesOnlyPointLights)
{
    // Area lights and the sky must go through addAreaLight and setSky, which keep
    // them linked to what rays can hit.
    static_assert(CanAddLight<PointLight>);
    static_assert(!CanAddLight<AreaLight>);
    static_assert(!CanAddLight<SkyLight>);
}

// Two triangles forming the square [-1, 1]^2 in the plane z = -5, facing the origin,
// plus a face without area.
TriangleMesh squareMesh()
{
    return TriangleMesh{
        {{-1.0, -1.0, -5.0}, {1.0, -1.0, -5.0}, {1.0, 1.0, -5.0}, {-1.0, 1.0, -5.0}},
        {{0, 1, 2}, {0, 2, 3}, {0, 0, 1}}};
}

TEST(SceneTest, addingAMeshAddsOneObjectPerTriangleWithArea)
{
    Scene scene;

    scene.add(squareMesh(), matte);

    EXPECT_EQ(scene.shapeCount(), 2U);
}

TEST(SceneTest, addingAnEmptyOrDegenerateMeshAddsNothing)
{
    Scene scene;
    const TriangleMesh degenerate{{{0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}}, {{0, 0, 1}, {1, 1, 1}}};

    scene.add(TriangleMesh{{}, {}}, matte);
    scene.add(degenerate, matte);

    EXPECT_EQ(scene.shapeCount(), 0U);
    EXPECT_TRUE(scene.lights().empty());
}

TEST(SceneTest, addingManyMeshesKeepsEveryTriangle)
{
    // Guards against storage that grows on every add rather than only when full.
    const TriangleMesh one_triangle{{{0.0, 0.0, -5.0}, {1.0, 0.0, -5.0}, {0.0, 1.0, -5.0}},
                                    {{0, 1, 2}}};
    constexpr std::size_t mesh_count{100};
    Scene scene;

    for (std::size_t index{0}; index < mesh_count; ++index) {
        scene.add(one_triangle, matte);
    }

    EXPECT_EQ(scene.shapeCount(), mesh_count);
}

TEST(SceneTest, raysHitAMeshTriangleAndRecordWhichOne)
{
    Scene scene;
    scene.add(Sphere{Point3r{0.0, 0.0, -20.0}, 1.0}, matte);
    scene.add(squareMesh(), matte);
    // Below the diagonal through (-1, -1) and (1, 1): the mesh's first triangle.
    const Ray ray{.origin = {0.5, -0.5, 0.0}, .direction = {0.0, 0.0, -1.0}};

    const HitRecord hit{requireValue(scene.intersect(ray, positive_t))};

    EXPECT_NEAR(hit.t, 5.0, default_tolerance);
    EXPECT_TRUE(hit.front_face);
    EXPECT_EQ(hit.object, 1U);
}

TEST(SceneTest, meshTrianglesUseTheMeshMaterial)
{
    constexpr Rgb mesh_albedo{0.2, 0.7, 0.4};
    Scene scene;
    scene.add(squareMesh(), Material{DiffuseMaterial{.albedo = mesh_albedo}});
    const HitRecord hit{requireValue(scene.intersect(forward_ray, positive_t))};
    const Vector3r back_towards_origin{0.0, 0.0, 1.0};

    const Scattering scattering{scene.scatteringAt(hit)};

    expectNear(scattering.evaluate(back_towards_origin, back_towards_origin), mesh_albedo * inv_pi);
}

TEST(SceneTest, meshesBlockShadowRays)
{
    Scene scene;
    scene.add(squareMesh(), matte);

    EXPECT_TRUE(scene.occluded(forward_ray, {.min = 0.0, .max = 10.0}));
}

TEST(SceneTest, meshTrianglesAreNotLights)
{
    Scene scene;
    scene.add(squareMesh(), matte);
    const HitRecord hit{requireValue(scene.intersect(forward_ray, positive_t))};

    EXPECT_FALSE(scene.lightIndexOf(hit).has_value());
    EXPECT_TRUE(scene.lights().empty());
}

TEST(SceneTest, objectsAddedAfterAQueryAreFound)
{
    Scene scene;
    scene.add(Sphere{Point3r{0.0, 0.0, -9.0}, 1.0}, matte);
    const HitRecord before{requireValue(scene.intersect(forward_ray, positive_t))};

    scene.add(Sphere{Point3r{0.0, 0.0, -5.0}, 1.0}, matte);
    const HitRecord after{requireValue(scene.intersect(forward_ray, positive_t))};

    EXPECT_EQ(before.object, 0U);
    EXPECT_EQ(after.object, 1U);
    EXPECT_TRUE(scene.occluded(forward_ray, {.min = 0.0, .max = 4.5}));
}

TEST(SceneTest, findsTheNearestObjectAmongMany)
{
    // A 10 x 10 x 10 grid of spheres; the ray runs down the line x = 3, y = 4.
    Scene scene;
    for (int x{0}; x < 10; ++x) {
        for (int y{0}; y < 10; ++y) {
            for (int z{0}; z < 10; ++z) {
                scene.add(Sphere{Point3r{1.0 * x, 1.0 * y, -1.0 * z - 5.0}, 0.25}, matte);
            }
        }
    }
    const Ray ray{.origin = {3.0, 4.0, 0.0}, .direction = {0.0, 0.0, -1.0}};

    const HitRecord hit{requireValue(scene.intersect(ray, positive_t))};

    EXPECT_EQ(hit.object, (3U * 10U + 4U) * 10U);
    EXPECT_NEAR(hit.t, 4.75, default_tolerance);
}

TEST(SceneTest, anAreaLightIsFoundByIntersectionAndShadowRays)
{
    Scene scene;
    scene.add(Sphere{Point3r{0.0, 0.0, -20.0}, 1.0}, matte);
    scene.addAreaLight(Sphere{Point3r{0.0, 0.0, -5.0}, 1.0}, Rgb{1.0, 1.0, 1.0}, matte);

    const HitRecord hit{requireValue(scene.intersect(forward_ray, positive_t))};

    EXPECT_EQ(hit.object, 1U);
    EXPECT_TRUE(scene.occluded(forward_ray, {.min = 0.0, .max = 10.0}));
}

} // namespace
} // namespace albedo
