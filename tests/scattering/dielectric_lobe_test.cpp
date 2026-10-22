#include "albedo/scattering/dielectric_lobe.hpp"

#include "albedo/core/vector3.hpp"
#include "albedo/sampling/pcg32.hpp"
#include "albedo/scattering/fresnel.hpp"
#include "albedo/scattering/specular.hpp"
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

constexpr Real glass{1.5};
const Vector3r up{0.0, 0.0, 1.0};
const Vector3r tilted_wo{normalized(Vector3r{0.5, -0.3, 0.6})};
const Vector3r below{normalized(Vector3r{-0.3, 0.4, -0.7})};

Rgb gray(Real value)
{
    return {value, value, value};
}

Vector3r flipZ(Vector3r w)
{
    return {w.x, w.y, -w.z};
}

Point2r nextPoint(Pcg32& random)
{
    const Real x{random.nextReal()};
    return {.x = x, .y = random.nextReal()};
}

// The integral of `f` over the whole sphere by the midpoint rule.
Real integrateSphere(const std::function<Real(Vector3r)>& f)
{
    constexpr int theta_steps{3000};
    constexpr int phi_steps{600};
    constexpr Real theta_step{pi / theta_steps};
    constexpr Real phi_step{2 * pi / phi_steps};
    Real sum{0};
    for (int i{0}; i < theta_steps; ++i) {
        const Real theta{(i + 0.5) * theta_step};
        for (int j{0}; j < phi_steps; ++j) {
            const Real phi{(j + 0.5) * phi_step};
            const Vector3r w{std::sin(theta) * std::cos(phi), std::sin(theta) * std::sin(phi),
                             std::cos(theta)};
            sum += f(w) * std::sin(theta);
        }
    }
    return sum * theta_step * phi_step;
}

// --- Smooth ---

TEST(DielectricLobeTest, smoothLobeReflectsWhenTheChoiceIsBelowFresnel)
{
    const DielectricLobe lobe{.eta = glass, .alpha = 0.0};

    const ScatterSample sample{requireValue(lobe.sample(tilted_wo, 0.0, {0.3, 0.6}))};

    expectNear(sample.direction, reflect(tilted_wo, up));
    expectNear(sample.weight, gray(1.0));
    EXPECT_NEAR(sample.pdf, fresnelDielectric(tilted_wo.z, glass), default_tolerance);
    EXPECT_TRUE(sample.specular);
}

TEST(DielectricLobeTest, smoothLobeRefractsWhenTheChoiceIsAboveFresnel)
{
    // Radiance leaving the glass towards wo spreads into a larger solid angle and
    // shrinks by 1 / eta^2, so the path entering the glass is weighted by that.
    const DielectricLobe lobe{.eta = glass, .alpha = 0.0};

    const ScatterSample sample{requireValue(lobe.sample(tilted_wo, 0.99, {0.3, 0.6}))};

    expectNear(sample.direction, requireValue(refract(tilted_wo, up, glass)));
    expectNear(sample.weight, gray(1 / (glass * glass)));
    EXPECT_NEAR(sample.pdf, 1 - fresnelDielectric(tilted_wo.z, glass), default_tolerance);
    EXPECT_TRUE(sample.specular);
}

TEST(DielectricLobeTest, smoothLobeAlwaysReflectsUnderTotalInternalReflection)
{
    const DielectricLobe lobe{.eta = 1 / glass, .alpha = 0.0};
    const Vector3r wo{std::sin(pi / 3), 0.0, std::cos(pi / 3)};

    const ScatterSample sample{requireValue(lobe.sample(wo, 0.99, {0.3, 0.6}))};

    expectNear(sample.direction, reflect(wo, up));
    EXPECT_NEAR(sample.pdf, 1.0, default_tolerance);
}

TEST(DielectricLobeTest, smoothMatchedIndicesPassStraightThrough)
{
    const ScatterSample sample{
        requireValue(DielectricLobe{.eta = 1.0, .alpha = 0.0}.sample(tilted_wo, 0.0, {0.3, 0.6}))};

    expectNear(sample.direction, -tilted_wo);
    expectNear(sample.weight, gray(1.0));
}

