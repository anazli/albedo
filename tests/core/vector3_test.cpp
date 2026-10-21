#include "albedo/core/vector3.hpp"

#include "support/expect_near.hpp"

#include <gtest/gtest.h>

#include <cmath>

namespace albedo {
namespace {

using test_support::default_tolerance;
using test_support::expectNear;

template <typename V>
concept HasLength = requires(V v) { length(v); };

template <typename V>
concept HasNormalized = requires(V v) { normalized(v); };

template <typename V>
concept DivisibleByScalar = requires(V v) { v / 2; };

template <typename V, typename S>
concept ScalableOnRightBy = requires(V v, S s) { v* s; };

template <typename V, typename S>
concept ScalableOnLeftBy = requires(V v, S s) { s* v; };

template <typename V, typename S>
concept CompoundScalableBy = requires(V v, S s) { v *= s; };

template <typename V, typename S>
concept DivisibleBy = requires(V v, S s) { v / s; };

template <typename V, typename S>
concept CompoundDivisibleBy = requires(V v, S s) { v /= s; };

TEST(Vector3Test, defaultConstructsToZero)
{
    constexpr Vector3i v{};

    static_assert(v == Vector3i{0, 0, 0});
}

TEST(Vector3Test, addsComponentWise)
{
    constexpr Vector3i a{1, 2, 3};
    constexpr Vector3i b{4, 5, 6};

    static_assert(a + b == Vector3i{5, 7, 9});
}

TEST(Vector3Test, subtractsComponentWise)
{
    constexpr Vector3i a{4, 5, 6};
    constexpr Vector3i b{1, 2, 3};

    static_assert(a - b == Vector3i{3, 3, 3});
}

TEST(Vector3Test, negatesEachComponent)
{
    constexpr Vector3i v{1, -2, 3};

    static_assert(-v == Vector3i{-1, 2, -3});
}

TEST(Vector3Test, negationFlipsSignOfZero)
{
    const Vector3r v{-Vector3r{}};

    EXPECT_TRUE(std::signbit(v.x));
    EXPECT_TRUE(std::signbit(v.y));
    EXPECT_TRUE(std::signbit(v.z));
}

TEST(Vector3Test, rejectsScaleFactorsThatWouldLosePrecision)
{
    // A floating-point factor would be truncated for integer vectors, and
    // narrowed for float vectors; both are silent bugs (and C4244 on MSVC).
    static_assert(!ScalableOnRightBy<Vector3i, double>);
    static_assert(!ScalableOnLeftBy<Vector3i, double>);
    static_assert(!ScalableOnRightBy<Vector3i, float>);
    static_assert(!ScalableOnLeftBy<Vector3i, float>);
    static_assert(!CompoundScalableBy<Vector3i, double>);
    static_assert(!DivisibleBy<Vector3i, double>);
    static_assert(!CompoundDivisibleBy<Vector3i, double>);
    static_assert(!ScalableOnRightBy<Vector3<float>, double>);
    static_assert(!ScalableOnLeftBy<Vector3<float>, double>);
    static_assert(!CompoundScalableBy<Vector3<float>, double>);
    static_assert(!DivisibleBy<Vector3<float>, double>);
    static_assert(!CompoundDivisibleBy<Vector3<float>, double>);
}

TEST(Vector3Test, rejectsLongDoubleFactorOnEveryPlatform)
{
    // long double -> double narrows by language rules even where both are 8 bytes (MSVC).
    static_assert(!ScalableOnRightBy<Vector3r, long double>);
    static_assert(!ScalableOnLeftBy<Vector3r, long double>);
    static_assert(!DivisibleBy<Vector3r, long double>);
    static_assert(!CompoundScalableBy<Vector3r, long double>);
    static_assert(!CompoundDivisibleBy<Vector3r, long double>);
}

TEST(Vector3Test, acceptsScaleFactorsThatKeepPrecision)
{
    static_assert(ScalableOnRightBy<Vector3i, int>);
    static_assert(ScalableOnLeftBy<Vector3i, int>);
    static_assert(ScalableOnRightBy<Vector3r, int>);
    static_assert(ScalableOnRightBy<Vector3r, float>);
    static_assert(ScalableOnLeftBy<Vector3r, double>);
    static_assert(ScalableOnLeftBy<Vector3r, float>);
    static_assert(CompoundDivisibleBy<Vector3r, float>);
    static_assert(CompoundScalableBy<Vector3r, float>);
    static_assert(ScalableOnRightBy<Vector3<float>, float>);
    static_assert(DivisibleBy<Vector3r, float>);
}

TEST(Vector3Test, scalesByScalarOnEitherSide)
{
    constexpr Vector3i v{1, 2, 3};

    static_assert(2 * v == Vector3i{2, 4, 6});
    static_assert(v * 2 == Vector3i{2, 4, 6});
}

TEST(Vector3Test, scalesRealVectorByIntegerLiteral)
{
    constexpr Vector3r v{1.5, 2.0, 3.0};

    expectNear(v * 2, Vector3r{3.0, 4.0, 6.0});
}

TEST(Vector3Test, dividesRealVectorByScalar)
{
    constexpr Vector3r v{2.0, 4.0, 6.0};

    expectNear(v / 2.0, Vector3r{1.0, 2.0, 3.0});
}

TEST(Vector3Test, integerVectorHasNoDivision)
{
    // Integer division would silently truncate; callers must convert first.
    static_assert(!DivisibleByScalar<Vector3i>);
    static_assert(DivisibleByScalar<Vector3r>);
}

TEST(Vector3Test, compoundAssignmentMatchesBinaryOperators)
{
    Vector3r v{1.0, 2.0, 3.0};

    v += Vector3r{1.0, 1.0, 1.0};
    v -= Vector3r{0.5, 0.5, 0.5};
    v *= 2.0;
    v /= 4.0;

    expectNear(v, Vector3r{0.75, 1.25, 1.75});
}

TEST(Vector3Test, dotOfOrthogonalVectorsIsZero)
{
    constexpr Vector3i x_axis{1, 0, 0};
    constexpr Vector3i y_axis{0, 1, 0};

    static_assert(dot(x_axis, y_axis) == 0);
}

TEST(Vector3Test, dotSumsComponentProducts)
{
    constexpr Vector3i a{1, 2, 3};
    constexpr Vector3i b{4, -5, 6};

    static_assert(dot(a, b) == 12);
}

TEST(Vector3Test, crossOfAxesFollowsRightHandRule)
{
    constexpr Vector3i x_axis{1, 0, 0};
    constexpr Vector3i y_axis{0, 1, 0};

    static_assert(cross(x_axis, y_axis) == Vector3i{0, 0, 1});
    static_assert(cross(y_axis, x_axis) == Vector3i{0, 0, -1});
}

TEST(Vector3Test, crossIsOrthogonalToBothOperands)
{
    constexpr Vector3r a{1.0, 2.0, 3.0};
    constexpr Vector3r b{-2.0, 0.5, 4.0};

    const Vector3r c{cross(a, b)};

    EXPECT_NEAR(dot(c, a), 0.0, default_tolerance);
    EXPECT_NEAR(dot(c, b), 0.0, default_tolerance);
}

TEST(Vector3Test, crossOfParallelVectorsIsZero)
{
    constexpr Vector3i a{1, 2, 3};

    static_assert(cross(a, 2 * a) == Vector3i{});
}

TEST(Vector3Test, lengthSquaredSumsSquares)
{
    constexpr Vector3i v{1, 2, 2};

    static_assert(lengthSquared(v) == 9);
}

TEST(Vector3Test, lengthIsEuclideanNorm)
{
    constexpr Vector3r v{3.0, 4.0, 12.0};

    EXPECT_NEAR(length(v), 13.0, default_tolerance);
}

TEST(Vector3Test, lengthOfZeroVectorIsZero)
{
    EXPECT_NEAR(length(Vector3r{}), 0.0, default_tolerance);
}

TEST(Vector3Test, normalizedHasUnitLengthAndSameDirection)
{
    constexpr Vector3r v{0.0, 3.0, 4.0};

    const Vector3r n{normalized(v)};

    expectNear(n, Vector3r{0.0, 0.6, 0.8});
}

TEST(Vector3Test, lengthAndNormalizedRequireFloatingPoint)
{
    static_assert(!HasLength<Vector3i>);
    static_assert(!HasNormalized<Vector3i>);
    static_assert(HasLength<Vector3r>);
    static_assert(HasNormalized<Vector3r>);
}

TEST(Vector3DeathTest, normalizedAssertsOnZeroVector)
{
    EXPECT_DEBUG_DEATH(static_cast<void>(normalized(Vector3r{})), "");
}

} // namespace
} // namespace albedo
