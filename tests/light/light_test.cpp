#include "albedo/light/light.hpp"

#include "support/expect_near.hpp"
#include "support/require_value.hpp"

#include <gtest/gtest.h>

#include <optional>

namespace albedo {
namespace {

using test_support::default_tolerance;
using test_support::expectNear;
using test_support::requireValue;

// Can be sampled but has no pdf, so it must not count as a light.
struct LightWithoutPdf {
    [[nodiscard]] static std::optional<LightSample> sampleIncidentLight(Point3r /*point*/,
                                                                        Point2r /*u*/) noexcept
    {
        return std::nullopt;
    }
};

template <typename... Lights>
concept CanFormLightVariant = requires { typename LightVariant<Lights...>; };

TEST(LightTest, typeWithoutPdfIsNotALight)
{
    static_assert(LightLike<PointLight>);
    static_assert(LightLike<AreaLight>);
    static_assert(LightLike<SkyLight>);
    static_assert(!LightLike<LightWithoutPdf>);
}

TEST(LightTest, lightVariantRejectsNonLightAlternatives)
{
    static_assert(CanFormLightVariant<PointLight>);
    static_assert(!CanFormLightVariant<PointLight, LightWithoutPdf>);
}

TEST(LightTest, forwardsToTheHeldAlternative)
{
    const SkyLight sky{.nadir = {1.0, 1.0, 1.0}, .zenith = {0.0, 0.0, 1.0}};
    const Light light{sky};
    constexpr Point3r point{0.0, 0.0, 0.0};

    expectNear(requireValue(sampleIncidentLight(light, point, {0.2, 0.9})).direction,
               requireValue(sky.sampleIncidentLight(point, {0.2, 0.9})).direction);
    EXPECT_NEAR(incidentLightPdf(light, point, {0.0, 1.0, 0.0}),
                sky.incidentLightPdf(point, {0.0, 1.0, 0.0}), default_tolerance);
}

TEST(LightTest, forwardsToAPointLight)
{
    const Light light{PointLight{.position = {0.0, 2.0, 0.0}, .intensity = {1.0, 1.0, 1.0}}};

    const LightSample sample{requireValue(sampleIncidentLight(light, {0.0, 0.0, 0.0}, {0.5, 0.5}))};

    EXPECT_TRUE(sample.delta);
    EXPECT_NEAR(sample.distance, 2.0, default_tolerance);
}

TEST(LightTest, forwardsToAnAreaLight)
{
    const AreaLight area{.shape = Sphere{Point3r{0.0, 5.0, 0.0}, 2.0}, .radiance = {1.0, 1.0, 1.0}};
    const Light light{area};
    constexpr Point3r point{0.0, 0.0, 0.0};

    EXPECT_NEAR(incidentLightPdf(light, point, {0.0, 1.0, 0.0}),
                area.incidentLightPdf(point, {0.0, 1.0, 0.0}), default_tolerance);
    EXPECT_FALSE(requireValue(sampleIncidentLight(light, point, {0.5, 0.5})).delta);
}

} // namespace
} // namespace albedo