TEST(DielectricLobeTest, smoothLobeEvaluatesAndSamplesNoDensity)
{
    const DielectricLobe lobe{.eta = glass, .alpha = 0.0};
    const Vector3r refracted{requireValue(refract(tilted_wo, up, glass))};

    EXPECT_TRUE(isBlack(lobe.evaluate(tilted_wo, refracted)));
    EXPECT_NEAR(lobe.pdf(tilted_wo, refracted), 0.0, exact_tolerance);
}

TEST(DielectricLobeTest, smoothSampleReturnsNothingFromBelow)
{
    const DielectricLobe lobe{.eta = glass, .alpha = 0.0};

    EXPECT_FALSE(lobe.sample(below, 0.5, {0.3, 0.6}).has_value());
}

// --- Rough ---

TEST(DielectricLobeTest, roughReflectionMatchesTheMicrofacetModelAtTheMirrorDirection)
{
    // Looking straight down, the mirror direction's half vector is the normal:
    // f = F(1) D(n) G2 / 4 with D(n) = 1 / (pi alpha^2) and G2 = 1.
    constexpr Real alpha{0.3};
    const DielectricLobe lobe{.eta = glass, .alpha = alpha};

    expectNear(lobe.evaluate(up, up),
               gray(fresnelDielectric(1.0, glass) / (pi * alpha * alpha) / 4), 1e-9);
}

TEST(DielectricLobeTest, roughReflectionIsReciprocal)
{
    const DielectricLobe lobe{.eta = glass, .alpha = 0.4};
    const Vector3r wi{normalized(Vector3r{-0.2, 0.6, 0.5})};

    expectNear(lobe.evaluate(tilted_wo, wi), lobe.evaluate(wi, tilted_wo));
}

TEST(DielectricLobeTest, roughTransmissionObeysGeneralizedReciprocity)
{
    // For a radiance BTDF, f(wo, wi) / eta_wi^2 = f(wi, wo) / eta_wo^2 (Veach's
    // thesis, sec. 5.2). Seen from wi's side the normal points the other way
    // and the index ratio inverts.
    for (const Real eta : {glass, 1 / glass}) {
        SCOPED_TRACE(::testing::Message() << "eta " << eta);
        const DielectricLobe forward{.eta = eta, .alpha = 0.4};
        const DielectricLobe backward{.eta = 1 / eta, .alpha = 0.4};
        const Rgb f{forward.evaluate(tilted_wo, below)};
        ASSERT_FALSE(isBlack(f));
        expectNear(backward.evaluate(flipZ(below), flipZ(tilted_wo)), f * (eta * eta), 1e-9);
    }
}

// Checks every sample against `evaluate` and `pdf`, and that both reflection
// and transmission occur.
void expectSamplesMatchEvaluateAndPdf(Real eta)
{
    const DielectricLobe lobe{.eta = eta, .alpha = 0.4};
    Pcg32 random{{.seed = 41, .stream = 1}};
    int reflected{0};
    int transmitted{0};
    for (int i{0}; i < 2000; ++i) {
        const Real u_choice{random.nextReal()};
        const auto sample{lobe.sample(tilted_wo, u_choice, nextPoint(random))};
        if (!sample) {
            continue;
        }
        ++(sample->direction.z > 0 ? reflected : transmitted);
        ASSERT_FALSE(sample->specular);
        ASSERT_NEAR(sample->pdf, lobe.pdf(tilted_wo, sample->direction), 1e-9 * sample->pdf);
        const Real cos_i{std::abs(sample->direction.z)};
        expectNear(sample->weight,
                   lobe.evaluate(tilted_wo, sample->direction) * cos_i / sample->pdf, 1e-9);
    }
    EXPECT_GT(reflected, 0);
    EXPECT_GT(transmitted, 0);
}

TEST(DielectricLobeTest, roughSamplesMatchEvaluateAndPdfEnteringGlass)
{
    expectSamplesMatchEvaluateAndPdf(glass);
}

TEST(DielectricLobeTest, roughSamplesMatchEvaluateAndPdfLeavingGlass)
{
    expectSamplesMatchEvaluateAndPdf(1 / glass);
}

