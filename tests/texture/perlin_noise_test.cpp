#include "albedo/texture/perlin_noise.hpp"

#include "support/expect_near.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <limits>

namespace albedo {
namespace {

using test_support::exact_tolerance;

// Points spread over several lattice cells, on both sides of the origin, none
// of them on the lattice.
Point3r samplePoint(int index)
{
    const Real step{0.137};
    return {(index % 23) * step - 1.5, (index % 17) * step * 1.3 - 1.1,
            (index % 13) * step * 1.7 - 0.9};
}

constexpr int sample_count{2000};

TEST(PerlinNoiseTest, isZeroOnTheLattice)
{
    for (const Point3r point :
         {Point3r{0, 0, 0}, Point3r{1, 2, 3}, Point3r{-4, 7, -255}, Point3r{300, -1000, 12}}) {
        EXPECT_NEAR(perlinNoise(point), 0.0, exact_tolerance);
    }
}

TEST(PerlinNoiseTest, staysWithinMinusOneToOne)
{
    for (int index{0}; index < sample_count; ++index) {
        const Real noise{perlinNoise(samplePoint(index))};
        EXPECT_LE(std::abs(noise), 1.0) << "sample " << index;
    }
}

TEST(PerlinNoiseTest, variesBetweenLatticePoints)
{
    Real largest{0};
    for (int index{0}; index < sample_count; ++index) {
        largest = std::max(largest, std::abs(perlinNoise(samplePoint(index))));
    }

    EXPECT_GT(largest, 0.3);
}

TEST(PerlinNoiseTest, isContinuous)
{
    const Point3r point{0.42, 1.37, -2.71};
    const Vector3r nudge{1e-6, -1e-6, 1e-6};

    EXPECT_NEAR(perlinNoise(point + nudge), perlinNoise(point), 1e-4);
}

TEST(PerlinNoiseTest, isContinuousAcrossCellBoundaries)
{
    // Just below and just above x = 1, where the lattice cell changes.
    const Real below{perlinNoise({std::nextafter(1.0, 0.0), 0.3, 0.6})};
    const Real above{perlinNoise({1.0, 0.3, 0.6})};

    EXPECT_NEAR(below, above, 1e-9);
}

TEST(PerlinNoiseTest, isContinuousWhereNegativeCellsWrapAround)
{
    // Just below x = 0 lies cell -1, which wraps around to cell 255.
    const Real below{perlinNoise({std::nextafter(0.0, -1.0), 0.3, 0.6})};
    const Real above{perlinNoise({0.0, 0.3, 0.6})};

    EXPECT_NEAR(below, above, 1e-9);
}

TEST(PerlinNoiseTest, matchesTheReferenceImplementation)
{
    // The first is Perlin's published value for his Java reference code
    // (ImprovedNoise); the others, at negative coordinates that exercise the
    // wrap-around, come from a direct port of that code.
    EXPECT_NEAR(perlinNoise({3.14, 42, 7}), 0.13691995878400012, 1e-12);
    EXPECT_NEAR(perlinNoise({-0.3, -5.7, 2.25}), 0.40750939192218755, 1e-12);
    EXPECT_NEAR(perlinNoise({-300.6, 17.2, -0.45}), -0.11245507194489657, 1e-12);
}

TEST(PerlinNoiseTest, repeatsEvery256Cells)
{
    const Point3r point{0.42, 1.37, -2.71};

    EXPECT_NEAR(perlinNoise(point + Vector3r{256, -512, 256}), perlinNoise(point), 1e-9);
}

TEST(PerlinNoiseTest, isZeroForNonFiniteCoordinates)
{
    constexpr Real infinity{std::numeric_limits<Real>::infinity()};
    constexpr Real nan{std::numeric_limits<Real>::quiet_NaN()};

    EXPECT_NEAR(perlinNoise({infinity, 0.5, 0.5}), 0.0, exact_tolerance);
    EXPECT_NEAR(perlinNoise({0.5, -infinity, 0.5}), 0.0, exact_tolerance);
    EXPECT_NEAR(perlinNoise({0.5, 0.5, nan}), 0.0, exact_tolerance);
}

TEST(PerlinNoiseTest, staysFiniteForCoordinatesTooLargeForAnyInteger)
{
    // Converting the cell index straight to an integer would be undefined here.
    EXPECT_TRUE(std::isfinite(perlinNoise({1e300, -1e300, 0.5})));
}

TEST(PerlinTurbulenceTest, isZeroOnTheLattice)
{
    // Every octave doubles the frequency, so lattice points stay on the lattice.
    EXPECT_NEAR(turbulence({3, -2, 5}), 0.0, exact_tolerance);
}

TEST(PerlinTurbulenceTest, isNonNegativeAndBelowTwo)
{
    for (int index{0}; index < sample_count; ++index) {
        const Real value{turbulence(samplePoint(index))};
        EXPECT_GE(value, 0.0) << "sample " << index;
        EXPECT_LT(value, 2.0) << "sample " << index;
    }
}

TEST(PerlinTurbulenceTest, addsDetailToTheNoise)
{
    // The first octave alone is |noise|; the finer octaves change the value.
    const Point3r point{0.42, 1.37, -2.71};

    EXPECT_GT(std::abs(turbulence(point) - std::abs(perlinNoise(point))), 1e-6);
}

TEST(PerlinTurbulenceTest, isZeroForNonFiniteCoordinates)
{
    EXPECT_NEAR(turbulence({std::numeric_limits<Real>::quiet_NaN(), 0, 0}), 0.0, exact_tolerance);
}

} // namespace
} // namespace albedo
