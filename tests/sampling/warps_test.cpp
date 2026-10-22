#include "albedo/sampling/warps.hpp"

#include "albedo/sampling/pcg32.hpp"
#include "support/expect_near.hpp"

#include <gtest/gtest.h>

#include <cmath>

namespace albedo {
namespace {

using test_support::default_tolerance;
using test_support::expectNear;

// Statistical tests draw this many samples from a fixed seed. Tolerances are
// several standard deviations wide (e.g. ~7 sigma for a fraction near 1/4).
constexpr int sample_count{100'000};

void expectNear2D(Point2r actual, Point2r expected)
{
    EXPECT_NEAR(actual.x, expected.x, default_tolerance);
    EXPECT_NEAR(actual.y, expected.y, default_tolerance);
}

Point2r nextPoint(Pcg32& random)
{
    const Real x{random.nextReal()};
    return {.x = x, .y = random.nextReal()};
}

TEST(WarpsTest, concentricDiskMapsCenterToOrigin)
{
    const Point2r point{sampleConcentricDisk({0.5, 0.5})};

    EXPECT_NEAR(point.x, 0.0, default_tolerance);
    EXPECT_NEAR(point.y, 0.0, default_tolerance);
}

TEST(WarpsTest, concentricDiskMapsEdgeMidpointsToAxes)
{
    expectNear2D(sampleConcentricDisk({1.0, 0.5}), {1.0, 0.0});
    expectNear2D(sampleConcentricDisk({0.5, 1.0}), {0.0, 1.0});
    expectNear2D(sampleConcentricDisk({0.0, 0.5}), {-1.0, 0.0});
    expectNear2D(sampleConcentricDisk({0.5, 0.0}), {0.0, -1.0});
}

TEST(WarpsTest, concentricDiskMapsCornersToDiagonals)
{
    // Covers offset_x == offset_y and offset_x == -offset_y at the four corners.
    const Real s{std::sqrt(0.5)};

    expectNear2D(sampleConcentricDisk({1.0, 1.0}), {s, s});
    expectNear2D(sampleConcentricDisk({0.0, 0.0}), {-s, -s});
    expectNear2D(sampleConcentricDisk({0.0, 1.0}), {-s, s});
    expectNear2D(sampleConcentricDisk({1.0, 0.0}), {s, -s});
}

TEST(WarpsTest, concentricDiskMapsOffAxisPointsToTheirAngles)
{
    // One point per branch of the map, away from axes and diagonals, so a sign
    // error in either angle formula (which would still cover the disk evenly) shows.
    expectNear2D(sampleConcentricDisk({1.0, 0.75}), {std::cos(pi / 8), std::sin(pi / 8)});
    expectNear2D(sampleConcentricDisk({0.75, 1.0}), {std::cos(3 * pi / 8), std::sin(3 * pi / 8)});
}

TEST(WarpsTest, concentricDiskStaysInsideUnitDisk)
{
    Pcg32 random{{.seed = 1, .stream = 1}};

    for (int i{0}; i < sample_count; ++i) {
        const Point2r point{sampleConcentricDisk(nextPoint(random))};
        ASSERT_LE(point.x * point.x + point.y * point.y, 1.0 + default_tolerance);
    }
}

TEST(WarpsTest, concentricDiskIsUniformInArea)
{
    // The inner disk of radius 1/2 has a quarter of the area.
    Pcg32 random{{.seed = 2, .stream = 1}};
    int inside_half_radius{0};

    for (int i{0}; i < sample_count; ++i) {
        const Point2r point{sampleConcentricDisk(nextPoint(random))};
        inside_half_radius += point.x * point.x + point.y * point.y < 0.25 ? 1 : 0;
    }

    EXPECT_NEAR(static_cast<Real>(inside_half_radius) / sample_count, 0.25, 0.01);
}

TEST(WarpsTest, cosineHemisphereGivesUnitVectorsAboveTheSurface)
{
    Pcg32 random{{.seed = 3, .stream = 1}};

    for (int i{0}; i < sample_count; ++i) {
        const Vector3r direction{sampleCosineHemisphere(nextPoint(random))};
        ASSERT_NEAR(length(direction), 1.0, 1e-12);
        ASSERT_GE(direction.z, 0.0);
    }
}

TEST(WarpsTest, cosineHemisphereOnSquareBoundaryLiesInTheSurface)
{
    // Boundary inputs map to the disk's rim, i.e. z = 0 where the pdf is 0.
    const Vector3r direction{sampleCosineHemisphere({0.0, 0.5})};

    EXPECT_NEAR(length(direction), 1.0, default_tolerance);
    EXPECT_NEAR(direction.z, 0.0, 1e-7);
    EXPECT_GE(direction.z, 0.0);
}

TEST(WarpsTest, cosineHemisphereIsBalancedAroundTheNormal)
{
    // Catches mirrored or folded quadrants, which radial tests cannot see.
    Pcg32 random{{.seed = 9, .stream = 1}};
    Vector3r sum{};

    for (int i{0}; i < sample_count; ++i) {
        sum += sampleCosineHemisphere(nextPoint(random));
    }

    EXPECT_NEAR(sum.x / sample_count, 0.0, 0.01);
    EXPECT_NEAR(sum.y / sample_count, 0.0, 0.01);
}

TEST(WarpsTest, cosineHemisphereFollowsCosineDistribution)
{
    // Under a cos(theta)/pi density, P(cos(theta) <= c) = c^2, so cos^2 is uniform:
    // a quarter of the directions have cos(theta) below 1/2.
    Pcg32 random{{.seed = 4, .stream = 1}};
    int below_half{0};

    for (int i{0}; i < sample_count; ++i) {
        below_half += sampleCosineHemisphere(nextPoint(random)).z < 0.5 ? 1 : 0;
    }

    EXPECT_NEAR(static_cast<Real>(below_half) / sample_count, 0.25, 0.01);
}

TEST(WarpsTest, cosineHemispherePdfMatchesFormula)
{
    EXPECT_NEAR(cosineHemispherePdf(1.0), 1.0 / pi, default_tolerance);
    EXPECT_NEAR(cosineHemispherePdf(0.5), 0.5 / pi, default_tolerance);
}

TEST(WarpsTest, cosineHemispherePdfIsZeroBelowTheSurface)
{
    EXPECT_NEAR(cosineHemispherePdf(-0.3), 0.0, default_tolerance);
}

TEST(WarpsTest, cosineHemispherePdfIsZeroInTheSurface)
{
    EXPECT_NEAR(cosineHemispherePdf(0.0), 0.0, default_tolerance);
}

TEST(WarpsTest, cosineHemispherePdfIntegratesToOne)
{
    // Monte Carlo integral over the sphere with uniform samples: E[pdf / (1/4pi)] = 1.
    Pcg32 random{{.seed = 5, .stream = 1}};
    Real sum{0};

    for (int i{0}; i < sample_count; ++i) {
        const Vector3r direction{sampleUniformSphere(nextPoint(random))};
        sum += cosineHemispherePdf(direction.z) / uniformSpherePdf();
    }

    EXPECT_NEAR(sum / sample_count, 1.0, 0.02);
}

TEST(WarpsTest, uniformSphereGivesUnitVectors)
{
    Pcg32 random{{.seed = 6, .stream = 1}};

    for (int i{0}; i < sample_count; ++i) {
        ASSERT_NEAR(length(sampleUniformSphere(nextPoint(random))), 1.0, 1e-12);
    }
}

TEST(WarpsTest, uniformSphereCoversBothHemispheresEqually)
{
    Pcg32 random{{.seed = 7, .stream = 1}};
    int upper{0};

    for (int i{0}; i < sample_count; ++i) {
        upper += sampleUniformSphere(nextPoint(random)).z > 0 ? 1 : 0;
    }

    EXPECT_NEAR(static_cast<Real>(upper) / sample_count, 0.5, 0.01);
}

TEST(WarpsTest, uniformSphereIsUniformInHeight)
{
    // Uniform area means z is uniform, so |z| < 1/2 holds half the time; sampling
    // the polar angle uniformly instead would give only 1/3.
    Pcg32 random{{.seed = 10, .stream = 1}};
    int near_equator{0};

    for (int i{0}; i < sample_count; ++i) {
        near_equator += std::abs(sampleUniformSphere(nextPoint(random)).z) < 0.5 ? 1 : 0;
    }

    EXPECT_NEAR(static_cast<Real>(near_equator) / sample_count, 0.5, 0.01);
}

TEST(WarpsTest, uniformSphereHasZeroMeanDirection)
{
    Pcg32 random{{.seed = 8, .stream = 1}};
    Vector3r sum{};

    for (int i{0}; i < sample_count; ++i) {
        sum += sampleUniformSphere(nextPoint(random));
    }

    expectNear(sum / static_cast<Real>(sample_count), Vector3r{}, 0.01);
}

TEST(WarpsTest, uniformSpherePdfIsOneOverFourPi)
{
    EXPECT_NEAR(uniformSpherePdf(), 1.0 / (4.0 * pi), default_tolerance);
}

} // namespace
} // namespace albedo
