#include "albedo/sampling/sample_generator.hpp"

#include <gtest/gtest.h>

#include <cstdint>

namespace albedo {
namespace {

TEST(SampleGeneratorTest, next1DIsInUnitInterval)
{
    SampleGenerator generator{SampleGenerator::forPixel(5, {.x = 3, .y = 4}, 640)};

    for (int i{0}; i < 10'000; ++i) {
        const Real value{generator.next1D()};
        ASSERT_GE(value, 0.0);
        ASSERT_LT(value, 1.0);
    }
}

TEST(SampleGeneratorTest, next2DIsInUnitSquare)
{
    SampleGenerator generator{SampleGenerator::forPixel(5, {.x = 3, .y = 4}, 640)};

    for (int i{0}; i < 10'000; ++i) {
        const Point2r value{generator.next2D()};
        ASSERT_GE(value.x, 0.0);
        ASSERT_LT(value.x, 1.0);
        ASSERT_GE(value.y, 0.0);
        ASSERT_LT(value.y, 1.0);
    }
}

// Determinism and difference are checked on the raw 32-bit outputs, so no
// floating-point values are compared exactly. The generator's first value is
// the first Pcg32 output scaled by 2^-32, so equal outputs mean equal values.
std::uint32_t firstBits(SampleGenerator generator)
{
    return static_cast<std::uint32_t>(generator.next1D() * 4294967296.0);
}

TEST(SampleGeneratorTest, samePixelAndSeedGiveSameSequence)
{
    // Determinism per pixel is what lets renders be identical across thread counts.
    SampleGenerator first{SampleGenerator::forPixel(9, {.x = 10, .y = 20}, 100)};
    SampleGenerator second{SampleGenerator::forPixel(9, {.x = 10, .y = 20}, 100)};

    for (int i{0}; i < 100; ++i) {
        EXPECT_EQ(firstBits(first), firstBits(second));
        static_cast<void>(first.next1D());
        static_cast<void>(second.next1D());
    }
}

TEST(SampleGeneratorTest, horizontalNeighboursGetDifferentSequences)
{
    EXPECT_NE(firstBits(SampleGenerator::forPixel(9, {.x = 10, .y = 20}, 100)),
              firstBits(SampleGenerator::forPixel(9, {.x = 11, .y = 20}, 100)));
}

TEST(SampleGeneratorTest, verticalNeighboursGetDifferentSequences)
{
    EXPECT_NE(firstBits(SampleGenerator::forPixel(9, {.x = 10, .y = 20}, 100)),
              firstBits(SampleGenerator::forPixel(9, {.x = 10, .y = 21}, 100)));
}

TEST(SampleGeneratorTest, differentSeedsGiveDifferentSequences)
{
    EXPECT_NE(firstBits(SampleGenerator::forPixel(1, {.x = 10, .y = 20}, 100)),
              firstBits(SampleGenerator::forPixel(2, {.x = 10, .y = 20}, 100)));
}

TEST(SampleGeneratorTest, next2DDrawsXBeforeY)
{
    // Later steps rely on this order for reproducible sample consumption.
    SampleGenerator pairs{SampleGenerator::forPixel(3, {.x = 1, .y = 2}, 10)};
    SampleGenerator singles{SampleGenerator::forPixel(3, {.x = 1, .y = 2}, 10)};

    const Point2r point{pairs.next2D()};
    const Real first{singles.next1D()};
    const Real second{singles.next1D()};

    EXPECT_DOUBLE_EQ(point.x, first);
    EXPECT_DOUBLE_EQ(point.y, second);
}

TEST(SampleGeneratorDeathTest, forPixelAssertsOnXOutsideImageWidth)
{
    EXPECT_DEBUG_DEATH(static_cast<void>(SampleGenerator::forPixel(1, {.x = 100, .y = 0}, 100)),
                       "");
    EXPECT_DEBUG_DEATH(static_cast<void>(SampleGenerator::forPixel(1, {.x = -1, .y = 0}, 100)), "");
}

TEST(SampleGeneratorDeathTest, forPixelAssertsOnNegativeY)
{
    EXPECT_DEBUG_DEATH(static_cast<void>(SampleGenerator::forPixel(1, {.x = 0, .y = -1}, 100)), "");
}

} // namespace
} // namespace albedo
