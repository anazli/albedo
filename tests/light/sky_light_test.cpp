#include "albedo/light/sky_light.hpp"

#include "support/expect_near.hpp"
#include "support/require_value.hpp"

#include <gtest/gtest.h>

#include <cmath>

namespace albedo {
namespace {

using test_support::expectNear;

constexpr Rgb nadir{1.0, 0.8, 0.6};
constexpr Rgb zenith{0.2, 0.4, 1.0};
constexpr SkyLight gradient{.nadir = nadir, .zenith = zenith};

TEST(SkyLightTest, defaultSkyIsBlack)
{
    constexpr SkyLight sky;

    EXPECT_TRUE(isBlack(sky.emittedRadiance({0.0, 1.0, 0.0})));
}

TEST(SkyLightTest, uniformSkyIsTheSameInEveryDirection)
{
    constexpr Rgb radiance{0.3, 0.6, 0.9};
    constexpr SkyLight sky{SkyLight::uniform(radiance)};

    expectNear(sky.emittedRadiance({0.0, 1.0, 0.0}), radiance);
    expectNear(sky.emittedRadiance({0.0, -1.0, 0.0}), radiance);
    expectNear(sky.emittedRadiance(normalized(Vector3r{0.3, 0.2, -0.9})), radiance);
}

TEST(SkyLightTest, gradientIsZenithStraightUp)
{
    expectNear(gradient.emittedRadiance({0.0, 1.0, 0.0}), zenith);
}

TEST(SkyLightTest, gradientIsNadirStraightDown)
{
    expectNear(gradient.emittedRadiance({0.0, -1.0, 0.0}), nadir);
}

TEST(SkyLightTest, gradientIsHalfwayAtTheHorizon)
{
    expectNear(gradient.emittedRadiance({0.0, 0.0, 1.0}), (nadir + zenith) * 0.5);
}

TEST(SkyLightTest, gradientDependsOnlyOnHeight)
{
    expectNear(gradient.emittedRadiance(normalized(Vector3r{1.0, 0.5, 0.0})),
               gradient.emittedRadiance(normalized(Vector3r{0.0, 0.5, -1.0})));
}

TEST(SkyLightTest, sampleCarriesTheSkyRadianceInItsDirection)
{
    const LightSample sample{
        test_support::requireValue(gradient.sampleIncidentLight({1.0, 2.0, 3.0}, {0.3, 0.7}))};

    EXPECT_NEAR(length(sample.direction), 1.0, test_support::default_tolerance);
    expectNear(sample.radiance, gradient.emittedRadiance(sample.direction));
    EXPECT_TRUE(std::isinf(sample.distance));
    EXPECT_FALSE(sample.delta);
}

TEST(SkyLightTest, sampleAndPdfAreUniformOverTheSphere)
{
    const LightSample sample{
        test_support::requireValue(gradient.sampleIncidentLight({1.0, 2.0, 3.0}, {0.3, 0.7}))};

    EXPECT_NEAR(sample.pdf, 1 / (4 * pi), test_support::default_tolerance);
    EXPECT_NEAR(gradient.incidentLightPdf({1.0, 2.0, 3.0}, {0.0, -1.0, 0.0}), 1 / (4 * pi),
                test_support::default_tolerance);
}

} // namespace
} // namespace albedo
