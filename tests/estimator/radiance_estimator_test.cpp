#include "albedo/estimator/radiance_estimator.hpp"

#include "albedo/material/material.hpp"
#include "support/expect_near.hpp"

#include <gtest/gtest.h>

namespace albedo {
namespace {

using test_support::expectNear;

constexpr Ray down_minus_z{.origin = {0.0, 0.0, 0.0}, .direction = {0.0, 0.0, -1.0}};

SampleGenerator testGenerator()
{
    return SampleGenerator::forPixel(11, {.x = 0, .y = 0}, 1);
}

// Has a `radiance` that ignores the generator, so it must not count as an estimator.
struct EstimatorWithoutGenerator {
    [[nodiscard]] static Rgb radiance(const Ray& /*ray*/, const Scene& /*scene*/) noexcept
    {
        return {};
    }
};

template <typename... Estimators>
concept CanFormEstimatorVariant = requires { typename RadianceEstimatorVariant<Estimators...>; };

TEST(RadianceEstimatorTest, typeWithoutGeneratorIsNotAnEstimator)
{
    static_assert(EstimatorLike<NormalsEstimator>);
    static_assert(EstimatorLike<ScatterOnlyEstimator>);
    static_assert(EstimatorLike<PathEstimator>);
    static_assert(!EstimatorLike<EstimatorWithoutGenerator>);
}

TEST(RadianceEstimatorTest, estimatorVariantRejectsNonEstimatorAlternatives)
{
    static_assert(CanFormEstimatorVariant<NormalsEstimator>);
    static_assert(!CanFormEstimatorVariant<NormalsEstimator, EstimatorWithoutGenerator>);
}

TEST(RadianceEstimatorTest, forwardsToNormalsEstimator)
{
    Scene scene;
    scene.add(Sphere{Point3r{0.0, 0.0, -5.0}, 1.0},
              DiffuseMaterial{.albedo = ConstantTexture{.color = {0.5, 0.5, 0.5}}});
    SampleGenerator generator{testGenerator()};

    expectNear(radiance(RadianceEstimator{NormalsEstimator{}}, down_minus_z, scene, generator),
               NormalsEstimator::radiance(down_minus_z, scene));
}

TEST(RadianceEstimatorTest, forwardsToScatterOnlyEstimatorWithItsSettings)
{
    // With no scattering allowed, a hit is black; the default limit would give the sky.
    Scene scene;
    scene.add(Sphere{Point3r{0.0, 0.0, -5.0}, 1.0},
              DiffuseMaterial{.albedo = ConstantTexture{.color = {1.0, 1.0, 1.0}}});
    scene.setSky(SkyLight::uniform({1.0, 1.0, 1.0}));
    SampleGenerator generator{testGenerator()};
    const RadianceEstimator estimator{ScatterOnlyEstimator{.max_scatter_count = 0}};

    EXPECT_TRUE(isBlack(radiance(estimator, down_minus_z, scene, generator)));
}

} // namespace
} // namespace albedo
