#include "albedo/core/normal3.hpp"

#include "support/expect_near.hpp"

#include <gtest/gtest.h>

#include <cmath>

namespace albedo {
namespace {

using test_support::default_tolerance;
using test_support::expectNear;

template <typename T>
concept CanMakeNormal = requires { typename Normal3<T>; };

template <typename A, typename B>
concept Addable = requires(A a, B b) { a + b; };

template <typename A, typename B>
concept Subtractable = requires(A a, B b) { a - b; };

template <typename N, typename S>
concept ScalableOnRightBy = requires(N n, S s) { n* s; };

template <typename N, typename S>
concept ScalableOnLeftBy = requires(N n, S s) { s* n; };

TEST(Normal3Test, requiresFloatingPointComponents)
{
    static_assert(!CanMakeNormal<int>);
    static_assert(CanMakeNormal<float>);
    static_assert(CanMakeNormal<double>);
}

TEST(Normal3Test, negationFlipsOrientation)
{
    constexpr Normal3r n{0.0, 1.0, 0.0};

    expectNear(-n, Normal3r{0.0, -1.0, 0.0});
}

TEST(Normal3Test, scalesByScalar)
{
    constexpr Normal3r n{0.0, 1.0, 2.0};

    expectNear(n * 2.0, Normal3r{0.0, 2.0, 4.0});
    expectNear(2.0 * n, Normal3r{0.0, 2.0, 4.0});
}

TEST(Normal3Test, rejectsScaleFactorsThatWouldLosePrecision)
{
    static_assert(!ScalableOnRightBy<Normal3<float>, double>);
    static_assert(!ScalableOnLeftBy<Normal3<float>, double>);
    static_assert(ScalableOnRightBy<Normal3<float>, float>);
    static_assert(ScalableOnLeftBy<Normal3<float>, float>);
    static_assert(ScalableOnRightBy<Normal3r, float>);
    static_assert(ScalableOnLeftBy<Normal3r, float>);
}

TEST(Normal3Test, rejectsLongDoubleFactorOnEveryPlatform)
{
    static_assert(!ScalableOnRightBy<Normal3r, long double>);
    static_assert(!ScalableOnLeftBy<Normal3r, long double>);
}

TEST(Normal3Test, dotWithVectorInEitherOrder)
{
    constexpr Normal3r n{0.0, 0.0, 1.0};
    constexpr Vector3r v{1.0, 2.0, 3.0};

    EXPECT_NEAR(dot(n, v), 3.0, default_tolerance);
    EXPECT_NEAR(dot(v, n), 3.0, default_tolerance);
}

TEST(Normal3Test, dotWithNormal)
{
    constexpr Normal3r a{1.0, 2.0, 3.0};
    constexpr Normal3r b{0.0, -1.0, 1.0};

    EXPECT_NEAR(dot(a, b), 1.0, default_tolerance);
}

TEST(Normal3Test, normalizedHasUnitLength)
{
    constexpr Normal3r n{0.0, 3.0, 4.0};

    expectNear(normalized(n), Normal3r{0.0, 0.6, 0.8});
}

TEST(Normal3Test, convertsExplicitlyToAndFromVector)
{
    constexpr Normal3r n{1.0, 2.0, 3.0};

    constexpr Vector3r v{asVector(n)};

    expectNear(v, Vector3r{1.0, 2.0, 3.0});
    expectNear(asNormal(v), n);
}

TEST(Normal3Test, doesNotMixWithVectorArithmetic)
{
    // A normal transforms differently from a vector, so implicit mixing is a bug.
    static_assert(!Addable<Normal3r, Vector3r>);
    static_assert(!Addable<Vector3r, Normal3r>);
    static_assert(!Subtractable<Normal3r, Vector3r>);
    static_assert(!Subtractable<Vector3r, Normal3r>);
}

TEST(Normal3Test, negationFlipsSignOfZero)
{
    const Normal3r n{-Normal3r{}};

    EXPECT_TRUE(std::signbit(n.x));
    EXPECT_TRUE(std::signbit(n.y));
    EXPECT_TRUE(std::signbit(n.z));
}

TEST(Normal3DeathTest, normalizedAssertsOnZeroNormal)
{
    EXPECT_DEBUG_DEATH(static_cast<void>(normalized(Normal3r{})), "");
}

} // namespace
} // namespace albedo
