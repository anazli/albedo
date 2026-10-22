#include "albedo/scattering/lambert_lobe.hpp"

#include "albedo/core/vector3.hpp"
#include "albedo/sampling/pcg32.hpp"
#include "support/expect_near.hpp"
#include "support/require_value.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <optional>

namespace albedo {
namespace {

using test_support::default_tolerance;
using test_support::expectNear;
using test_support::requireValue;

constexpr Rgb albedo{0.2, 0.5, 0.8};
constexpr LambertLobe lobe{.albedo = albedo};
const Vector3r up{0.0, 0.0, 1.0};
const Vector3r tilted_out{normalized(Vector3r{0.3, -0.4, 0.8})};
const Vector3r tilted_in{normalized(Vector3r{-0.5, 0.2, -0.6})};
// In the tangent plane, as a view ray at a sphere's silhouette arrives.
const Vector3r grazing{1.0, 0.0, 0.0};
// Exact comparisons where the value is copied rather than computed; a zero
// tolerance keeps to the tolerance-only rule for floating-point checks.
constexpr double exact{0.0};

TEST(LambertLobeTest, evaluatesToAlbedoOverPiForReflection)
{
    expectNear(lobe.evaluate(up, tilted_out), albedo * inv_pi);
}

TEST(LambertLobeTest, evaluatesToBlackForTransmission)
{
    EXPECT_TRUE(isBlack(lobe.evaluate(up, tilted_in)));
}

TEST(LambertLobeTest, evaluatesToBlackWhenOutgoingIsBelowSurface)
{
    EXPECT_TRUE(isBlack(lobe.evaluate(tilted_in, tilted_out)));
}

TEST(LambertLobeTest, pdfIsCosineOverPiForReflection)
{
    EXPECT_NEAR(lobe.pdf(up, tilted_out), tilted_out.z * inv_pi, default_tolerance);
}

TEST(LambertLobeTest, pdfIsZeroForTransmission)
{
    EXPECT_NEAR(lobe.pdf(up, tilted_in), 0.0, exact);
}

TEST(LambertLobeTest, pdfIsZeroWhenOutgoingIsBelowSurface)
{
    EXPECT_NEAR(lobe.pdf(tilted_in, tilted_out), 0.0, exact);
}

TEST(LambertLobeTest, evaluatesToBlackForGrazingOutgoing)
{
    EXPECT_TRUE(isBlack(lobe.evaluate(grazing, up)));
}

TEST(LambertLobeTest, pdfIsZeroForGrazingOutgoing)
{
    EXPECT_NEAR(lobe.pdf(grazing, up), 0.0, exact);
}

TEST(LambertLobeTest, sampleReturnsNothingForGrazingOutgoing)
{
    EXPECT_FALSE(lobe.sample(grazing, {0.3, 0.6}).has_value());
}

TEST(LambertLobeTest, sampleReturnsNothingWhenOutgoingIsBelowSurface)
{
    EXPECT_FALSE(lobe.sample(tilted_in, {0.3, 0.6}).has_value());
}

TEST(LambertLobeTest, sampleWeightIsAlbedo)
{
    // The cosine-weighted pdf cancels f * cos(theta) exactly, so the weight is
    // the albedo itself rather than a ratio that could blow up near grazing.
    const ScatterSample sample{requireValue(lobe.sample(tilted_out, {0.3, 0.6}))};

    expectNear(sample.weight, albedo, exact);
}

TEST(LambertLobeTest, boundarySamplesAreAboveSurfaceWithPositivePdf)
{
    // Corners and edge midpoints of the square; some may map into the tangent
    // plane, and those must be dropped rather than returned with a zero pdf.
    for (const Point2r u :
         {Point2r{0.0, 0.0}, Point2r{1.0, 1.0}, Point2r{0.0, 1.0}, Point2r{1.0, 0.0},
          Point2r{0.5, 0.0}, Point2r{0.0, 0.5}, Point2r{1.0, 0.5}, Point2r{0.5, 1.0}}) {
        if (const auto sample{lobe.sample(up, u)}) {
            EXPECT_GT(sample->direction.z, 0.0);
            EXPECT_GT(sample->pdf, 0.0);
            expectNear(sample->weight, albedo, exact);
        }
    }
}

TEST(LambertLobeTest, sampleMatchesEvaluateAndPdf)
{
    const ScatterSample sample{requireValue(lobe.sample(tilted_out, {0.7, 0.2}))};

    EXPECT_NEAR(sample.pdf, lobe.pdf(tilted_out, sample.direction), default_tolerance);
    expectNear(lobe.evaluate(tilted_out, sample.direction) * sample.direction.z / sample.pdf,
               sample.weight);
}

TEST(LambertLobeTest, sampledDirectionsAreUnitAndAboveSurface)
{
    Pcg32 random{{.seed = 3, .stream = 1}};

    for (int i{0}; i < 10'000; ++i) {
        const Real ux{random.nextReal()};
        const Point2r u{.x = ux, .y = random.nextReal()};
        const ScatterSample sample{requireValue(lobe.sample(up, u))};
        ASSERT_NEAR(length(sample.direction), 1.0, 1e-9);
        ASSERT_GE(sample.direction.z, 0.0);
    }
}

TEST(LambertLobeTest, sampleDoesNotDependOnOutgoingDirection)
{
    const ScatterSample from_up{requireValue(lobe.sample(up, {0.4, 0.9}))};
    const ScatterSample from_tilted{requireValue(lobe.sample(tilted_out, {0.4, 0.9}))};

    expectNear(from_up.direction, from_tilted.direction);
}

} // namespace
} // namespace albedo
