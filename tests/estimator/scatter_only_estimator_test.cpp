#include "albedo/estimator/scatter_only_estimator.hpp"

#include "albedo/core/vector3.hpp"
#include "albedo/material/material.hpp"
#include "albedo/sampling/sample_generator.hpp"
#include "support/expect_near.hpp"

#include <gtest/gtest.h>

namespace albedo {
namespace {

using test_support::exact_tolerance;
using test_support::expectNear;

constexpr Rgb sky_radiance{0.9, 0.7, 0.5};
const Material white{DiffuseMaterial{.albedo = ConstantTexture{.color = {1.0, 1.0, 1.0}}}};
constexpr Ray down_minus_z{.origin = {0.0, 0.0, 0.0}, .direction = {0.0, 0.0, -1.0}};
// Estimates are combinations of a few products, so they match closely.
constexpr double estimate_tolerance{1e-9};

SampleGenerator testGenerator()
{
    return SampleGenerator::forPixel(7, {.x = 0, .y = 0}, 1);
}

// A ray from a point in front of the scene towards a random spot near the origin.
Ray randomRayTowardsOrigin(SampleGenerator& generator)
{
    const Point2r target{generator.next2D()};
    const Point3r origin{0.0, 0.5, 6.0};
    const Point3r aim{4 * target.x - 2, 3 * target.y - 1, 0.0};
    return {.origin = origin, .direction = normalized(aim - origin)};
}

TEST(ScatterOnlyEstimatorTest, missReturnsTheSky)
{
    Scene scene;
    scene.setSky({.nadir = {1.0, 1.0, 1.0}, .zenith = {0.0, 0.0, 1.0}});
    SampleGenerator generator{testGenerator()};
    const Ray up{.origin = {0.0, 0.0, 0.0}, .direction = {0.0, 1.0, 0.0}};

    expectNear(ScatterOnlyEstimator{}.radiance(up, scene, generator), Rgb{0.0, 0.0, 1.0});
}

TEST(ScatterOnlyEstimatorTest, whiteFurnaceSphereDisappearsIntoTheSky)
{
    // A white diffuse object under a uniform sky reflects exactly what it
    // receives, so every path must return the sky radiance (white furnace test).
    Scene scene;
    scene.add(Sphere{Point3r{0.0, 0.0, -5.0}, 1.0}, white);
    scene.setSky(SkyLight::uniform(sky_radiance));
    SampleGenerator generator{testGenerator()};

    for (int i{0}; i < 1000; ++i) {
        expectNear(ScatterOnlyEstimator{}.radiance(down_minus_z, scene, generator), sky_radiance,
                   estimate_tolerance);
    }
}

TEST(ScatterOnlyEstimatorTest, whiteFurnaceHoldsWithInterreflections)
{
    // Touching spheres on a ground sphere bounce paths between each other
    // several times before they escape; no energy may be gained or lost.
    Scene scene;
    scene.add(Sphere{Point3r{0.0, -1000.0, 0.0}, 1000.0}, white);
    scene.add(Sphere{Point3r{-1.0, 1.0, 0.0}, 1.0}, white);
    scene.add(Sphere{Point3r{1.0, 1.0, 0.0}, 1.0}, white);
    scene.add(Sphere{Point3r{0.0, 0.5, 1.2}, 0.5}, white);
    scene.setSky(SkyLight::uniform(sky_radiance));
    SampleGenerator generator{testGenerator()};
    const ScatterOnlyEstimator estimator{.max_scatter_count = 1000};

    for (int i{0}; i < 10'000; ++i) {
        const Ray ray{randomRayTowardsOrigin(generator)};
        expectNear(estimator.radiance(ray, scene, generator), sky_radiance, estimate_tolerance);
    }
}

TEST(ScatterOnlyEstimatorTest, singleBounceIsScaledByTheAlbedo)
{
    constexpr Rgb albedo{0.5, 0.25, 0.8};
    Scene scene;
    scene.add(Sphere{Point3r{0.0, 0.0, -5.0}, 1.0},
              Material{DiffuseMaterial{.albedo = ConstantTexture{.color = albedo}}});
    scene.setSky(SkyLight::uniform(sky_radiance));
    SampleGenerator generator{testGenerator()};

    expectNear(ScatterOnlyEstimator{}.radiance(down_minus_z, scene, generator),
               albedo * sky_radiance, estimate_tolerance);
}

TEST(ScatterOnlyEstimatorTest, usesTheMaterialOfTheObjectHit)
{
    constexpr Rgb near_albedo{0.2, 0.2, 0.2};
    Scene scene;
    scene.add(Sphere{Point3r{0.0, 0.0, -9.0}, 1.0}, white);
    scene.add(Sphere{Point3r{0.0, 0.0, -5.0}, 1.0},
              Material{DiffuseMaterial{.albedo = ConstantTexture{.color = near_albedo}}});
    scene.setSky(SkyLight::uniform({1.0, 1.0, 1.0}));
    SampleGenerator generator{testGenerator()};
    // Scattered rays leave the near sphere towards +z, away from the far one.

    expectNear(ScatterOnlyEstimator{}.radiance(down_minus_z, scene, generator), near_albedo,
               estimate_tolerance);
}

TEST(ScatterOnlyEstimatorTest, diffuseGroundAveragesTheGradientSky)
{
    // A diffuse plane facing +y sees the sky weighted by cos(theta) / pi, under
    // which the mean height of a direction is 2/3; the linear gradient then gives
    // nadir + (zenith - nadir) * (1 + 2/3) / 2. The huge sphere stands in for the plane.
    constexpr Rgb nadir{0.0, 0.0, 0.0};
    constexpr Rgb zenith{1.0, 1.0, 1.0};
    Scene scene;
    scene.add(Sphere{Point3r{0.0, -1000.0, 0.0}, 1000.0}, white);
    scene.setSky({.nadir = nadir, .zenith = zenith});
    SampleGenerator generator{testGenerator()};
    const Ray at_ground{.origin = {0.0, 1.0, 0.0}, .direction = {0.0, -1.0, 0.0}};
    constexpr int sample_count{100'000};

    Rgb sum{};
    for (int i{0}; i < sample_count; ++i) {
        sum += ScatterOnlyEstimator{}.radiance(at_ground, scene, generator);
    }

    // The per-sample standard deviation is about 0.12, so 0.003 is ~8 sigma.
    EXPECT_NEAR((sum / sample_count).g, 5.0 / 6.0, 0.003);
}

TEST(ScatterOnlyEstimatorTest, pathTrappedInsideAClosedSurfaceIsBlack)
{
    // From inside a white sphere no path escapes, so the scatter limit ends it
    // and it carries no light.
    Scene scene;
    scene.add(Sphere{Point3r{0.0, 0.0, 0.0}, 2.0}, white);
    scene.setSky(SkyLight::uniform(sky_radiance));
    SampleGenerator generator{testGenerator()};

    EXPECT_TRUE(isBlack(
        ScatterOnlyEstimator{.max_scatter_count = 8}.radiance(down_minus_z, scene, generator)));
}

TEST(ScatterOnlyEstimatorTest, zeroScatterCountMakesEveryHitBlack)
{
    Scene scene;
    scene.add(Sphere{Point3r{0.0, 0.0, -5.0}, 1.0}, white);
    scene.setSky(SkyLight::uniform(sky_radiance));
    SampleGenerator generator{testGenerator()};

    EXPECT_TRUE(isBlack(
        ScatterOnlyEstimator{.max_scatter_count = 0}.radiance(down_minus_z, scene, generator)));
}

TEST(ScatterOnlyEstimatorTest, scatterCountLimitAllowsExactlyThatManyScatterings)
{
    // One scattering off a convex sphere is all this path needs to escape.
    constexpr Rgb albedo{0.5, 0.25, 0.8};
    Scene scene;
    scene.add(Sphere{Point3r{0.0, 0.0, -5.0}, 1.0},
              Material{DiffuseMaterial{.albedo = ConstantTexture{.color = albedo}}});
    scene.setSky(SkyLight::uniform(sky_radiance));
    SampleGenerator generator{testGenerator()};

    expectNear(
        ScatterOnlyEstimator{.max_scatter_count = 1}.radiance(down_minus_z, scene, generator),
        albedo * sky_radiance, estimate_tolerance);
}

TEST(ScatterOnlyEstimatorTest, sameGeneratorStateGivesTheSameEstimate)
{
    Scene scene;
    scene.add(Sphere{Point3r{0.0, -1000.0, 0.0}, 1000.0}, white);
    scene.setSky({.nadir = {0.0, 0.0, 0.0}, .zenith = {1.0, 1.0, 1.0}});
    SampleGenerator first{testGenerator()};
    SampleGenerator second{testGenerator()};
    const Ray at_ground{.origin = {0.0, 1.0, 0.0}, .direction = {0.0, -1.0, 0.0}};

    expectNear(ScatterOnlyEstimator{}.radiance(at_ground, scene, first),
               ScatterOnlyEstimator{}.radiance(at_ground, scene, second), exact_tolerance);
}

TEST(ScatterOnlyEstimatorTest, indexMatchedGlassIsInvisible)
{
    // Glass with the index of air neither reflects nor bends, so the path must
    // pass through both walls and see the sky straight behind. A continued ray
    // started on the wrong side of the surface would re-enter the sphere instead.
    Scene scene;
    scene.add(Sphere{Point3r{0.0, 0.0, -5.0}, 1.0},
              DielectricMaterial{.ior = 1.0, .roughness = 0.0});
    const SkyLight sky{.nadir = {1.0, 1.0, 1.0}, .zenith = {0.0, 0.0, 1.0}};
    scene.setSky(sky);
    SampleGenerator generator{testGenerator()};
    const Ray tilted{.origin = {0.0, 0.0, 0.0}, .direction = normalized(Vector3r{0.1, 0.15, -1.0})};

    expectNear(ScatterOnlyEstimator{}.radiance(tilted, scene, generator),
               sky.emittedRadiance(tilted.direction), estimate_tolerance);
}

TEST(ScatterOnlyEstimatorTest, whiteFurnaceHoldsThroughSmoothGlass)
{
    // Entering the glass weights the path by 1 / eta^2 and leaving by eta^2, and
    // Fresnel reflection only reroutes it, so every path returns the sky.
    Scene scene;
    scene.add(Sphere{Point3r{0.0, 0.0, 0.0}, 1.0},
              DielectricMaterial{.ior = 1.5, .roughness = 0.0});
    scene.setSky(SkyLight::uniform(sky_radiance));
    SampleGenerator generator{testGenerator()};

    for (int i{0}; i < 1000; ++i) {
        const Ray ray{randomRayTowardsOrigin(generator)};
        expectNear(ScatterOnlyEstimator{}.radiance(ray, scene, generator), sky_radiance,
                   estimate_tolerance);
    }
}

TEST(ScatterOnlyEstimatorTest, cameraRaySeesAnAreaLight)
{
    constexpr Rgb emitted{4.0, 3.0, 2.0};
    Scene scene;
    scene.addAreaLight(Sphere{Point3r{0.0, 0.0, -5.0}, 1.0}, emitted,
                       DiffuseMaterial{.albedo = ConstantTexture{.color = {0.0, 0.0, 0.0}}});
    SampleGenerator generator{testGenerator()};

    expectNear(ScatterOnlyEstimator{}.radiance(down_minus_z, scene, generator), emitted,
               estimate_tolerance);
}

TEST(ScatterOnlyEstimatorTest, diffuseGroundUnderAnAreaLightMatchesTheAnalyticRadiance)
{
    // A sphere light straight above a diffuse plane gives irradiance
    // E = L pi sin^2(theta_max), so the plane reflects albedo E / pi =
    // albedo L sin^2(theta_max). Here sin^2 = 2^2 / 5^2.
    constexpr Rgb emitted{5.0, 5.0, 5.0};
    constexpr Real albedo{0.5};
    Scene scene;
    scene.add(Sphere{Point3r{0.0, -1000.0, 0.0}, 1000.0},
              DiffuseMaterial{.albedo = ConstantTexture{.color = {albedo, albedo, albedo}}});
    scene.addAreaLight(Sphere{Point3r{0.0, 5.0, 0.0}, 2.0}, emitted,
                       DiffuseMaterial{.albedo = ConstantTexture{.color = {0.0, 0.0, 0.0}}});
    SampleGenerator generator{testGenerator()};
    // Straight down at the point under the light, from below the light.
    const Ray at_ground{.origin = {0.0, 1.0, 0.0}, .direction = {0.0, -1.0, 0.0}};
    constexpr int sample_count{100'000};

    Rgb sum{};
    for (int i{0}; i < sample_count; ++i) {
        sum += ScatterOnlyEstimator{}.radiance(at_ground, scene, generator);
    }

    // Each sample is 0 or albedo L = 2.5, hit with probability 0.16: standard
    // deviation about 0.92, so 0.02 is over 6 standard errors.
    const Real expected{albedo * emitted.g * 4.0 / 25.0};
    expectNear(sum / sample_count, Rgb{expected, expected, expected}, 0.02);
}

TEST(ScatterOnlyEstimatorTest, emissionSeenBeforeTheScatterLimitCounts)
{
    // The limit stops scattering, not the light already reached.
    constexpr Rgb emitted{4.0, 3.0, 2.0};
    Scene scene;
    scene.addAreaLight(Sphere{Point3r{0.0, 0.0, -5.0}, 1.0}, emitted,
                       DiffuseMaterial{.albedo = ConstantTexture{.color = {0.5, 0.5, 0.5}}});
    SampleGenerator generator{testGenerator()};

    expectNear(
        ScatterOnlyEstimator{.max_scatter_count = 0}.radiance(down_minus_z, scene, generator),
        emitted, estimate_tolerance);
}

} // namespace
} // namespace albedo
