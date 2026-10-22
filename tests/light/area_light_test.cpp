#include "albedo/light/area_light.hpp"

#include "albedo/core/vector3.hpp"
#include "albedo/sampling/pcg32.hpp"
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

constexpr Rgb radiance{3.0, 2.0, 1.0};
const AreaLight light{.shape = Sphere{Point3r{0.0, 5.0, 0.0}, 2.0}, .radiance = radiance};
constexpr Point3r receiver{0.0, 0.0, 0.0};

Point2r nextPoint(Pcg32& random)
{
    const Real x{random.nextReal()};
    return {.x = x, .y = random.nextReal()};
}

// The solid angle of a sphere of radius r seen from distance d: 2 pi (1 - cos theta_max).
Real subtendedSolidAngle(Real r, Real d)
{
    return 2 * pi * (1 - std::sqrt(1 - r * r / (d * d)));
}

TEST(AreaLightTest, samplesLandOnTheNearSideOfTheSphere)
{
    Pcg32 random{{.seed = 51, .stream = 1}};

    for (int i{0}; i < 10'000; ++i) {
        const LightSample sample{
            requireValue(light.sampleIncidentLight(receiver, nextPoint(random)))};
        const Point3r point{receiver + sample.direction * sample.distance};
        ASSERT_NEAR(length(point - light.shape.center()), 2.0, 1e-9);
        // The near side faces the receiver.
        ASSERT_LT(dot(point - light.shape.center(), sample.direction), 1e-9);
        ASSERT_NEAR(length(sample.direction), 1.0, 1e-12);
    }
}

TEST(AreaLightTest, sampleCarriesTheEmittedRadianceAndConePdf)
{
    const LightSample sample{requireValue(light.sampleIncidentLight(receiver, {0.3, 0.7}))};

    expectNear(sample.radiance, radiance);
    EXPECT_FALSE(sample.delta);
    EXPECT_NEAR(sample.pdf, 1 / subtendedSolidAngle(2.0, 5.0), 1e-12);
}

TEST(AreaLightTest, pdfMatchesSampling)
{
    Pcg32 random{{.seed = 52, .stream = 1}};

    for (int i{0}; i < 1000; ++i) {
        const LightSample sample{
            requireValue(light.sampleIncidentLight(receiver, nextPoint(random)))};
        ASSERT_NEAR(light.incidentLightPdf(receiver, sample.direction), sample.pdf, 1e-9);
    }
}

TEST(AreaLightTest, pdfIsZeroForDirectionsMissingTheSphere)
{
    EXPECT_NEAR(light.incidentLightPdf(receiver, normalized(Vector3r{1.0, 1.0, 0.0})), 0.0,
                exact_tolerance);
}

TEST(AreaLightTest, sampledIrradianceMatchesTheAnalyticValue)
{
    // A surface facing the sphere's center receives E = L pi sin^2(theta_max).
    Pcg32 random{{.seed = 53, .stream = 1}};
    constexpr int sample_count{100'000};
    const Vector3r facing{0.0, 1.0, 0.0};
    Rgb sum{};

    for (int i{0}; i < sample_count; ++i) {
        const LightSample sample{
            requireValue(light.sampleIncidentLight(receiver, nextPoint(random)))};
        sum += sample.radiance * (dot(sample.direction, facing) / sample.pdf);
    }

    const Real sin_squared{4.0 / 25.0};
    expectNear(sum / sample_count, radiance * (pi * sin_squared), 0.003);
}

TEST(AreaLightTest, pdfStaysAccurateForASmallDistantSphere)
{
    // 1 - cos(theta_max) is about 5e-13 here, where computing it naively loses
    // most digits. The solid angle is close to pi r^2 / d^2.
    const AreaLight tiny{.shape = Sphere{Point3r{0.0, 1000.0, 0.0}, 1e-3}, .radiance = radiance};

    const LightSample sample{requireValue(tiny.sampleIncidentLight(receiver, {0.3, 0.7}))};

    const Real expected{1e6 * 1e6 / (pi * 1.0)};
    EXPECT_NEAR(sample.pdf / expected, 1.0, 1e-6);
}

TEST(AreaLightTest, receiverInsideTheSphereGetsNoSample)
{
    // From inside, only the sphere's inside is visible, and it does not emit.
    const Point3r inside{0.0, 5.5, 0.5};

    EXPECT_FALSE(light.sampleIncidentLight(inside, {0.3, 0.7}).has_value());
    EXPECT_NEAR(light.incidentLightPdf(inside, {0.0, 1.0, 0.0}), 0.0, exact_tolerance);
}

TEST(AreaLightTest, distanceStaysAccurateForAReceiverAtTheSurface)
{
    // One ulp below y = 3, the sphere's lowest point: as close as a double gets.
    // No ray reaches the sphere in less than the gap d - r; computing the
    // distance as d cos - sqrt(r^2 - d^2 sin^2) cancels and falls below it.
    const Point3r just_outside{0.0, std::nextafter(3.0, 0.0), 0.0};
    const Real gap{(5.0 - just_outside.y) - 2.0};
    Pcg32 random{{.seed = 55, .stream = 1}};

    for (int i{0}; i < 1000; ++i) {
        const LightSample sample{
            requireValue(light.sampleIncidentLight(just_outside, nextPoint(random)))};
        // A relative tolerance leaves room for rounding but not for the factor of
        // about two the cancelling form loses.
        ASSERT_GE(sample.distance, gap * (1 - 1e-9));
        const Point3r point{just_outside + sample.direction * sample.distance};
        ASSERT_NEAR(length(point - light.shape.center()), 2.0, 1e-9);
    }
}

TEST(AreaLightTest, sphereTooSmallToResolveGetsNoSample)
{
    // Its solid angle underflows, so the pdf would be infinite.
    const AreaLight speck{.shape = Sphere{Point3r{0.0, 1e10, 0.0}, 1e-150}, .radiance = radiance};

    EXPECT_FALSE(speck.sampleIncidentLight(receiver, {0.3, 0.7}).has_value());
    EXPECT_NEAR(speck.incidentLightPdf(receiver, {0.0, 1.0, 0.0}), 0.0, exact_tolerance);
}

TEST(AreaLightTest, emitsFromTheOutsideOnly)
{
    const HitRecord outside{
        .t = 1.0, .point = {0.0, 3.0, 0.0}, .normal = {0.0, -1.0, 0.0}, .front_face = true};
    HitRecord inside{outside};
    inside.front_face = false;

    expectNear(light.emittedRadiance(outside), radiance);
    EXPECT_TRUE(isBlack(light.emittedRadiance(inside)));
}

} // namespace
} // namespace albedo
