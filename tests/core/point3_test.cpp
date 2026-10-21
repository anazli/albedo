#include "albedo/core/point3.hpp"

#include "support/expect_near.hpp"

#include <gtest/gtest.h>

namespace albedo {
namespace {

using test_support::default_tolerance;
using test_support::expectNear;

template <typename A, typename B>
concept Addable = requires(A a, B b) { a + b; };

template <typename A, typename B>
concept Subtractable = requires(A a, B b) { a - b; };

template <typename A>
concept Scalable = requires(A a) { a * 2; };

template <typename A>
concept ScalableOnLeft = requires(A a) { 2 * a; };

template <typename A>
concept Divisible = requires(A a) { a / 2; };

TEST(Point3Test, defaultConstructsToOrigin)
{
    constexpr Point3i p{};

    static_assert(p == Point3i{0, 0, 0});
}

TEST(Point3Test, differenceOfPointsIsVector)
{
    constexpr Point3i a{4, 5, 6};
    constexpr Point3i b{1, 2, 3};

    constexpr Vector3i d{a - b};

    static_assert(d == Vector3i{3, 3, 3});
}

TEST(Point3Test, offsettingByVectorGivesPoint)
{
    constexpr Point3i p{1, 2, 3};
    constexpr Vector3i v{1, -1, 2};

    static_assert(p + v == Point3i{2, 1, 5});
    static_assert(p - v == Point3i{0, 3, 1});
}

TEST(Point3Test, compoundOffsetMatchesBinaryOperators)
{
    Point3r p{1.0, 2.0, 3.0};

    p += Vector3r{0.5, 0.5, 0.5};
    p -= Vector3r{1.0, 0.0, 2.0};

    expectNear(p, Point3r{0.5, 2.5, 1.5});
}

TEST(Point3Test, pointPlusDifferenceRoundTrips)
{
    constexpr Point3r a{1.5, -2.0, 0.25};
    constexpr Point3r b{-3.0, 4.0, 8.0};

    expectNear(b + (a - b), a);
}

TEST(Point3Test, rejectsGeometricallyMeaninglessOperations)
{
    static_assert(!Addable<Point3r, Point3r>);
    static_assert(!Addable<Vector3r, Point3r>);
    static_assert(!Subtractable<Vector3r, Point3r>);
    static_assert(!Scalable<Point3r>);
    static_assert(!ScalableOnLeft<Point3r>);
    static_assert(!Divisible<Point3r>);
}

TEST(Point3Test, allowsMeaningfulOperations)
{
    static_assert(Addable<Point3r, Vector3r>);
    static_assert(Subtractable<Point3r, Vector3r>);
    static_assert(Subtractable<Point3r, Point3r>);
}

TEST(Point3Test, distanceIsLengthOfDifference)
{
    constexpr Point3r a{1.0, 2.0, 3.0};
    constexpr Point3r b{4.0, 6.0, 3.0};

    EXPECT_NEAR(distance(a, b), 5.0, default_tolerance);
}

TEST(Point3Test, distanceSquaredIsSquaredLengthOfDifference)
{
    constexpr Point3i a{1, 2, 3};
    constexpr Point3i b{4, 6, 3};

    static_assert(distanceSquared(a, b) == 25);
}

TEST(Point3Test, componentMinAndMaxPickPerAxis)
{
    constexpr Point3i a{1, 5, -3};
    constexpr Point3i b{2, -4, -3};

    static_assert(componentMin(a, b) == Point3i{1, -4, -3});
    static_assert(componentMax(a, b) == Point3i{2, 5, -3});
}

TEST(Point3Test, distanceToSelfIsZero)
{
    constexpr Point3r p{1.0, 2.0, 3.0};

    EXPECT_NEAR(distance(p, p), 0.0, default_tolerance);
}

} // namespace
} // namespace albedo
