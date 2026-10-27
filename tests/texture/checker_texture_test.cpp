#include "albedo/texture/checker_texture.hpp"

#include "support/expect_near.hpp"

#include <gtest/gtest.h>

#include <limits>

namespace albedo {
namespace {

using test_support::exact_tolerance;
using test_support::expectNear;

constexpr Rgb even_color{0.9, 0.9, 0.9};
constexpr Rgb odd_color{0.1, 0.2, 0.3};

// Four squares per unit of u and of v, so each square is 0.25 wide.
constexpr CheckerTexture checker{.even = even_color, .odd = odd_color, .frequency = 4.0};

HitRecord hitAt(Point2r uv)
{
    return HitRecord{.uv = uv};
}

TEST(CheckerTextureTest, originSquareIsEven)
{
    expectNear(checker.evaluate(hitAt({0.1, 0.1})), even_color, exact_tolerance);
}

TEST(CheckerTextureTest, neighboursAlongUOrVAreOdd)
{
    expectNear(checker.evaluate(hitAt({0.3, 0.1})), odd_color, exact_tolerance);
    expectNear(checker.evaluate(hitAt({0.1, 0.3})), odd_color, exact_tolerance);
}

TEST(CheckerTextureTest, diagonalNeighbourIsEven)
{
    expectNear(checker.evaluate(hitAt({0.3, 0.3})), even_color, exact_tolerance);
}

TEST(CheckerTextureTest, squareEdgesBelongToTheSquareAbove)
{
    // u = 0.25 starts the second square along u.
    expectNear(checker.evaluate(hitAt({0.25, 0.1})), odd_color, exact_tolerance);
}

TEST(CheckerTextureTest, patternContinuesBelowZero)
{
    // Squares (-1, 0) and (-1, -1): odd and even, as the pattern continues.
    expectNear(checker.evaluate(hitAt({-0.1, 0.1})), odd_color, exact_tolerance);
    expectNear(checker.evaluate(hitAt({-0.1, -0.1})), even_color, exact_tolerance);
}

TEST(CheckerTextureTest, negativeSquareEdgesBelongToTheSquareAbove)
{
    // u = -0.25 starts square -1 along u, which is odd.
    expectNear(checker.evaluate(hitAt({-0.25, 0.1})), odd_color, exact_tolerance);
}

TEST(CheckerTextureTest, frequencySetsTheSquareSize)
{
    constexpr CheckerTexture coarse{.even = even_color, .odd = odd_color, .frequency = 1.0};

    expectNear(coarse.evaluate(hitAt({0.3, 0.1})), even_color, exact_tolerance);
    expectNear(coarse.evaluate(hitAt({1.3, 0.1})), odd_color, exact_tolerance);
}

TEST(CheckerTextureTest, coordinatesTooLargeForAnyIntegerStillGiveAColor)
{
    // Far beyond the range of any integer type, so converting the square indices
    // to integers would be undefined behaviour. The two floors cancel exactly,
    // giving square sum 0: even.
    constexpr Real huge{1e300};

    expectNear(checker.evaluate(hitAt({huge, -huge})), even_color, exact_tolerance);
}

TEST(CheckerTextureTest, productsOverflowingToInfinityGiveTheEvenColor)
{
    // u * frequency overflows: infinity + infinity and infinity - infinity.
    constexpr Real largest{std::numeric_limits<Real>::max()};

    expectNear(checker.evaluate(hitAt({largest, largest})), even_color, exact_tolerance);
    expectNear(checker.evaluate(hitAt({largest, -largest})), even_color, exact_tolerance);
}

TEST(CheckerTextureTest, nanCoordinatesGiveTheEvenColor)
{
    constexpr Real nan{std::numeric_limits<Real>::quiet_NaN()};

    expectNear(checker.evaluate(hitAt({nan, 0.3})), even_color, exact_tolerance);
    expectNear(checker.evaluate(hitAt({0.3, nan})), even_color, exact_tolerance);
}

// Evaluates a checker with the given frequency, for the precondition checks.
Rgb evaluateWithFrequency(Real frequency)
{
    const CheckerTexture texture{.even = even_color, .odd = odd_color, .frequency = frequency};
    return texture.evaluate(hitAt({0.3, 0.1}));
}

TEST(CheckerTextureDeathTest, rejectsZeroFrequency)
{
    EXPECT_DEBUG_DEATH(static_cast<void>(evaluateWithFrequency(0.0)), "");
}

TEST(CheckerTextureDeathTest, rejectsNegativeFrequency)
{
    EXPECT_DEBUG_DEATH(static_cast<void>(evaluateWithFrequency(-1.0)), "");
}

TEST(CheckerTextureDeathTest, rejectsNanFrequency)
{
    EXPECT_DEBUG_DEATH(
        static_cast<void>(evaluateWithFrequency(std::numeric_limits<Real>::quiet_NaN())), "");
}

TEST(CheckerTextureDeathTest, rejectsInfiniteFrequency)
{
    EXPECT_DEBUG_DEATH(
        static_cast<void>(evaluateWithFrequency(std::numeric_limits<Real>::infinity())), "");
}

} // namespace
} // namespace albedo
