#include "albedo/estimator/path_estimator.hpp"

#include "albedo/estimator/scatter_only_estimator.hpp"
#include "albedo/material/material.hpp"
#include "albedo/sampling/sample_generator.hpp"
#include "albedo/scattering/fresnel.hpp"
#include "support/expect_near.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <functional>

namespace albedo {
namespace {

using test_support::exact_tolerance;
using test_support::expectNear;

const Material black{DiffuseMaterial{.albedo = {0.0, 0.0, 0.0}}};
const Material white{DiffuseMaterial{.albedo = {1.0, 1.0, 1.0}}};
constexpr Ray down_minus_z{.origin = {0.0, 0.0, 0.0}, .direction = {0.0, 0.0, -1.0}};
// Straight down onto the ground at the origin, from below any light above it.
constexpr Ray at_ground{.origin = {0.0, 1.0, 0.0}, .direction = {0.0, -1.0, 0.0}};
constexpr double exact_estimate{1e-9};

SampleGenerator testGenerator()
{
    return SampleGenerator::forPixel(17, {.x = 0, .y = 0}, 1);
}

void addGround(Scene& scene, Real albedo)
{
    scene.add(Sphere{Point3r{0.0, -1000.0, 0.0}, 1000.0},
              DiffuseMaterial{.albedo = {albedo, albedo, albedo}});
}

// The mean of `sample_count` estimates, each drawn from one shared generator.
Rgb meanRadiance(const std::function<Rgb(SampleGenerator&)>& estimate, int sample_count)
{
    SampleGenerator generator{testGenerator()};
    Rgb sum{};
    for (int i{0}; i < sample_count; ++i) {
        sum += estimate(generator);
    }
    return sum / static_cast<Real>(sample_count);
}

TEST(PathEstimatorTest, missReturnsTheSky)
{
    Scene scene;
    const SkyLight sky{.nadir = {1.0, 1.0, 1.0}, .zenith = {0.0, 0.0, 1.0}};
    scene.setSky(sky);
    SampleGenerator generator{testGenerator()};
    const Ray up{.origin = {0.0, 0.0, 0.0}, .direction = {0.0, 1.0, 0.0}};

    expectNear(PathEstimator{}.radiance(up, scene, generator), sky.emittedRadiance(up.direction),
               exact_estimate);
}

TEST(PathEstimatorTest, cameraRaySeesAnAreaLightWithFullWeight)
{
    constexpr Rgb emitted{4.0, 3.0, 2.0};
    Scene scene;
    scene.addAreaLight(Sphere{Point3r{0.0, 0.0, -5.0}, 1.0}, emitted, black);
    SampleGenerator generator{testGenerator()};

    expectNear(PathEstimator{}.radiance(down_minus_z, scene, generator), emitted, exact_estimate);
}

TEST(PathEstimatorTest, pointLightOverADiffusePlaneMatchesTheAnalyticRadiance)
{
    // L = (albedo / pi) I cos(theta) / d^2 with the light 2 units straight up.
    // Only the light sample contributes and it is deterministic, so every
    // estimate is exact.
    constexpr Rgb intensity{8.0, 6.0, 4.0};
    constexpr Real albedo{0.5};
    Scene scene;
    addGround(scene, albedo);
    scene.addLight(PointLight{.position = {0.0, 2.0, 0.0}, .intensity = intensity});
    SampleGenerator generator{testGenerator()};

    for (int i{0}; i < 100; ++i) {
        expectNear(PathEstimator{}.radiance(at_ground, scene, generator),
                   intensity * (albedo * inv_pi / 4), exact_estimate);
    }
}

TEST(PathEstimatorTest, pointLightBehindAnOccluderIsDark)
{
    Scene scene;
    addGround(scene, 0.5);
    scene.add(Sphere{Point3r{0.0, 1.5, 0.0}, 0.2}, black);
    scene.addLight(PointLight{.position = {0.0, 2.0, 0.0}, .intensity = {8.0, 8.0, 8.0}});
    SampleGenerator generator{testGenerator()};
    const Ray beside_blocker{.origin = {0.5, 1.0, 0.0},
                             .direction = normalized(Vector3r{-0.5, -1.0, 0.0})};

    EXPECT_TRUE(isBlack(PathEstimator{}.radiance(beside_blocker, scene, generator)));
}

TEST(PathEstimatorTest, mirrorShowsAnAreaLightWithFullWeight)
{
    // The mirror's sample is specular, so the light it reflects counts in full:
    // F(cos theta) L, exactly. Light sampling cannot see through a mirror.
    constexpr Rgb emitted{4.0, 3.0, 2.0};
    constexpr Rgb eta{0.2, 0.9, 1.4};
    constexpr Rgb k{3.0, 2.4, 1.6};
    Scene scene;
    scene.add(Sphere{Point3r{0.0, -1000.0, 0.0}, 1000.0},
              ConductorMaterial{.eta = eta, .k = k, .roughness = 0.0});
    scene.addAreaLight(Sphere{Point3r{0.0, 3.0, -2.0}, 1.0}, emitted, black);
    SampleGenerator generator{testGenerator()};
    // Hits the mirror at the origin and reflects straight towards the light.
    const Ray tilted{.origin = {0.0, 3.0, 2.0}, .direction = normalized(Vector3r{0.0, -3.0, -2.0})};

    const Real cos_theta{-tilted.direction.y};
    expectNear(PathEstimator{}.radiance(tilted, scene, generator),
               fresnelConductor(cos_theta, eta, k) * emitted, exact_estimate);
}

TEST(PathEstimatorTest, diffuseGroundUnderAnAreaLightMatchesTheAnalyticRadiance)
{
    // albedo L sin^2(theta_max), as for ScatterOnlyEstimator, but light
    // sampling lowers the variance enough for a much tighter tolerance.
    constexpr Rgb emitted{5.0, 5.0, 5.0};
    constexpr Real albedo{0.5};
    Scene scene;
    addGround(scene, albedo);
    scene.addAreaLight(Sphere{Point3r{0.0, 5.0, 0.0}, 2.0}, emitted, black);

    const Rgb mean{meanRadiance(
        [&](SampleGenerator& generator) {
            return PathEstimator{}.radiance(at_ground, scene, generator);
        },
        20'000)};

    const Real expected{albedo * emitted.g * 4.0 / 25.0};
    expectNear(mean, Rgb{expected, expected, expected}, 0.004);
}

TEST(PathEstimatorTest, whiteFurnaceAveragesToTheSky)
{
    // MIS mixes two estimators, so single estimates vary, but their mean must be
    // the sky: a white diffuse object neither gains nor loses energy.
    constexpr Rgb sky_radiance{0.9, 0.7, 0.5};
    Scene scene;
    scene.add(Sphere{Point3r{0.0, 0.0, -5.0}, 1.0}, white);
    scene.setSky(SkyLight::uniform(sky_radiance));

    const Rgb mean{meanRadiance(
        [&](SampleGenerator& generator) {
            return PathEstimator{}.radiance(down_minus_z, scene, generator);
        },
        20'000)};

    expectNear(mean, sky_radiance, 0.01);
}

TEST(PathEstimatorTest, russianRouletteKeepsInterreflectionsUnbiased)
{
    // Touching white spheres bounce paths many times; roulette from the first
    // bounce on must still average to the sky.
    constexpr Rgb sky_radiance{0.6, 0.6, 0.6};
    Scene scene;
    addGround(scene, 1.0);
    scene.add(Sphere{Point3r{-1.0, 1.0, 0.0}, 1.0}, white);
    scene.add(Sphere{Point3r{1.0, 1.0, 0.0}, 1.0}, white);
    scene.setSky(SkyLight::uniform(sky_radiance));
    const Ray into_the_gap{.origin = {0.0, 1.0, 3.0},
                           .direction = normalized(Vector3r{0.0, -0.4, -1.0})};

    const Rgb mean{meanRadiance(
        [&](SampleGenerator& generator) {
            return PathEstimator{.roulette_start = 0}.radiance(into_the_gap, scene, generator);
        },
        20'000)};

    expectNear(mean, sky_radiance, 0.015);
}

TEST(PathEstimatorTest, agreesWithScatterOnlyOnGlossyMetalUnderAnAreaLight)
{
    // Rough metal under a small light is where light sampling and BSDF sampling
    // both matter; wrong MIS weights would bias the mean against the simpler
    // estimator, which samples the BSDF only.
    Scene scene;
    scene.add(Sphere{Point3r{0.0, -1000.0, 0.0}, 1000.0},
              ConductorMaterial{.eta = {0.2, 0.9, 1.4}, .k = {3.0, 2.4, 1.6}, .roughness = 0.4});
    scene.addAreaLight(Sphere{Point3r{0.0, 3.0, -2.0}, 1.0}, {4.0, 4.0, 4.0}, black);
    scene.setSky(SkyLight::uniform({0.2, 0.2, 0.2}));
    const Ray tilted{.origin = {0.0, 3.0, 2.0}, .direction = normalized(Vector3r{0.0, -3.0, -2.0})};
    constexpr int sample_count{100'000};

    const Rgb path{meanRadiance(
        [&](SampleGenerator& generator) {
            return PathEstimator{}.radiance(tilted, scene, generator);
        },
        sample_count)};
    const Rgb reference{meanRadiance(
        [&](SampleGenerator& generator) {
            return ScatterOnlyEstimator{}.radiance(tilted, scene, generator);
        },
        sample_count)};

    expectNear(path, reference, 0.03 * luminance(reference) + 0.01);
}

TEST(PathEstimatorTest, zeroDepthShowsOnlyWhatTheCameraRayHits)
{
    constexpr Rgb emitted{4.0, 3.0, 2.0};
    Scene scene;
    addGround(scene, 0.5);
    scene.addLight(PointLight{.position = {0.0, 2.0, 0.0}, .intensity = {8.0, 8.0, 8.0}});
    SampleGenerator generator{testGenerator()};
    const PathEstimator estimator{.max_depth = 0};

    EXPECT_TRUE(isBlack(estimator.radiance(at_ground, scene, generator)));
    scene.addAreaLight(Sphere{Point3r{0.0, 0.0, -5.0}, 1.0}, emitted, black);
    expectNear(estimator.radiance(down_minus_z, scene, generator), emitted, exact_estimate);
}

TEST(PathEstimatorTest, sameGeneratorStateGivesTheSameEstimate)
{
    Scene scene;
    addGround(scene, 0.5);
    scene.addAreaLight(Sphere{Point3r{0.0, 5.0, 0.0}, 2.0}, {5.0, 5.0, 5.0}, black);
    scene.setSky({.nadir = {0.0, 0.0, 0.0}, .zenith = {1.0, 1.0, 1.0}});
    SampleGenerator first{testGenerator()};
    SampleGenerator second{testGenerator()};

    expectNear(PathEstimator{}.radiance(at_ground, scene, first),
               PathEstimator{}.radiance(at_ground, scene, second), exact_tolerance);
}

TEST(PathEstimatorTest, skyBehindAnEnclosureIsDark)
{
    // A white sphere sealed inside a black shell: neither light sampling nor
    // scattering can reach the sky, so the estimate is exactly black.
    Scene scene;
    scene.add(Sphere{Point3r{0.0, 0.0, 0.0}, 10.0}, black);
    scene.add(Sphere{Point3r{0.0, 0.0, -5.0}, 1.0}, white);
    scene.setSky(SkyLight::uniform({1.0, 1.0, 1.0}));
    SampleGenerator generator{testGenerator()};

    for (int i{0}; i < 100; ++i) {
        EXPECT_TRUE(isBlack(PathEstimator{}.radiance(down_minus_z, scene, generator)));
    }
}

TEST(PathEstimatorTest, agreesWithScatterOnlyThroughRoughGlass)
{
    // Light sampling behind rough glass starts shadow rays behind the surface
    // and weighs them against the refraction pdf, which ScatterOnly never uses.
    Scene scene;
    scene.add(Sphere{Point3r{0.0, 0.0, -3.0}, 1.0},
              DielectricMaterial{.ior = 1.5, .roughness = 0.3});
    scene.addAreaLight(Sphere{Point3r{0.0, 0.0, -7.0}, 1.5}, {4.0, 4.0, 4.0}, black);
    scene.setSky(SkyLight::uniform({0.2, 0.2, 0.2}));
    const Ray towards_glass{.origin = {0.0, 0.0, 0.0},
                            .direction = normalized(Vector3r{0.1, 0.05, -1.0})};
    constexpr int sample_count{100'000};

    const Rgb path{meanRadiance(
        [&](SampleGenerator& generator) {
            return PathEstimator{}.radiance(towards_glass, scene, generator);
        },
        sample_count)};
    const Rgb reference{meanRadiance(
        [&](SampleGenerator& generator) {
            return ScatterOnlyEstimator{}.radiance(towards_glass, scene, generator);
        },
        sample_count)};

    expectNear(path, reference, 0.03 * luminance(reference) + 0.01);
}

TEST(PathEstimatorTest, pointLightAndSkyAddUp)
{
    // With two lights each is chosen half the time. A diffuse plane under a
    // uniform sky S reflects albedo S, and under the point light
    // (albedo / pi) I / d^2; the mean must be the sum.
    constexpr Rgb intensity{8.0, 8.0, 8.0};
    constexpr Rgb sky_radiance{0.4, 0.4, 0.4};
    constexpr Real albedo{0.5};
    Scene scene;
    addGround(scene, albedo);
    scene.addLight(PointLight{.position = {0.0, 2.0, 0.0}, .intensity = intensity});
    scene.setSky(SkyLight::uniform(sky_radiance));

    const Rgb mean{meanRadiance(
        [&](SampleGenerator& generator) {
            return PathEstimator{}.radiance(at_ground, scene, generator);
        },
        20'000)};

    expectNear(mean, sky_radiance * albedo + intensity * (albedo * inv_pi / 4), 0.01);
}

TEST(PathEstimatorDeathTest, rejectsANegativeDepthLimit)
{
    const Scene scene;
    SampleGenerator generator{testGenerator()};

    EXPECT_DEBUG_DEATH(
        static_cast<void>(PathEstimator{.max_depth = -1}.radiance(down_minus_z, scene, generator)),
        "");
}

TEST(PathEstimatorDeathTest, rejectsANegativeRouletteStart)
{
    const Scene scene;
    SampleGenerator generator{testGenerator()};

    EXPECT_DEBUG_DEATH(static_cast<void>(PathEstimator{.roulette_start = -1}.radiance(
                           down_minus_z, scene, generator)),
                       "");
}

} // namespace
} // namespace albedo
