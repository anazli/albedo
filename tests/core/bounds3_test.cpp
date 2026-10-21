#include "albedo/core/bounds3.hpp"

#include "support/expect_near.hpp"

#include <gtest/gtest.h>

namespace albedo {
namespace {

using test_support::default_tolerance;
using test_support::expectNear;

constexpr Bounds3r unit_cube{.min = {0.0, 0.0, 0.0}, .max = {1.0, 1.0, 1.0}};

TEST(Bounds3Test, defaultConstructedIsEmpty)
{
    static_assert(Bounds3r{}.isEmpty());
    static_assert(Bounds3i{}.isEmpty());
}

TEST(Bounds3Test, boxWithMinAboveMaxOnOneAxisIsEmpty)
{
    constexpr Bounds3r box{.min = {0.0, 2.0, 0.0}, .max = {1.0, 1.0, 1.0}};

    static_assert(box.isEmpty());
}

TEST(Bounds3Test, mergingPointIntoEmptyGivesDegenerateBox)
{
    constexpr Point3r p{1.0, 2.0, 3.0};

    constexpr Bounds3r box{merged(Bounds3r{}, p)};

    static_assert(!box.isEmpty());
    expectNear(box.min, p);
    expectNear(box.max, p);
}

TEST(Bounds3Test, mergingPointsGrowsToEncloseAll)
{
    constexpr Bounds3i box{merged(merged(Bounds3i{}, Point3i{1, -2, 3}), Point3i{-1, 4, 0})};

    static_assert(box.min == Point3i{-1, -2, 0});
    static_assert(box.max == Point3i{1, 4, 3});
}

TEST(Bounds3Test, mergingBoxesEnclosesBoth)
{
    constexpr Bounds3i a{.min = {0, 0, 0}, .max = {1, 1, 1}};
    constexpr Bounds3i b{.min = {-1, 2, 0}, .max = {0, 3, 4}};

    constexpr Bounds3i box{merged(a, b)};

    static_assert(box.min == Point3i{-1, 0, 0});
    static_assert(box.max == Point3i{1, 3, 4});
}

TEST(Bounds3Test, emptyBoxIsIdentityForMerge)
{
    constexpr Bounds3i box{.min = {0, 1, 2}, .max = {3, 4, 5}};

    static_assert(merged(box, Bounds3i{}) == box);
    static_assert(merged(Bounds3i{}, box) == box);
}

TEST(Bounds3Test, containsIncludesFaces)
{
    static_assert(unit_cube.contains({0.5, 0.5, 0.5}));
    static_assert(unit_cube.contains({0.0, 0.0, 0.0}));
    static_assert(unit_cube.contains({1.0, 1.0, 1.0}));
    static_assert(!unit_cube.contains({1.5, 0.5, 0.5}));
    static_assert(!unit_cube.contains({0.5, -0.1, 0.5}));
}

TEST(Bounds3Test, emptyBoxContainsNothing)
{
    static_assert(!Bounds3r{}.contains({0.0, 0.0, 0.0}));
}

TEST(Bounds3Test, extentIsDiagonal)
{
    constexpr Bounds3r box{.min = {-1.0, 0.0, 2.0}, .max = {1.0, 3.0, 2.5}};

    expectNear(box.extent(), Vector3r{2.0, 3.0, 0.5});
}

TEST(Bounds3Test, surfaceAreaOfUnitCubeIsSix)
{
    EXPECT_NEAR(unit_cube.surfaceArea(), 6.0, default_tolerance);
}

TEST(Bounds3Test, surfaceAreaOfFlatBoxCountsBothSides)
{
    constexpr Bounds3r flat{.min = {0.0, 0.0, 0.0}, .max = {2.0, 3.0, 0.0}};

    EXPECT_NEAR(flat.surfaceArea(), 12.0, default_tolerance);
}

TEST(Bounds3Test, surfaceAreaOfEmptyBoxIsZero)
{
    EXPECT_NEAR(Bounds3r{}.surfaceArea(), 0.0, default_tolerance);
}

TEST(Bounds3DeathTest, extentAssertsOnEmptyBox)
{
    EXPECT_DEBUG_DEATH(static_cast<void>(Bounds3r{}.extent()), "");
}

} // namespace
} // namespace albedo
