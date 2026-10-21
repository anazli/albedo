#include "albedo/core/point2.hpp"

#include <gtest/gtest.h>

#include <concepts>

namespace albedo {
namespace {

TEST(Point2Test, defaultConstructsToOrigin)
{
    constexpr Point2i p{};

    static_assert(p == Point2i{0, 0});
}

TEST(Point2Test, supportsDesignatedInitializers)
{
    constexpr Point2i p{.x = 3, .y = 7};

    static_assert(p.x == 3);
    static_assert(p.y == 7);
}

TEST(Point2Test, realAndIntegerAliasesUseExpectedScalars)
{
    static_assert(std::same_as<decltype(Point2i{}.x), int>);
    static_assert(std::same_as<decltype(Point2r{}.x), Real>);
}

} // namespace
} // namespace albedo
