#include "albedo/image/pixel_buffer.hpp"

#include "support/expect_near.hpp"

#include <gtest/gtest.h>

#include <limits>
#include <stdexcept>

namespace albedo {
namespace {

using test_support::expectNear;

TEST(PixelBufferTest, reportsItsDimensions)
{
    const PixelBuffer buffer{3, 2};

    EXPECT_EQ(buffer.width(), 3);
    EXPECT_EQ(buffer.height(), 2);
}

TEST(PixelBufferTest, pixelWithoutSamplesIsBlack)
{
    const PixelBuffer buffer{2, 2};

    EXPECT_EQ(buffer.average({.x = 1, .y = 1}), Rgb{});
}

TEST(PixelBufferTest, averageOfOneSampleIsThatSample)
{
    PixelBuffer buffer{2, 2};

    buffer.addSample({.x = 1, .y = 0}, Rgb{0.25, 0.5, 1.0});

    expectNear(buffer.average({.x = 1, .y = 0}), Rgb{0.25, 0.5, 1.0});
}

TEST(PixelBufferTest, averagesAllSamplesOfAPixel)
{
    PixelBuffer buffer{1, 1};

    buffer.addSample({.x = 0, .y = 0}, Rgb{1.0, 0.0, 0.0});
    buffer.addSample({.x = 0, .y = 0}, Rgb{0.0, 1.0, 0.0});
    buffer.addSample({.x = 0, .y = 0}, Rgb{0.5, 0.5, 3.0});

    expectNear(buffer.average({.x = 0, .y = 0}), Rgb{0.5, 0.5, 1.0});
}

TEST(PixelBufferTest, samplesOnlyAffectTheirOwnPixel)
{
    PixelBuffer buffer{2, 2};

    buffer.addSample({.x = 0, .y = 1}, Rgb{1.0, 1.0, 1.0});

    EXPECT_EQ(buffer.average({.x = 0, .y = 0}), Rgb{});
    EXPECT_EQ(buffer.average({.x = 1, .y = 0}), Rgb{});
    EXPECT_EQ(buffer.average({.x = 1, .y = 1}), Rgb{});
}

TEST(PixelBufferTest, cornerPixelsAreAddressable)
{
    PixelBuffer buffer{3, 2};

    buffer.addSample({.x = 0, .y = 0}, Rgb{1.0, 0.0, 0.0});
    buffer.addSample({.x = 2, .y = 1}, Rgb{0.0, 0.0, 1.0});

    expectNear(buffer.average({.x = 0, .y = 0}), Rgb{1.0, 0.0, 0.0});
    expectNear(buffer.average({.x = 2, .y = 1}), Rgb{0.0, 0.0, 1.0});
}

TEST(PixelBufferTest, rejectsZeroDimensions)
{
    EXPECT_THROW(PixelBuffer(0, 4), std::invalid_argument);
    EXPECT_THROW(PixelBuffer(4, 0), std::invalid_argument);
}

TEST(PixelBufferTest, rejectsNegativeDimensions)
{
    EXPECT_THROW(PixelBuffer(-1, 4), std::invalid_argument);
    EXPECT_THROW(PixelBuffer(4, -1), std::invalid_argument);
}

TEST(PixelBufferTest, rejectsImagesAboveThePixelLimit)
{
    // 65536 x 65536 = 2^32 pixels: would need ~128 GiB of accumulators.
    EXPECT_THROW(PixelBuffer(65536, 65536), std::invalid_argument);
}

TEST(PixelBufferTest, sizeValidationAcceptsUpToThePixelLimitInclusive)
{
    // Checked without allocating: an image at the limit needs several GiB.
    static_assert(PixelBuffer::isValidSize(1, 1));
    static_assert(PixelBuffer::isValidSize(16384, 16384));
    static_assert(PixelBuffer::isValidSize(1 << 28, 1));
}

TEST(PixelBufferTest, sizeValidationRejectsOnePixelOverTheLimit)
{
    static_assert(!PixelBuffer::isValidSize(16384, 16385));
    static_assert(!PixelBuffer::isValidSize((1 << 28) + 1, 1));
}

TEST(PixelBufferTest, sizeValidationRejectsNonPositiveDimensions)
{
    static_assert(!PixelBuffer::isValidSize(0, 1));
    static_assert(!PixelBuffer::isValidSize(1, -1));
}

TEST(PixelBufferTest, sizeValidationDoesNotOverflowForLargeDimensions)
{
    constexpr int largest{std::numeric_limits<int>::max()};

    static_assert(!PixelBuffer::isValidSize(largest, largest));
}

TEST(PixelBufferDeathTest, addSampleAssertsOnOutOfBoundsPixel)
{
    PixelBuffer buffer{2, 2};

    EXPECT_DEBUG_DEATH(buffer.addSample({.x = 2, .y = 0}, Rgb{}), "");
    EXPECT_DEBUG_DEATH(buffer.addSample({.x = 0, .y = -1}, Rgb{}), "");
}

TEST(PixelBufferDeathTest, averageAssertsOnOutOfBoundsPixel)
{
    const PixelBuffer buffer{2, 2};

    EXPECT_DEBUG_DEATH(static_cast<void>(buffer.average({.x = 0, .y = 2})), "");
}

} // namespace
} // namespace albedo