TEST(DielectricLobeTest, roughSamplingEstimatesTheDirectionalAlbedo)
{
    // Mean sample weight against quadrature of f |cos| over the whole sphere,
    // entering glass and leaving it.
    for (const Real eta : {glass, 1 / glass}) {
        SCOPED_TRACE(::testing::Message() << "eta " << eta);
        const DielectricLobe lobe{.eta = eta, .alpha = 0.4};
        const Real expected{integrateSphere(
            [&](Vector3r wi) { return lobe.evaluate(tilted_wo, wi).g * std::abs(wi.z); })};
        Pcg32 random{{.seed = 42, .stream = 1}};
        constexpr int sample_count{100'000};
        Real sum{0};
        for (int i{0}; i < sample_count; ++i) {
            const Real u_choice{random.nextReal()};
            if (const auto sample{lobe.sample(tilted_wo, u_choice, nextPoint(random))}) {
                sum += sample->weight.g;
            }
        }
        EXPECT_NEAR(sum / sample_count, expected, 0.005);
    }
}

TEST(DielectricLobeTest, roughPdfIntegratesToTheFractionOfSamplesKept)
{
    const DielectricLobe lobe{.eta = glass, .alpha = 0.4};
    const Real pdf_integral{integrateSphere([&](Vector3r wi) { return lobe.pdf(tilted_wo, wi); })};
    Pcg32 random{{.seed = 43, .stream = 1}};
    constexpr int sample_count{100'000};
    int kept{0};

    for (int i{0}; i < sample_count; ++i) {
        const Real u_choice{random.nextReal()};
        kept += lobe.sample(tilted_wo, u_choice, nextPoint(random)).has_value() ? 1 : 0;
    }

    EXPECT_LE(pdf_integral, 1.0 + 1e-3);
    EXPECT_NEAR(static_cast<Real>(kept) / sample_count, pdf_integral, 0.005);
}

// The fraction of samples from `wo` that are transmitted.
Real transmittedFraction(const DielectricLobe& lobe, Vector3r wo)
{
    Pcg32 random{{.seed = 44, .stream = 1}};
    constexpr int sample_count{10'000};
    int transmitted{0};
    for (int i{0}; i < sample_count; ++i) {
        const Real u_choice{random.nextReal()};
        if (const auto sample{lobe.sample(wo, u_choice, nextPoint(random))}) {
            transmitted += sample->direction.z < 0 ? 1 : 0;
        }
    }
    return static_cast<Real>(transmitted) / sample_count;
}

TEST(DielectricLobeTest, roughLobeRarelyTransmitsBeyondTheCriticalAngle)
{
    // At about 72 degrees from the normal, light entering glass is mostly
    // transmitted, but light leaving it is past the 41.8 degree critical angle:
    // only the rare microfacets in GGX's tail tilted 30 degrees or more towards
    // the viewer let any through.
    const Vector3r wo{normalized(Vector3r{0.95, 0.0, 0.3})};

    EXPECT_GT(transmittedFraction({.eta = glass, .alpha = 0.05}, wo), 0.5);
    EXPECT_LT(transmittedFraction({.eta = 1 / glass, .alpha = 0.05}, wo), 0.02);
}

TEST(DielectricLobeTest, roughLobeIsBlackFromBelow)
{
    const DielectricLobe lobe{.eta = glass, .alpha = 0.4};

    EXPECT_TRUE(isBlack(lobe.evaluate(below, tilted_wo)));
    EXPECT_NEAR(lobe.pdf(below, tilted_wo), 0.0, exact_tolerance);
    EXPECT_FALSE(lobe.sample(below, 0.5, {0.3, 0.6}).has_value());
}

TEST(DielectricLobeTest, roughLobeIsBlackInTheTangentPlane)
{
    const DielectricLobe lobe{.eta = glass, .alpha = 0.4};

    EXPECT_TRUE(isBlack(lobe.evaluate(tilted_wo, {1.0, 0.0, 0.0})));
    EXPECT_NEAR(lobe.pdf(tilted_wo, {1.0, 0.0, 0.0}), 0.0, exact_tolerance);
}

TEST(DielectricLobeTest, roughMatchedIndicesPassStraightThrough)
{
    // Without a change of index rough microfacets cannot deflect light either.
    const ScatterSample sample{
        requireValue(DielectricLobe{.eta = 1.0, .alpha = 0.4}.sample(tilted_wo, 0.0, {0.3, 0.6}))};

    expectNear(sample.direction, -tilted_wo);
    expectNear(sample.weight, gray(1.0));
    EXPECT_TRUE(sample.specular);
}

} // namespace
} // namespace albedo
