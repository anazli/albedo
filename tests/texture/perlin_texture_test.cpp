#include "albedo/texture/perlin_texture.hpp"

#include "albedo/texture/perlin_noise.hpp"
#include "support/expect_near.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <limits>

namespace albedo {
namespace {

using test_support::default_tolerance;
using test_support::expectNear;

constexpr Rgb low_color{0.05, 0.1, 0.4};
constexpr Rgb high_color{0.9, 0.95, 1.0};

HitRecord hitAt(Point3r point)
{
    return HitRecord{.point = point};
}

TEST(PerlinTextureTest, cloudsAreTheLowColorOnTheLattice)
{
    // Turbulence is zero on the noise lattice.
    constexpr PerlinTexture clouds{.low = low_color, .high = high_color, .scale = 1};

    expectNear(clouds.evaluate(hitAt({2, -1, 4})), low_color);
}

TEST(PerlinTextureTest, scaleShrinksThePattern)
{
    // With scale 4, the point 0.25 away maps onto the lattice.
    constexpr PerlinTexture clouds{.low = low_color, .high = high_color, .scale = 4};

    expectNear(clouds.evaluate(hitAt({0.25, 0.5, -0.75})), low_color);
}

TEST(PerlinTextureTest, cloudsFollowTheTurbulence)
{
    constexpr PerlinTexture clouds{.low = Rgb{0, 0, 0}, .high = Rgb{1, 1, 1}, .scale = 2};
    const Point3r point{0.31, 0.77, -0.18};
    const Real expected{std::min(turbulence({0.62, 1.54, -0.36}), Real{1})};

    expectNear(clouds.evaluate(hitAt(point)), Rgb{expected, expected, expected});
}

TEST(PerlinTextureTest, veinsRunAcrossX)
{
    // On the lattice the turbulence is zero, leaving sin(scale * x): x = 0 is
    // halfway between the colors.
    constexpr PerlinTexture marble{
        .low = Rgb{0, 0, 0}, .high = Rgb{1, 1, 1}, .scale = 1, .veins = 5};

    expectNear(marble.evaluate(hitAt({0, 3, 7})), Rgb{0.5, 0.5, 0.5});
}

TEST(PerlinTextureTest, veinsAreWarpedByTheTurbulence)
{
    constexpr PerlinTexture marble{
        .low = Rgb{0, 0, 0}, .high = Rgb{1, 1, 1}, .scale = 1, .veins = 5};
    const Point3r point{0.31, 0.77, -0.18};
    const Real expected{0.5 * (1 + std::sin(0.31 + 5 * turbulence(point)))};

    expectNear(marble.evaluate(hitAt(point)), Rgb{expected, expected, expected});
}

// Expects each channel of `color` to lie between those of `low` and `high`.
void expectBetween(const Rgb& color, const Rgb& low, const Rgb& high)
{
    EXPECT_GE(color.r, low.r - default_tolerance);
    EXPECT_LE(color.r, high.r + default_tolerance);
    EXPECT_GE(color.g, low.g - default_tolerance);
    EXPECT_LE(color.g, high.g + default_tolerance);
    EXPECT_GE(color.b, low.b - default_tolerance);
    EXPECT_LE(color.b, high.b + default_tolerance);
}

TEST(PerlinTextureTest, colorsStayBetweenLowAndHigh)
{
    constexpr PerlinTexture marble{.low = low_color, .high = high_color, .scale = 3, .veins = 8};

    for (int index{0}; index < 500; ++index) {
        const Real step{0.0731};
        expectBetween(marble.evaluate(hitAt({index * step, index * step * 0.7, -index * step})),
                      low_color, high_color);
    }
}

TEST(PerlinTextureTest, nonFinitePointsGiveAColorBetweenLowAndHigh)
{
    // Turbulence is zero there, so clouds are the low color, and the veins'
    // sine of an infinite or NaN argument is replaced by the midpoint.
    constexpr Real nan{std::numeric_limits<Real>::quiet_NaN()};
    constexpr PerlinTexture clouds{.low = low_color, .high = high_color, .scale = 1};
    constexpr PerlinTexture marble{
        .low = Rgb{0, 0, 0}, .high = Rgb{1, 1, 1}, .scale = 1, .veins = 5};

    expectNear(clouds.evaluate(hitAt({nan, 0, 0})), low_color);
    expectNear(marble.evaluate(hitAt({nan, 0, 0})), Rgb{0.5, 0.5, 0.5});
}

Rgb evaluateWith(Real scale, Real veins)
{
    const PerlinTexture texture{
        .low = low_color, .high = high_color, .scale = scale, .veins = veins};
    return texture.evaluate(hitAt({0.3, 0.1, 0.7}));
}

TEST(PerlinTextureDeathTest, rejectsZeroScale)
{
    EXPECT_DEBUG_DEATH(static_cast<void>(evaluateWith(0.0, 0.0)), "");
}

TEST(PerlinTextureDeathTest, rejectsNegativeScale)
{
    EXPECT_DEBUG_DEATH(static_cast<void>(evaluateWith(-1.0, 0.0)), "");
}

TEST(PerlinTextureDeathTest, rejectsNanScale)
{
    EXPECT_DEBUG_DEATH(static_cast<void>(evaluateWith(std::numeric_limits<Real>::quiet_NaN(), 0.0)),
                       "");
}

TEST(PerlinTextureDeathTest, rejectsInfiniteScale)
{
    EXPECT_DEBUG_DEATH(static_cast<void>(evaluateWith(std::numeric_limits<Real>::infinity(), 0.0)),
                       "");
}

TEST(PerlinTextureDeathTest, rejectsNegativeVeins)
{
    EXPECT_DEBUG_DEATH(static_cast<void>(evaluateWith(1.0, -1.0)), "");
}

TEST(PerlinTextureDeathTest, rejectsNanVeins)
{
    EXPECT_DEBUG_DEATH(static_cast<void>(evaluateWith(1.0, std::numeric_limits<Real>::quiet_NaN())),
                       "");
}

TEST(PerlinTextureDeathTest, rejectsInfiniteVeins)
{
    EXPECT_DEBUG_DEATH(static_cast<void>(evaluateWith(1.0, std::numeric_limits<Real>::infinity())),
                       "");
}

} // namespace
} // namespace albedo
