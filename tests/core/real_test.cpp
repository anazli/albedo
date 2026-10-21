#include "albedo/core/real.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <concepts>

namespace albedo {
namespace {

TEST(RealTest, isDoublePrecision)
{
    static_assert(std::same_as<Real, double>);
}

TEST(RealTest, piMatchesArcCosineOfMinusOne)
{
    constexpr Real tolerance{1e-15};

    EXPECT_NEAR(pi, std::acos(Real{-1}), tolerance);
}

TEST(RealTest, inversePiIsReciprocalOfPi)
{
    constexpr Real tolerance{1e-15};

    EXPECT_NEAR(inv_pi * pi, Real{1}, tolerance);
}

} // namespace
} // namespace albedo
