#include "albedo/scattering/ggx_distribution.hpp"

#include "albedo/core/vector3.hpp"
#include "albedo/sampling/pcg32.hpp"
#include "support/expect_near.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <functional>
#include <limits>

namespace albedo {
namespace {

using test_support::default_tolerance;

const Vector3r up{0.0, 0.0, 1.0};
// Tilted and rotated about z, so errors in either angle of a sampled normal show.
const Vector3r tilted_wo{normalized(Vector3r{0.5, -0.3, 0.6})};

// The direction with polar angle `theta` and azimuth `phi` about +z.
Vector3r sphericalDirection(Real theta, Real phi)
{
    return {std::sin(theta) * std::cos(phi), std::sin(theta) * std::sin(phi), std::cos(theta)};
}

// The integral of `f` over the upper hemisphere by the midpoint rule; fine
// enough in theta to resolve lobes as narrow as alpha = 0.2.
Real integrateHemisphere(const std::function<Real(Vector3r)>& f)
{
    constexpr int theta_steps{2000};
    constexpr int phi_steps{400};
    constexpr Real theta_step{pi / 2 / theta_steps};
    constexpr Real phi_step{2 * pi / phi_steps};
    Real sum{0};
    for (int i{0}; i < theta_steps; ++i) {
        const Real theta{(i + 0.5) * theta_step};
        for (int j{0}; j < phi_steps; ++j) {
            const Real phi{(j + 0.5) * phi_step};
            sum += f(sphericalDirection(theta, phi)) * std::sin(theta);
        }
    }
    return sum * theta_step * phi_step;
}

Point2r nextPoint(Pcg32& random)
{
    const Real x{random.nextReal()};
    return {.x = x, .y = random.nextReal()};
}

TEST(GgxDistributionTest, densityAtTheNormalIsOneOverPiAlphaSquared)
{
    const GgxDistribution ggx{0.3};

    EXPECT_NEAR(ggx.density(up), 1 / (pi * 0.09), 1e-9);
}

TEST(GgxDistributionTest, densityIsZeroBelowTheSurface)
{
    const GgxDistribution ggx{0.3};

    EXPECT_NEAR(ggx.density({0.0, 0.6, -0.8}), 0.0, test_support::exact_tolerance);
}

TEST(GgxDistributionTest, projectedDensityIntegratesToOne)
{
    // Microfacets cover exactly the macrosurface's projected area (Walter et
    // al. 2007, eq. 4).
    for (const Real alpha : {0.2, 0.5, 1.0}) {
        SCOPED_TRACE(::testing::Message() << "alpha " << alpha);
        const GgxDistribution ggx{alpha};
        EXPECT_NEAR(integrateHemisphere([&](Vector3r m) { return ggx.density(m) * m.z; }), 1.0,
                    1e-3);
    }
}

TEST(GgxDistributionTest, maskingIsOneAtNormalIncidence)
{
    EXPECT_NEAR(GgxDistribution{0.7}.masking(up), 1.0, default_tolerance);
}

TEST(GgxDistributionTest, maskingMatchesSmithGgxAtSixtyDegrees)
{
    // With alpha = 1 and tan^2(theta) = 3, Lambda = (sqrt(1 + 3) - 1) / 2 = 1/2.
    const Vector3r w{sphericalDirection(pi / 3, 0.4)};

    EXPECT_NEAR(GgxDistribution{1.0}.masking(w), 2.0 / 3.0, 1e-12);
}

TEST(GgxDistributionTest, maskingIsZeroAtGrazing)
{
    EXPECT_NEAR(GgxDistribution{0.5}.masking({1.0, 0.0, 0.0}), 0.0, default_tolerance);
}

TEST(GgxDistributionTest, maskingIsTheSameFromBelow)
{
    const GgxDistribution ggx{0.4};

    EXPECT_NEAR(ggx.masking({0.6, 0.0, -0.8}), ggx.masking({0.6, 0.0, 0.8}), default_tolerance);
}

TEST(GgxDistributionTest, maskingShadowingIsSymmetricAndBelowEitherMasking)
{
    const GgxDistribution ggx{0.6};
    const Vector3r other{normalized(Vector3r{-0.7, 0.2, 0.3})};

    const Real g2{ggx.maskingShadowing(tilted_wo, other)};

    EXPECT_NEAR(g2, ggx.maskingShadowing(other, tilted_wo), default_tolerance);
    EXPECT_LE(g2, ggx.masking(tilted_wo));
    EXPECT_LE(g2, ggx.masking(other));
}

TEST(GgxDistributionTest, visibleNormalPdfIntegratesToOne)
{
    for (const Real alpha : {0.2, 0.5, 1.0}) {
        SCOPED_TRACE(::testing::Message() << "alpha " << alpha);
        const GgxDistribution ggx{alpha};
        EXPECT_NEAR(
            integrateHemisphere([&](Vector3r m) { return ggx.visibleNormalPdf(tilted_wo, m); }),
            1.0, 1e-3);
    }
}

TEST(GgxDistributionTest, visibleNormalPdfIsZeroForBackFacingNormals)
{
    // A normal facing away from the viewer cannot be seen.
    const GgxDistribution ggx{0.5};
    const Vector3r back_facing{normalized(Vector3r{-0.9, 0.5, 0.2})};

    ASSERT_LT(dot(tilted_wo, back_facing), 0.0);
    EXPECT_NEAR(ggx.visibleNormalPdf(tilted_wo, back_facing), 0.0, test_support::exact_tolerance);
}

TEST(GgxDistributionTest, sampledNormalsAreUnitAndVisible)
{
    const GgxDistribution ggx{0.5};
    Pcg32 random{{.seed = 21, .stream = 1}};

    for (int i{0}; i < 10'000; ++i) {
        const Vector3r m{ggx.sampleVisibleNormal(tilted_wo, nextPoint(random))};
        ASSERT_NEAR(length(m), 1.0, 1e-9);
        ASSERT_GE(m.z, 0.0);
        ASSERT_GE(dot(tilted_wo, m), -1e-12);
    }
}

// Checks that the mean of each component of sampled normals matches its
// expectation under the visible-normal pdf, computed by quadrature. Component
// standard deviations are at most 0.5, so for 100k samples the tolerance is
// at least 3.8 standard errors.
void expectSampleMeansMatchThePdf(Real alpha, Vector3r wo, std::uint64_t seed)
{
    const GgxDistribution ggx{alpha};
    const auto expected_mean{[&](const std::function<Real(Vector3r)>& component) {
        return integrateHemisphere(
            [&](Vector3r m) { return component(m) * ggx.visibleNormalPdf(wo, m); });
    }};
    Pcg32 random{{.seed = seed, .stream = 1}};
    constexpr int sample_count{100'000};
    Vector3r sum{};

    for (int i{0}; i < sample_count; ++i) {
        sum += ggx.sampleVisibleNormal(wo, nextPoint(random));
    }

    EXPECT_NEAR(sum.x / sample_count, expected_mean([](Vector3r m) { return m.x; }), 0.006);
    EXPECT_NEAR(sum.y / sample_count, expected_mean([](Vector3r m) { return m.y; }), 0.006);
    EXPECT_NEAR(sum.z / sample_count, expected_mean([](Vector3r m) { return m.z; }), 0.006);
}

TEST(GgxDistributionTest, sampledNormalsFollowTheVisibleNormalPdf)
{
    expectSampleMeansMatchThePdf(0.5, tilted_wo, 22);
}

TEST(GgxDistributionTest, sampledNormalsFollowTheVisibleNormalPdfForASmoothSurface)
{
    expectSampleMeansMatchThePdf(0.1, tilted_wo, 24);
}

TEST(GgxDistributionTest, sampledNormalsFollowTheVisibleNormalPdfNearGrazing)
{
    // Seen from near grazing, the visible normals lean strongly towards the viewer.
    expectSampleMeansMatchThePdf(0.5, normalized(Vector3r{0.8, 0.6, 0.05}), 25);
}

TEST(GgxDistributionTest, visibleNormalPdfIsZeroFromBelowTheSurface)
{
    EXPECT_NEAR(GgxDistribution{0.5}.visibleNormalPdf({0.6, 0.0, -0.8}, up), 0.0,
                test_support::exact_tolerance);
}

TEST(GgxDistributionTest, samplingAtNormalIncidenceIsBalanced)
{
    // Looking straight down takes the sampler's degenerate-frame branch.
    const GgxDistribution ggx{0.5};
    Pcg32 random{{.seed = 23, .stream = 1}};
    constexpr int sample_count{100'000};
    Vector3r sum{};

    for (int i{0}; i < sample_count; ++i) {
        const Vector3r m{ggx.sampleVisibleNormal(up, nextPoint(random))};
        ASSERT_NEAR(length(m), 1.0, 1e-9);
        sum += m;
    }

    EXPECT_NEAR(sum.x / sample_count, 0.0, 0.006);
    EXPECT_NEAR(sum.y / sample_count, 0.0, 0.006);
}

TEST(GgxDistributionTest, samplingHandlesTheSquareBoundary)
{
    const GgxDistribution ggx{0.5};

    for (const Point2r u :
         {Point2r{0.0, 0.0}, Point2r{1.0, 1.0}, Point2r{0.0, 1.0}, Point2r{1.0, 0.0}}) {
        const Vector3r m{ggx.sampleVisibleNormal(tilted_wo, u)};
        EXPECT_NEAR(length(m), 1.0, 1e-9);
        EXPECT_GE(m.z, 0.0);
    }
}

TEST(GgxDistributionDeathTest, rejectsNonPositiveAlpha)
{
    EXPECT_DEBUG_DEATH(static_cast<void>(GgxDistribution{0.0}), "");
}

TEST(GgxDistributionDeathTest, rejectsNanAlpha)
{
    EXPECT_DEBUG_DEATH(static_cast<void>(GgxDistribution{std::numeric_limits<Real>::quiet_NaN()}),
                       "");
}

TEST(GgxDistributionDeathTest, rejectsNonUnitNormal)
{
    EXPECT_DEBUG_DEATH(static_cast<void>(GgxDistribution{0.5}.density({0.0, 0.0, 2.0})), "");
}

TEST(GgxDistributionDeathTest, rejectsSamplingFromBelowTheSurface)
{
    EXPECT_DEBUG_DEATH(
        static_cast<void>(GgxDistribution{0.5}.sampleVisibleNormal({0.6, 0.0, -0.8}, {0.5, 0.5})),
        "");
}

} // namespace
} // namespace albedo
