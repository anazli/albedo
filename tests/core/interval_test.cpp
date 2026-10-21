#include "albedo/core/interval.hpp"

#include "support/expect_near.hpp"

#include <gtest/gtest.h>

#include <limits>

namespace albedo {
namespace {

using test_support::default_tolerance;

constexpr Real infinity{std::numeric_limits<Real>::infinity()};

TEST(IntervalTest, defaultConstructedIsEmpty)
{
    constexpr Interval interval{};

    static_assert(interval.isEmpty());
}

TEST(IntervalTest, emptyConstantContainsNothing)
{
    static_assert(empty_interval.isEmpty());
    static_assert(!empty_interval.contains(0.0));
    static_assert(!empty_interval.contains(infinity));
}

TEST(IntervalTest, universeContainsEverything)
{
    static_assert(!universe_interval.isEmpty());
    static_assert(universe_interval.contains(0.0));
    static_assert(universe_interval.contains(-1e300));
    static_assert(universe_interval.contains(infinity));
}

TEST(IntervalTest, sizeIsDistanceBetweenBounds)
{
    constexpr Interval interval{.min = -1.0, .max = 2.5};

    EXPECT_NEAR(interval.size(), 3.5, default_tolerance);
}

TEST(IntervalTest, degenerateIntervalIsNotEmpty)
{
    constexpr Interval point{.min = 1.0, .max = 1.0};

    static_assert(!point.isEmpty());
    EXPECT_NEAR(point.size(), 0.0, default_tolerance);
}

TEST(IntervalTest, emptyIntervalHasNegativeSize)
{
    EXPECT_LT(empty_interval.size(), 0.0);
}

TEST(IntervalTest, containsIncludesEndpoints)
{
    constexpr Interval interval{.min = 0.0, .max = 1.0};

    static_assert(interval.contains(0.0));
    static_assert(interval.contains(0.5));
    static_assert(interval.contains(1.0));
    static_assert(!interval.contains(-0.001));
    static_assert(!interval.contains(1.001));
}

TEST(IntervalTest, surroundsExcludesEndpoints)
{
    constexpr Interval interval{.min = 0.0, .max = 1.0};

    static_assert(interval.surrounds(0.5));
    static_assert(!interval.surrounds(0.0));
    static_assert(!interval.surrounds(1.0));
}

TEST(IntervalTest, degenerateIntervalSurroundsNothing)
{
    constexpr Interval point{.min = 1.0, .max = 1.0};

    static_assert(point.contains(1.0));
    static_assert(!point.surrounds(1.0));
}

TEST(IntervalTest, clampLeavesInsideValuesUnchanged)
{
    constexpr Interval interval{.min = 0.0, .max = 1.0};

    EXPECT_NEAR(interval.clamp(0.25), 0.25, default_tolerance);
}

TEST(IntervalTest, clampMovesOutsideValuesToNearestBound)
{
    constexpr Interval interval{.min = 0.0, .max = 1.0};

    EXPECT_NEAR(interval.clamp(-5.0), 0.0, default_tolerance);
    EXPECT_NEAR(interval.clamp(5.0), 1.0, default_tolerance);
}

TEST(IntervalDeathTest, clampAssertsOnEmptyInterval)
{
    EXPECT_DEBUG_DEATH(static_cast<void>(empty_interval.clamp(0.0)), "");
}

} // namespace
} // namespace albedo
