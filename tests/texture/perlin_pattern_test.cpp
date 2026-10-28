#include "albedo/texture/perlin_pattern.hpp"

#include "albedo/texture/perlin_noise.hpp"
#include "support/expect_near.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <limits>

namespace albedo {
namespace {

using test_support::default_tolerance;

TEST(PerlinPatternTest, cloudsAreZeroOnTheLattice)
{
    // Turbulence is zero on the noise lattice.
    constexpr PerlinPattern clouds{.scale = 1};

    EXPECT_NEAR(clouds.weightAt({2, -1, 4}), 0.0, default_tolerance);
}

TEST(PerlinPatternTest, scaleShrinksThePattern)
{
    // With scale 4, the point 0.25 away maps onto the lattice.
    constexpr PerlinPattern clouds{.scale = 4};

    EXPECT_NEAR(clouds.weightAt({0.25, 0.5, -0.75}), 0.0, default_tolerance);
}

TEST(PerlinPatternTest, cloudsFollowTheTurbulence)
{
    constexpr PerlinPattern clouds{.scale = 2};

    EXPECT_NEAR(clouds.weightAt({0.31, 0.77, -0.18}),
                std::min(turbulence({0.62, 1.54, -0.36}), Real{1}), default_tolerance);
}

TEST(PerlinPatternTest, veinsRunAcrossX)
{
    // On the lattice the turbulence is zero, leaving sin(scale * x): x = 0 is
    // halfway.
    constexpr PerlinPattern marble{.scale = 1, .veins = 5};

    EXPECT_NEAR(marble.weightAt({0, 3, 7}), 0.5, default_tolerance);
}

TEST(PerlinPatternTest, veinsAreWarpedByTheTurbulence)
{
    constexpr PerlinPattern marble{.scale = 1, .veins = 5};
    const Point3r point{0.31, 0.77, -0.18};

    EXPECT_NEAR(marble.weightAt(point), 0.5 * (1 + std::sin(0.31 + 5 * turbulence(point))),
                default_tolerance);
}

TEST(PerlinPatternTest, weightsStayBetweenZeroAndOne)
{
    constexpr PerlinPattern clouds{.scale = 3};
    constexpr PerlinPattern marble{.scale = 3, .veins = 8};

    for (int index{0}; index < 500; ++index) {
        const Real step{0.0731};
        const Point3r point{index * step, index * step * 0.7, -index * step};
        for (const Real weight : {clouds.weightAt(point), marble.weightAt(point)}) {
            EXPECT_GE(weight, 0.0) << "sample " << index;
            EXPECT_LE(weight, 1.0) << "sample " << index;
        }
    }
}

TEST(PerlinPatternTest, nonFinitePointsGiveADefinedWeight)
{
    // Turbulence is zero there, so clouds give 0, and the veins' sine of an
    // infinite or NaN argument is replaced by the midpoint.
    constexpr Real nan{std::numeric_limits<Real>::quiet_NaN()};
    constexpr PerlinPattern clouds{.scale = 1};
    constexpr PerlinPattern marble{.scale = 1, .veins = 5};

    EXPECT_NEAR(clouds.weightAt({nan, 0, 0}), 0.0, default_tolerance);
    EXPECT_NEAR(marble.weightAt({nan, 0, 0}), 0.5, default_tolerance);
}

Real weightWith(Real scale, Real veins)
{
    const PerlinPattern pattern{.scale = scale, .veins = veins};
    return pattern.weightAt({0.3, 0.1, 0.7});
}

TEST(PerlinPatternDeathTest, rejectsZeroScale)
{
    EXPECT_DEBUG_DEATH(static_cast<void>(weightWith(0.0, 0.0)), "");
}

TEST(PerlinPatternDeathTest, rejectsNegativeScale)
{
    EXPECT_DEBUG_DEATH(static_cast<void>(weightWith(-1.0, 0.0)), "");
}

TEST(PerlinPatternDeathTest, rejectsNanScale)
{
    EXPECT_DEBUG_DEATH(static_cast<void>(weightWith(std::numeric_limits<Real>::quiet_NaN(), 0.0)),
                       "");
}

TEST(PerlinPatternDeathTest, rejectsInfiniteScale)
{
    EXPECT_DEBUG_DEATH(static_cast<void>(weightWith(std::numeric_limits<Real>::infinity(), 0.0)),
                       "");
}

TEST(PerlinPatternDeathTest, rejectsNegativeVeins)
{
    EXPECT_DEBUG_DEATH(static_cast<void>(weightWith(1.0, -1.0)), "");
}

TEST(PerlinPatternDeathTest, rejectsNanVeins)
{
    EXPECT_DEBUG_DEATH(static_cast<void>(weightWith(1.0, std::numeric_limits<Real>::quiet_NaN())),
                       "");
}

TEST(PerlinPatternDeathTest, rejectsInfiniteVeins)
{
    EXPECT_DEBUG_DEATH(static_cast<void>(weightWith(1.0, std::numeric_limits<Real>::infinity())),
                       "");
}

} // namespace
} // namespace albedo
