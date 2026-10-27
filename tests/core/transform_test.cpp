#include "albedo/core/transform.hpp"

#include "support/expect_near.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <limits>
#include <stdexcept>

namespace albedo {
namespace {

using test_support::default_tolerance;
using test_support::exact_tolerance;
using test_support::expectNear;

constexpr Real nan{std::numeric_limits<Real>::quiet_NaN()};
constexpr Real infinity{std::numeric_limits<Real>::infinity()};

// A transform mixing all three kinds, so composition and inversion are not
// trivially exact. A function so a throwing factory fails a test, not startup.
Transform mixedTransform()
{
    return Transform::translation({1.0, -2.0, 3.0}) * Transform::rotation({1.0, 2.0, -0.5}, 37.0) *
           Transform::scaling({2.0, 0.5, 3.0});
}

TEST(TransformTest, identityLeavesEverythingUnchanged)
{
    const Transform identity;

    expectNear(identity.apply(Point3r{1.5, -2.0, 3.0}), Point3r{1.5, -2.0, 3.0}, exact_tolerance);
    expectNear(identity.apply(Vector3r{0.5, 4.0, -1.0}), Vector3r{0.5, 4.0, -1.0}, exact_tolerance);
    expectNear(identity.apply(Normal3r{0.0, 1.0, 0.0}), Normal3r{0.0, 1.0, 0.0}, exact_tolerance);
}

TEST(TransformTest, translationMovesPointsButNotVectorsOrNormals)
{
    const Transform move{Transform::translation({1.0, 2.0, 3.0})};

    expectNear(move.apply(Point3r{1.0, 1.0, 1.0}), Point3r{2.0, 3.0, 4.0}, exact_tolerance);
    expectNear(move.apply(Vector3r{1.0, 1.0, 1.0}), Vector3r{1.0, 1.0, 1.0}, exact_tolerance);
    expectNear(move.apply(Normal3r{0.0, 0.0, 1.0}), Normal3r{0.0, 0.0, 1.0}, exact_tolerance);
}

TEST(TransformTest, scalingScalesPointsAndVectorsPerAxis)
{
    const Transform scale{Transform::scaling({2.0, 3.0, -4.0})};

    expectNear(scale.apply(Point3r{1.0, 1.0, 1.0}), Point3r{2.0, 3.0, -4.0}, exact_tolerance);
    expectNear(scale.apply(Vector3r{0.5, -1.0, 0.25}), Vector3r{1.0, -3.0, -1.0}, exact_tolerance);
}

TEST(TransformTest, normalsStayPerpendicularUnderNonUniformScaling)
{
    // The plane x + y = 0, squashed along x: its normal must tilt, not just scale.
    const Transform squash{Transform::scaling({0.25, 1.0, 1.0})};
    const Normal3r normal{1.0, 1.0, 0.0};
    const Vector3r along_plane{1.0, -1.0, 0.0};

    const Normal3r moved_normal{squash.apply(normal)};

    EXPECT_NEAR(dot(moved_normal, squash.apply(along_plane)), 0.0, default_tolerance);
    expectNear(moved_normal, Normal3r{4.0, 1.0, 0.0}, default_tolerance);
}

// Expects `transform` to keep the normal `n` perpendicular to the tangents of
// its surface.
void expectNormalStaysPerpendicular(const Transform& transform, Normal3r n,
                                    const std::array<Vector3r, 2>& tangents)
{
    const Normal3r moved{transform.apply(n)};

    for (const Vector3r& tangent : tangents) {
        EXPECT_NEAR(dot(moved, transform.apply(tangent)), 0.0, default_tolerance);
    }
}

// A normal and two independent tangents of the plane x + 2y - z = 0.
constexpr Normal3r plane_normal{1.0, 2.0, -1.0};
constexpr std::array<Vector3r, 2> plane_tangents{{{1.0, 0.0, 1.0}, {2.0, -1.0, 0.0}}};

TEST(TransformTest, normalsStayPerpendicularUnderAGeneralTransform)
{
    // mixedTransform's matrix is not symmetric, so this needs the transpose in the
    // inverse transpose.
    expectNormalStaysPerpendicular(mixedTransform(), plane_normal, plane_tangents);
}

TEST(TransformTest, normalsStayPerpendicularUnderAMirroring)
{
    const Transform mirror{Transform::rotation({0.3, 1.0, 0.2}, 50.0) *
                           Transform::scaling({-1.0, 2.0, 0.5})};

    expectNormalStaysPerpendicular(mirror, plane_normal, plane_tangents);
}

TEST(TransformTest, rotationFollowsTheRightHandRule)
{
    // A quarter turn about +z takes +x to +y and +y to -x.
    const Transform turn{Transform::rotation({0.0, 0.0, 1.0}, 90.0)};

    expectNear(turn.apply(Vector3r{1.0, 0.0, 0.0}), Vector3r{0.0, 1.0, 0.0});
    expectNear(turn.apply(Vector3r{0.0, 1.0, 0.0}), Vector3r{-1.0, 0.0, 0.0});
    expectNear(turn.apply(Vector3r{0.0, 0.0, 1.0}), Vector3r{0.0, 0.0, 1.0});
}

TEST(TransformTest, rotationsAboutXAndYFollowTheRightHandRule)
{
    // A quarter turn about +x takes +y to +z; one about +y takes +z to +x.
    const Transform about_x{Transform::rotation({1.0, 0.0, 0.0}, 90.0)};
    const Transform about_y{Transform::rotation({0.0, 1.0, 0.0}, 90.0)};

    expectNear(about_x.apply(Vector3r{0.0, 1.0, 0.0}), Vector3r{0.0, 0.0, 1.0});
    expectNear(about_y.apply(Vector3r{0.0, 0.0, 1.0}), Vector3r{1.0, 0.0, 0.0});
}

TEST(TransformTest, rotationAxisNeedNotBeNormalized)
{
    const Transform turn{Transform::rotation({0.0, 0.0, 5.0}, 90.0)};

    expectNear(turn.apply(Vector3r{1.0, 0.0, 0.0}), Vector3r{0.0, 1.0, 0.0});
}

TEST(TransformTest, rotationKeepsTheAxisAndLengths)
{
    const Vector3r axis{1.0, 2.0, -0.5};
    const Transform turn{Transform::rotation(axis, 123.0)};
    const Vector3r v{0.3, -1.7, 2.2};

    expectNear(turn.apply(axis), axis);
    EXPECT_NEAR(length(turn.apply(v)), length(v), default_tolerance);
}

TEST(TransformTest, compositionAppliesTheRightOperandFirst)
{
    const Transform move{Transform::translation({1.0, 0.0, 0.0})};
    const Transform scale{Transform::scaling({2.0, 2.0, 2.0})};
    const Point3r p{1.0, 1.0, 1.0};

    expectNear((move * scale).apply(p), Point3r{3.0, 2.0, 2.0}, exact_tolerance);
    expectNear((scale * move).apply(p), Point3r{4.0, 2.0, 2.0}, exact_tolerance);
}

TEST(TransformTest, inverseUndoesTheTransform)
{
    const Transform transform{mixedTransform()};
    const Transform inverse{transform.inverse()};
    const Point3r p{0.7, -1.3, 2.9};
    const Vector3r v{-0.4, 2.5, 1.1};
    const Normal3r n{0.2, -0.9, 0.4};

    expectNear(inverse.apply(transform.apply(p)), p);
    expectNear(inverse.apply(transform.apply(v)), v);
    expectNear(inverse.apply(transform.apply(n)), n);
}

TEST(TransformTest, composingWithTheInverseGivesTheIdentity)
{
    const Transform transform{mixedTransform()};
    const Point3r p{0.7, -1.3, 2.9};

    expectNear((transform * transform.inverse()).apply(p), p);
    expectNear((transform.inverse() * transform).apply(p), p);
}

TEST(TransformTest, inverseOfAProductIsTheProductOfInversesReversed)
{
    const Transform a{Transform::rotation({0.0, 1.0, 0.0}, 30.0)};
    const Transform b{Transform::translation({1.0, 2.0, 3.0})};
    const Point3r p{0.5, 0.25, -1.0};

    expectNear((a * b).inverse().apply(p), (b.inverse() * a.inverse()).apply(p));
}

TEST(TransformTest, transformedRayKeepsItsParameter)
{
    // The direction is not renormalized, so the same t names the same point.
    const Transform transform{mixedTransform()};
    const Ray ray{.origin = {0.1, 0.2, 0.3}, .direction = {1.0, -2.0, 0.5}};
    constexpr Real t{1.75};

    const Ray moved{transform.apply(ray)};

    expectNear(moved.at(t), transform.apply(ray.at(t)));
}

TEST(TransformTest, boundsEncloseTheTransformedBox)
{
    // The unit cube turned 45 degrees about z spans sqrt(2) along x and y.
    const Transform turn{Transform::rotation({0.0, 0.0, 1.0}, 45.0)};
    const Bounds3r cube{.min = {0.0, 0.0, 0.0}, .max = {1.0, 1.0, 1.0}};
    const Real half_diagonal{std::sqrt(0.5)};

    const Bounds3r moved{turn.apply(cube)};

    expectNear(moved.min, Point3r{-half_diagonal, 0.0, 0.0});
    expectNear(moved.max, Point3r{half_diagonal, 2 * half_diagonal, 1.0});
}

// Expects `p` inside `box`, with no allowance for rounding.
void expectInside(Point3r p, const Bounds3r& box)
{
    EXPECT_TRUE(box.contains(p)) << "point (" << p.x << ", " << p.y << ", " << p.z << ")";
}

TEST(TransformTest, boundsContainEveryTransformedCorner)
{
    const Transform transform{mixedTransform()};
    const Bounds3r box{.min = {-1.0, 0.5, 2.0}, .max = {3.0, 1.5, 2.5}};

    const Bounds3r moved{transform.apply(box)};

    for (const Real x : {box.min.x, box.max.x}) {
        for (const Real y : {box.min.y, box.max.y}) {
            for (const Real z : {box.min.z, box.max.z}) {
                expectInside(transform.apply(Point3r{x, y, z}), moved);
            }
        }
    }
}

TEST(TransformTest, emptyBoundsStayEmpty)
{
    EXPECT_TRUE(mixedTransform().apply(Bounds3r{}).isEmpty());
}

TEST(TransformTest, translationRejectsNonFiniteOffsets)
{
    EXPECT_THROW(static_cast<void>(Transform::translation({nan, 0.0, 0.0})), std::invalid_argument);
    EXPECT_THROW(static_cast<void>(Transform::translation({0.0, 0.0, -infinity})),
                 std::invalid_argument);
}

// Expects scaling y by `factor` to be rejected.
void expectScalingRejects(Real factor)
{
    EXPECT_THROW(static_cast<void>(Transform::scaling({1.0, factor, 1.0})), std::invalid_argument)
        << "factor: " << factor;
}

TEST(TransformTest, scalingRejectsFactorsWithoutAFiniteInverse)
{
    for (const Real factor : {0.0, -0.0, nan, infinity, 1e-320}) {
        expectScalingRejects(factor);
    }
}

TEST(TransformTest, rotationRejectsUnusableAxesAndAngles)
{
    EXPECT_THROW(static_cast<void>(Transform::rotation({0.0, 0.0, 0.0}, 30.0)),
                 std::invalid_argument);
    EXPECT_THROW(static_cast<void>(Transform::rotation({nan, 1.0, 0.0}, 30.0)),
                 std::invalid_argument);
    EXPECT_THROW(static_cast<void>(Transform::rotation({1e200, 0.0, 0.0}, 30.0)),
                 std::invalid_argument);
    EXPECT_THROW(static_cast<void>(Transform::rotation({1e-160, 0.0, 0.0}, 30.0)),
                 std::invalid_argument);
    EXPECT_THROW(static_cast<void>(Transform::rotation({0.0, 1.0, 0.0}, infinity)),
                 std::invalid_argument);
    EXPECT_THROW(static_cast<void>(Transform::rotation({0.0, 1.0, 0.0}, nan)),
                 std::invalid_argument);
}

TEST(TransformTest, compositionRejectsProductsThatOverflow)
{
    const Transform huge{Transform::scaling({1e200, 1.0, 1.0})};
    const Transform tiny{Transform::scaling({1e-200, 1.0, 1.0})};

    // The product overflows in one case, its inverse in the other.
    EXPECT_THROW(static_cast<void>(huge * huge), std::invalid_argument);
    EXPECT_THROW(static_cast<void>(tiny * tiny), std::invalid_argument);
}

} // namespace
} // namespace albedo
