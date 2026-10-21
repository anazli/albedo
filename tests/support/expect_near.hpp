#pragma once

#include <gtest/gtest.h>

namespace albedo::test_support {

/// Types with numeric `x`, `y` and `z` members, such as `Vector3` and `Point3`.
template <typename T>
concept HasXyz = requires(const T& t) {
    static_cast<double>(t.x);
    static_cast<double>(t.y);
    static_cast<double>(t.z);
};

/// Types with numeric `r`, `g` and `b` members, such as `Rgb`.
template <typename T>
concept HasRgb = requires(const T& t) {
    static_cast<double>(t.r);
    static_cast<double>(t.g);
    static_cast<double>(t.b);
};

/// Default tolerance for comparing results of a few floating-point operations.
inline constexpr double default_tolerance{1e-12};

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
