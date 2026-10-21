#pragma once

#include "albedo/core/normal3.hpp"
#include "albedo/core/point3.hpp"
#include "albedo/core/vector3.hpp"

#include <gtest/gtest.h>

namespace albedo::test_support {

/// Default tolerance for comparing results of a few floating-point operations.
inline constexpr double default_tolerance{1e-12};

/// Expects each component of `actual` to be within `tolerance` of `expected`.
template <typename Tuple>
void expectNear(const Tuple& actual, const Tuple& expected, double tolerance = default_tolerance)
{
    EXPECT_NEAR(actual.x, expected.x, tolerance);
    EXPECT_NEAR(actual.y, expected.y, tolerance);
    EXPECT_NEAR(actual.z, expected.z, tolerance);
}

} // namespace albedo::test_support
