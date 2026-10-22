#include "albedo/scene/scene.hpp"

#include "albedo/material/material.hpp"
#include "support/expect_near.hpp"
#include "support/require_value.hpp"

#include <gtest/gtest.h>

#include <limits>

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

} // namespace
} // namespace albedo
