#include "albedo/scattering/conductor_lobe.hpp"

#include "albedo/core/vector3.hpp"
#include "albedo/sampling/pcg32.hpp"
#include "albedo/scattering/fresnel.hpp"
#include "support/expect_near.hpp"
#include "support/require_value.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <functional>

namespace albedo {
namespace {

using test_support::default_tolerance;
using test_support::exact_tolerance;
using test_support::expectNear;
using test_support::requireValue;

// Gold-like indices; the tests need plausible values, not measured ones.
constexpr Rgb gold_eta{0.18, 0.42, 1.37};
constexpr Rgb gold_k{3.42, 2.35, 1.77};
const Vector3r up{0.0, 0.0, 1.0};
const Vector3r tilted_wo{normalized(Vector3r{0.5, -0.3, 0.6})};

ConductorLobe roughGold(Real alpha)
{
    return {.eta = gold_eta, .k = gold_k, .alpha = alpha};
}

Point2r nextPoint(Pcg32& random)
{
    const Real x{random.nextReal()};
    return {.x = x, .y = random.nextReal()};
}

// The integral of `f` over the upper hemisphere by the midpoint rule.
Rgb integrateHemisphere(const std::function<Rgb(Vector3r)>& f)
{
    constexpr int theta_steps{1500};
    constexpr int phi_steps{600};
    constexpr Real theta_step{pi / 2 / theta_steps};
    constexpr Real phi_step{2 * pi / phi_steps};
    Rgb sum{};
    for (int i{0}; i < theta_steps; ++i) {
        const Real theta{(i + 0.5) * theta_step};
        for (int j{0}; j < phi_steps; ++j) {
            const Real phi{(j + 0.5) * phi_step};
            const Vector3r w{std::sin(theta) * std::cos(phi), std::sin(theta) * std::sin(phi),
                             std::cos(theta)};
            sum += f(w) * std::sin(theta);
        }
    }
    return sum * (theta_step * phi_step);
}

TEST(ConductorLobeTest, roughEvaluateIsReciprocal)
{
    const ConductorLobe lobe{roughGold(0.4)};
    const Vector3r wi{normalized(Vector3r{-0.2, 0.6, 0.5})};

    expectNear(lobe.evaluate(tilted_wo, wi), lobe.evaluate(wi, tilted_wo));
}

TEST(ConductorLobeTest, roughEvaluateIsBlackForTransmission)
{
    EXPECT_TRUE(isBlack(roughGold(0.4).evaluate(tilted_wo, {0.0, 0.6, -0.8})));
}

TEST(ConductorLobeTest, roughEvaluateIsBlackFromBelow)
{
    EXPECT_TRUE(isBlack(roughGold(0.4).evaluate({0.0, 0.6, -0.8}, tilted_wo)));
}

TEST(ConductorLobeTest, roughPdfIsZeroForTransmission)
{
    EXPECT_NEAR(roughGold(0.4).pdf(tilted_wo, {0.0, 0.6, -0.8}), 0.0, exact_tolerance);
}

TEST(ConductorLobeTest, roughPdfIsZeroFromBelow)
{
    EXPECT_NEAR(roughGold(0.4).pdf({0.0, 0.6, -0.8}, tilted_wo), 0.0, exact_tolerance);
}

TEST(ConductorLobeTest, roughEvaluateMatchesTheMicrofacetModelAtTheMirrorDirection)
{
    // Looking straight down, the mirror direction's half vector is the normal:
    // f = F(1) D(n) G2 / 4, with D(n) = 1 / (pi alpha^2) and G2 = 1 there.
    constexpr Real alpha{0.3};

    expectNear(roughGold(alpha).evaluate(up, up),
               fresnelConductor(1.0, gold_eta, gold_k) * (1 / (pi * alpha * alpha) / 4), 1e-9);
}

TEST(ConductorLobeTest, roughSampleWeightMatchesEvaluateAndPdf)
{
    const ConductorLobe lobe{roughGold(0.4)};
    Pcg32 random{{.seed = 31, .stream = 1}};
    int kept{0};

    for (int i{0}; i < 1000; ++i) {
        const auto sample{lobe.sample(tilted_wo, 0.5, nextPoint(random))};
        if (!sample) {
            continue;
        }
        ++kept;
        ASSERT_FALSE(sample->specular);
        ASSERT_GT(sample->direction.z, 0.0);
        ASSERT_NEAR(sample->pdf, lobe.pdf(tilted_wo, sample->direction), 1e-9 * sample->pdf);
        expectNear(sample->weight,
                   lobe.evaluate(tilted_wo, sample->direction) * sample->direction.z / sample->pdf,
                   1e-9);
    }
    // Most samples must be kept, or the checks above prove little; at this alpha
    // and viewing angle about 87% are (the rest reflect below the surface).
    EXPECT_GT(kept, 800);
}

TEST(ConductorLobeTest, roughSamplingEstimatesTheDirectionalAlbedo)
{
    // The mean sample weight is a Monte Carlo estimate of the integral of
    // f cos over the hemisphere, which quadrature computes independently.
    for (const Real alpha : {0.3, 0.7}) {
        SCOPED_TRACE(::testing::Message() << "alpha " << alpha);
        const ConductorLobe lobe{roughGold(alpha)};
        const Rgb expected{
            integrateHemisphere([&](Vector3r wi) { return lobe.evaluate(tilted_wo, wi) * wi.z; })};
        Pcg32 random{{.seed = 32, .stream = 1}};
        constexpr int sample_count{100'000};
        Rgb sum{};
        for (int i{0}; i < sample_count; ++i) {
            if (const auto sample{lobe.sample(tilted_wo, 0.5, nextPoint(random))}) {
                sum += sample->weight;
            }
        }
        expectNear(sum / sample_count, expected, 0.005);
    }
}

TEST(ConductorLobeTest, roughPdfIntegratesToTheFractionOfSamplesKept)
{
    // Samples whose reflection points below the surface are dropped, so the pdf
    // over the upper hemisphere integrates to the fraction that is kept.
    const ConductorLobe lobe{roughGold(0.7)};
    const Real pdf_integral{integrateHemisphere([&](Vector3r wi) {
                                const Real density{lobe.pdf(tilted_wo, wi)};
                                return Rgb{density, density, density};
                            }).r};
    Pcg32 random{{.seed = 33, .stream = 1}};
    constexpr int sample_count{100'000};
    int kept{0};

    for (int i{0}; i < sample_count; ++i) {
        kept += lobe.sample(tilted_wo, 0.5, nextPoint(random)).has_value() ? 1 : 0;
    }

    EXPECT_LT(pdf_integral, 1.0);
    EXPECT_NEAR(static_cast<Real>(kept) / sample_count, pdf_integral, 0.005);
}

TEST(ConductorLobeTest, roughSampleReturnsNothingFromBelow)
{
    EXPECT_FALSE(roughGold(0.4).sample({0.0, 0.6, -0.8}, 0.5, {0.3, 0.6}).has_value());
}

TEST(ConductorLobeTest, roughSampleReturnsNothingAtGrazing)
{
    EXPECT_FALSE(roughGold(0.4).sample({1.0, 0.0, 0.0}, 0.5, {0.3, 0.6}).has_value());
}

TEST(ConductorLobeTest, smoothLobeSamplesTheMirrorDirection)
{
    const ConductorLobe lobe{roughGold(0.0)};

    const ScatterSample sample{requireValue(lobe.sample(tilted_wo, 0.5, {0.3, 0.6}))};

    expectNear(sample.direction, Vector3r{-tilted_wo.x, -tilted_wo.y, tilted_wo.z});
    EXPECT_TRUE(sample.specular);
    EXPECT_NEAR(sample.pdf, 1.0, exact_tolerance);
}

TEST(ConductorLobeTest, smoothLobeWeightIsTheFresnelReflectance)
{
    const ScatterSample sample{requireValue(roughGold(0.0).sample(tilted_wo, 0.5, {0.3, 0.6}))};

    expectNear(sample.weight, fresnelConductor(tilted_wo.z, gold_eta, gold_k));
}

TEST(ConductorLobeTest, smoothLobeEvaluatesAndSamplesNoDensity)
{
    // A mirror reflects into a single direction: no finite f or pdf describes it.
    const ConductorLobe lobe{roughGold(0.0)};
    const Vector3r mirror{-tilted_wo.x, -tilted_wo.y, tilted_wo.z};

    EXPECT_TRUE(isBlack(lobe.evaluate(tilted_wo, mirror)));
    EXPECT_NEAR(lobe.pdf(tilted_wo, mirror), 0.0, exact_tolerance);
}

TEST(ConductorLobeTest, tinyAlphaCountsAsSmooth)
{
    const ScatterSample sample{requireValue(roughGold(1e-6).sample(tilted_wo, 0.5, {0.3, 0.6}))};

    EXPECT_TRUE(sample.specular);
}

TEST(ConductorLobeTest, smoothSampleReturnsNothingFromBelow)
{
    EXPECT_FALSE(roughGold(0.0).sample({0.0, 0.6, -0.8}, 0.5, {0.3, 0.6}).has_value());
}

} // namespace
} // namespace albedo
