#include "albedo/color/rgb.hpp"

#include "support/expect_near.hpp"

#include <gtest/gtest.h>

namespace albedo {
namespace {

using test_support::default_tolerance;
using test_support::expectNear;

TEST(RgbTest, defaultConstructsToBlack)
{
    constexpr Rgb color{};

    expectNear(color, Rgb{0.0, 0.0, 0.0});
}

TEST(RgbTest, supportsDesignatedInitializers)
{
    constexpr Rgb color{.r = 0.25, .g = 0.5, .b = 1.0};

    expectNear(color, Rgb{0.25, 0.5, 1.0});
}

TEST(RgbTest, addsPerChannel)
{
    constexpr Rgb a{0.1, 0.2, 0.3};
    constexpr Rgb b{1.0, 2.0, 3.0};

    expectNear(a + b, Rgb{1.1, 2.2, 3.3});
}

TEST(RgbTest, multipliesColorsPerChannel)
{
    // Filtering light by a surface color: each channel attenuates independently.
    constexpr Rgb light{2.0, 1.0, 0.5};
    constexpr Rgb surface_color{0.5, 0.25, 1.0};

    expectNear(light * surface_color, Rgb{1.0, 0.25, 0.5});
}

TEST(RgbTest, scalesByScalarOnEitherSide)
{
    constexpr Rgb color{0.5, 1.0, 2.0};

    expectNear(color * 2.0, Rgb{1.0, 2.0, 4.0});
    expectNear(2.0 * color, Rgb{1.0, 2.0, 4.0});
}

TEST(RgbTest, dividesByScalar)
{
    constexpr Rgb color{1.0, 2.0, 4.0};

    expectNear(color / 4.0, Rgb{0.25, 0.5, 1.0});
}

TEST(RgbTest, compoundAssignmentMatchesBinaryOperators)
{
    Rgb color{1.0, 1.0, 1.0};

    color += Rgb{1.0, 0.0, 3.0};
    color *= Rgb{0.5, 2.0, 0.25};
    color *= 2.0;
    color /= 4.0;

    expectNear(color, Rgb{0.5, 1.0, 0.5});
}

TEST(RgbTest, luminanceOfWhiteIsOne)
{
    EXPECT_NEAR(luminance(Rgb{1.0, 1.0, 1.0}), 1.0, default_tolerance);
}

TEST(RgbTest, luminanceOfBlackIsZero)
{
    EXPECT_NEAR(luminance(Rgb{}), 0.0, default_tolerance);
}

TEST(RgbTest, luminanceUsesRec709Weights)
{
    // Rec. 709 weights: the eye is most sensitive to green, least to blue.
    const Real red{luminance(Rgb{1.0, 0.0, 0.0})};
    const Real green{luminance(Rgb{0.0, 1.0, 0.0})};
    const Real blue{luminance(Rgb{0.0, 0.0, 1.0})};

    EXPECT_NEAR(red, 0.2126, default_tolerance);
    EXPECT_NEAR(green, 0.7152, default_tolerance);
    EXPECT_NEAR(blue, 0.0722, default_tolerance);
}

TEST(RgbTest, isBlackWhenAllChannelsAreZero)
{
    EXPECT_TRUE(isBlack(Rgb{}));
    EXPECT_TRUE(isBlack(Rgb{-0.0, -0.0, -0.0}));
}

TEST(RgbTest, isNotBlackWhenAnyChannelIsNonZero)
{
    EXPECT_FALSE(isBlack(Rgb{1e-300, 0.0, 0.0}));
    EXPECT_FALSE(isBlack(Rgb{0.0, 1e-300, 0.0}));
    EXPECT_FALSE(isBlack(Rgb{0.0, 0.0, 1e-300}));
}

TEST(RgbTest, comparesEqualWhenAllChannelsMatch)
{
    // Exact comparison is for exactly constructed colors; computed ones use expectNear.
    static_assert(Rgb{0.25, 0.5, 1.0} == Rgb{0.25, 0.5, 1.0});
    static_assert(Rgb{0.25, 0.5, 1.0} != Rgb{0.25, 0.5, 0.0});
}

} // namespace
} // namespace albedo
