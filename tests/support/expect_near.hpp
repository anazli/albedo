#pragma once

#include <gtest/gtest.h>

namespace albedo::test_support {

/// Types with numeric `x` and `y` members, such as `Point2`.
template <typename T>
concept HasXy = requires(const T& t) {
    static_cast<double>(t.x);
    static_cast<double>(t.y);
};

/// Types with numeric `x`, `y` and `z` members, such as `Vector3` and `Point3`.
/// Built on `HasXy` so that overloads for these types win over the 2D ones.
template <typename T>
concept HasXyz = HasXy<T> && requires(const T& t) { static_cast<double>(t.z); };

/// Types with numeric `r`, `g` and `b` members, such as `Rgb`.
template <typename T>
concept HasRgb = requires(const T& t) {
    static_cast<double>(t.r);
    static_cast<double>(t.g);
    static_cast<double>(t.b);
};

/// Default tolerance for comparing results of a few floating-point operations.
inline constexpr double default_tolerance{1e-12};

/// Zero tolerance, for values copied rather than computed (an exact constant or
/// an unchanged input), where any difference is a bug.
inline constexpr double exact_tolerance{0.0};

/// Expects each component of the 2D `actual` to be within `tolerance` of `expected`.
template <HasXy Pair>
void expectNear(const Pair& actual, const Pair& expected, double tolerance = default_tolerance)
{
    EXPECT_NEAR(actual.x, expected.x, tolerance);
    EXPECT_NEAR(actual.y, expected.y, tolerance);
}

/// Expects each component of `actual` to be within `tolerance` of `expected`.
template <HasXyz Tuple>
void expectNear(const Tuple& actual, const Tuple& expected, double tolerance = default_tolerance)
{
    EXPECT_NEAR(actual.x, expected.x, tolerance);
    EXPECT_NEAR(actual.y, expected.y, tolerance);
    EXPECT_NEAR(actual.z, expected.z, tolerance);
}

/// Expects each channel of `actual` to be within `tolerance` of `expected`.
template <HasRgb Color>
void expectNear(const Color& actual, const Color& expected, double tolerance = default_tolerance)
{
    EXPECT_NEAR(actual.r, expected.r, tolerance);
    EXPECT_NEAR(actual.g, expected.g, tolerance);
    EXPECT_NEAR(actual.b, expected.b, tolerance);
}

} // namespace albedo::test_support
