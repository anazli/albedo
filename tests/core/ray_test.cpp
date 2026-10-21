#include "albedo/core/ray.hpp"

#include "support/expect_near.hpp"

#include <gtest/gtest.h>

namespace albedo {
namespace {

using test_support::expectNear;

TEST(RayTest, atZeroIsOrigin)
{
    constexpr Ray ray{.origin = {1.0, 2.0, 3.0}, .direction = {0.0, 0.0, 1.0}};

    expectNear(ray.at(0.0), ray.origin);
}

TEST(RayTest, atMovesAlongDirectionScaledByParameter)
{
    constexpr Ray ray{.origin = {1.0, 2.0, 3.0}, .direction = {1.0, -1.0, 2.0}};

    expectNear(ray.at(2.0), Point3r{3.0, 0.0, 7.0});
}

TEST(RayTest, atNegativeParameterGoesBehindOrigin)
{
    constexpr Ray ray{.origin = {0.0, 0.0, 0.0}, .direction = {0.0, 1.0, 0.0}};

    expectNear(ray.at(-1.5), Point3r{0.0, -1.5, 0.0});
}

TEST(RayTest, directionNeedNotBeNormalized)
{
    // Unnormalized directions are allowed; t is then measured in units of |direction|.
    constexpr Ray ray{.origin = {0.0, 0.0, 0.0}, .direction = {0.0, 0.0, 4.0}};

    expectNear(ray.at(0.5), Point3r{0.0, 0.0, 2.0});
}

} // namespace
} // namespace albedo
