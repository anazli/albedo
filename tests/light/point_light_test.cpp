#include "albedo/light/point_light.hpp"

#include "support/expect_near.hpp"
#include "support/require_value.hpp"

#include <gtest/gtest.h>

#include <cmath>

namespace albedo {
namespace {

using test_support::default_tolerance;
using test_support::exact_tolerance;
using test_support::expectNear;
using test_support::requireValue;

constexpr PointLight light{.position = {1.0, 4.0, -2.0}, .intensity = {8.0, 4.0, 2.0}};
constexpr Point3r receiver{1.0, 0.0, 1.0};

TEST(PointLightTest, sampleAimsAtTheLight)
{
    const LightSample sample{requireValue(light.sampleIncidentLight(receiver, {0.3, 0.7}))};

    expectNear(sample.direction, Vector3r{0.0, 0.8, -0.6});
    EXPECT_NEAR(sample.distance, 5.0, default_tolerance);
}

TEST(PointLightTest, incidentRadianceFallsOffWithTheSquaredDistance)
{
    const LightSample sample{requireValue(light.sampleIncidentLight(receiver, {0.3, 0.7}))};

    expectNear(sample.radiance, Rgb{8.0, 4.0, 2.0} / 25.0);
}

TEST(PointLightTest, sampleIsADeltaWithUnitProbability)
{
    const LightSample sample{requireValue(light.sampleIncidentLight(receiver, {0.3, 0.7}))};

    EXPECT_TRUE(sample.delta);
    EXPECT_NEAR(sample.pdf, 1.0, exact_tolerance);
}

TEST(PointLightTest, noDirectionHasADensity)
{
    // A direction found any other way than sampling hits the point with probability 0.
    EXPECT_NEAR(light.incidentLightPdf(receiver, {0.0, 0.8, -0.6}), 0.0, exact_tolerance);
}

TEST(PointLightTest, receiverAtTheLightGetsNoSample)
{
    EXPECT_FALSE(light.sampleIncidentLight(light.position, {0.3, 0.7}).has_value());
}

} // namespace
} // namespace albedo
