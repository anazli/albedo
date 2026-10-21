#include "albedo/color/srgb.hpp"

#include "support/expect_near.hpp"

#include <gtest/gtest.h>

#include <cstdint>
#include <limits>

namespace albedo {
namespace {

using test_support::default_tolerance;

// Where the sRGB transfer function switches from linear to power-law segment.
constexpr Real linear_segment_end{0.0031308};

TEST(SrgbTest, encodesBlackAndWhiteUnchanged)
{
    EXPECT_NEAR(encodeSrgb(0.0), 0.0, default_tolerance);
    EXPECT_NEAR(encodeSrgb(1.0), 1.0, default_tolerance);
}

TEST(SrgbTest, encodesSmallValuesLinearly)
{
    EXPECT_NEAR(encodeSrgb(0.001), 0.01292, default_tolerance);
}

TEST(SrgbTest, segmentsMeetAtTheBreakpoint)
{
    // Both segments give ~0.04045 at the breakpoint; the curve must not jump.
    constexpr Real tolerance{1e-6};

    EXPECT_NEAR(encodeSrgb(linear_segment_end), 0.04045, tolerance);
    EXPECT_NEAR(encodeSrgb(linear_segment_end * (1 + 1e-9)), 0.04045, tolerance);
}

TEST(SrgbTest, encodesMidGrayToKnownValue)
{
    // 50% linear reflectance is the well-known sRGB code value 188 of 255.
    constexpr Real tolerance{1e-6};

    EXPECT_NEAR(encodeSrgb(0.5), 0.735357, tolerance);
}

TEST(SrgbTest, encodesNegativeValuesOnLinearSegment)
{
    EXPECT_NEAR(encodeSrgb(-0.5), 12.92 * -0.5, default_tolerance);
}

TEST(SrgbTest, encodesValuesAboveOneAboveOne)
{
    EXPECT_GT(encodeSrgb(2.0), 1.0);
}

TEST(SrgbTest, encodedNanBecomesZeroByte)
{
    EXPECT_EQ(toByte(encodeSrgb(std::numeric_limits<Real>::quiet_NaN())), std::uint8_t{0});
}

TEST(SrgbTest, toByteMapsUnitRangeToFullByteRange)
{
    EXPECT_EQ(toByte(0.0), std::uint8_t{0});
    EXPECT_EQ(toByte(1.0), std::uint8_t{255});
}

TEST(SrgbTest, toByteRoundsToNearest)
{
    EXPECT_EQ(toByte(encodeSrgb(0.5)), std::uint8_t{188});
    EXPECT_EQ(toByte(0.5), std::uint8_t{128});
}

TEST(SrgbTest, toByteClampsOutOfRangeValues)
{
    EXPECT_EQ(toByte(-0.5), std::uint8_t{0});
    EXPECT_EQ(toByte(7.0), std::uint8_t{255});
    EXPECT_EQ(toByte(std::numeric_limits<Real>::infinity()), std::uint8_t{255});
    EXPECT_EQ(toByte(-std::numeric_limits<Real>::infinity()), std::uint8_t{0});
}

TEST(SrgbTest, toByteMapsNanToZero)
{
    // A NaN from a rendering bug must give a deterministic black pixel.
    EXPECT_EQ(toByte(std::numeric_limits<Real>::quiet_NaN()), std::uint8_t{0});
}

} // namespace
} // namespace albedo
