#include "albedo/geometry/mesh_instance.hpp"

#include "albedo/core/transform.hpp"
#include "albedo/geometry/mesh_geometry.hpp"
#include "albedo/geometry/triangle_mesh.hpp"
#include "support/expect_near.hpp"
#include "support/require_value.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <memory>
#include <stdexcept>

namespace albedo {
namespace {

using test_support::default_tolerance;
using test_support::expectNear;
using test_support::requireValue;

constexpr Interval positive_t{.min = 0.0, .max = std::numeric_limits<Real>::infinity()};

// Looking down -z from the origin.
constexpr Ray forward_ray{.origin = {0.0, 0.0, 0.0}, .direction = {0.0, 0.0, -1.0}};

// A shared 2 x 2 square facing +z at depth -5, texture coordinates spanning the
// unit square.
std::shared_ptr<const MeshGeometry> square()
{
    return std::make_shared<const MeshGeometry>(
        TriangleMesh{{{-1.0, -1.0, -5.0}, {1.0, -1.0, -5.0}, {1.0, 1.0, -5.0}, {-1.0, 1.0, -5.0}},
                     {{0, 1, 2}, {0, 2, 3}},
                     {{0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}},
                     {{0, 1, 2}, {0, 2, 3}}});
}

TEST(MeshInstanceTest, identityInstanceHitsLikeTheMesh)
{
    const MeshInstance instance{square(), Transform{}};

    const ShapeHit hit{requireValue(instance.intersect(forward_ray, positive_t))};

    EXPECT_NEAR(hit.t, 5.0, default_tolerance);
    expectNear(hit.point, Point3r{0.0, 0.0, -5.0});
    expectNear(hit.normal, Normal3r{0.0, 0.0, 1.0});
}

TEST(MeshInstanceTest, translatedInstanceIsHitWhereItWasMoved)
{
    const MeshInstance instance{square(), Transform::translation({3.0, 0.0, -2.0})};
    const Ray towards_moved{.origin = {3.0, 0.0, 0.0}, .direction = {0.0, 0.0, -1.0}};

    const ShapeHit hit{requireValue(instance.intersect(towards_moved, positive_t))};

    EXPECT_FALSE(instance.intersect(forward_ray, positive_t).has_value());
    EXPECT_NEAR(hit.t, 7.0, default_tolerance);
    expectNear(hit.point, Point3r{3.0, 0.0, -7.0});
}

TEST(MeshInstanceTest, scaledInstanceReportsTheWorldRayParameter)
{
    // Doubling everything moves the square to depth -10.
    const MeshInstance instance{square(), Transform::scaling({2.0, 2.0, 2.0})};

    const ShapeHit hit{requireValue(instance.intersect(forward_ray, positive_t))};

    EXPECT_NEAR(hit.t, 10.0, default_tolerance);
    expectNear(hit.point, Point3r{0.0, 0.0, -10.0});
}

TEST(MeshInstanceTest, parameterRangeIsInWorldUnits)
{
    const MeshInstance instance{square(), Transform::scaling({2.0, 2.0, 2.0})};

    EXPECT_FALSE(instance.intersect(forward_ray, Interval{.min = 0.0, .max = 9.0}).has_value());
}

TEST(MeshInstanceTest, rotatedInstanceTurnsItsNormal)
{
    // A quarter turn about +y takes the square to x = -5, facing +x.
    const MeshInstance instance{square(), Transform::rotation({0.0, 1.0, 0.0}, 90.0)};
    const Ray towards_minus_x{.origin = {0.0, 0.0, 0.0}, .direction = {-1.0, 0.0, 0.0}};

    const ShapeHit hit{requireValue(instance.intersect(towards_minus_x, positive_t))};

    EXPECT_NEAR(hit.t, 5.0, default_tolerance);
    expectNear(hit.normal, Normal3r{1.0, 0.0, 0.0});
}

TEST(MeshInstanceTest, normalIsUnitAndPerpendicularUnderNonUniformScaling)
{
    // Tilted so the scaling changes the normal's direction, not just its length.
    // The ray aims at where the square's center ends up.
    const Transform placement{Transform::scaling({3.0, 0.5, 1.0}) *
                              Transform::rotation({1.0, 1.0, 0.0}, 30.0)};
    const MeshInstance instance{square(), placement};
    const Ray towards_center{.origin = {0.0, 0.0, 0.0},
                             .direction = placement.apply(Point3r{0.0, 0.0, -5.0}) - Point3r{}};

    const ShapeHit hit{requireValue(instance.intersect(towards_center, positive_t))};

    EXPECT_NEAR(length(asVector(hit.normal)), 1.0, default_tolerance);
    EXPECT_NEAR(dot(hit.normal, placement.apply(Vector3r{1.0, 0.0, 0.0})), 0.0, default_tolerance);
    EXPECT_NEAR(dot(hit.normal, placement.apply(Vector3r{0.0, 1.0, 0.0})), 0.0, default_tolerance);
}

TEST(MeshInstanceTest, hitWhoseNormalCannotBeNormalizedIsAMiss)
{
    // Flattening z by 1e-160 stretches the normal to about 1e160, whose squared
    // length overflows. The tiny ray direction is (0, 0, -1) in the mesh's space,
    // so the triangle itself is hit.
    const MeshInstance instance{square(), Transform::scaling({1.0, 1.0, 1e-160})};
    const Ray ray{.origin = {0.0, 0.0, 0.0}, .direction = {0.0, 0.0, -1e-160}};

    EXPECT_FALSE(instance.intersect(ray, positive_t).has_value());
}

TEST(MeshInstanceTest, mirroredInstanceKeepsItsFrontFacingTheSameWay)
{
    // Mirroring z puts the square at depth +5; the side that faced the origin
    // still does, so its normal now points along -z.
    const MeshInstance instance{square(), Transform::scaling({1.0, 1.0, -1.0})};
    const Ray backward{.origin = {0.0, 0.0, 0.0}, .direction = {0.0, 0.0, 1.0}};

    const ShapeHit hit{requireValue(instance.intersect(backward, positive_t))};

    expectNear(hit.normal, Normal3r{0.0, 0.0, -1.0});
}

TEST(MeshInstanceTest, textureCoordinatesComeFromTheMesh)
{
    const MeshInstance instance{square(), Transform::scaling({2.0, 2.0, 2.0})};
    // Hits object-space (0.5, -0.5), a quarter of the way into the square from +x.
    const Ray ray{.origin = {1.0, -1.0, 0.0}, .direction = {0.0, 0.0, -1.0}};

    const ShapeHit hit{requireValue(instance.intersect(ray, positive_t))};

    expectNear(hit.uv, Point2r{0.75, 0.25});
}

TEST(MeshInstanceTest, boundsAreTheTransformedMeshBounds)
{
    const MeshInstance instance{square(), Transform::translation({1.0, 2.0, 3.0}) *
                                              Transform::scaling({2.0, 1.0, 1.0})};

    const Bounds3r box{instance.bounds()};

    expectNear(box.min, Point3r{-1.0, 1.0, -2.0});
    expectNear(box.max, Point3r{3.0, 3.0, -2.0});
}

TEST(MeshInstanceTest, instancesShareTheirMesh)
{
    const std::shared_ptr<const MeshGeometry> shared{square()};

    const MeshInstance first{shared, Transform{}};
    const MeshInstance second{shared, Transform::translation({5.0, 0.0, 0.0})};

    EXPECT_EQ(&first.mesh(), shared.get());
    EXPECT_EQ(&second.mesh(), shared.get());
}

TEST(MeshInstanceTest, rejectsAMissingMesh)
{
    EXPECT_THROW(MeshInstance(nullptr, Transform{}), std::invalid_argument);
}

TEST(MeshInstanceTest, rejectsATransformWhoseBoundsOverflow)
{
    // Edges of 1e70 still form a triangle; scaled by 1e300 they overflow.
    const auto large{std::make_shared<const MeshGeometry>(
        TriangleMesh{{{0.0, 0.0, 0.0}, {1e70, 0.0, 0.0}, {0.0, 1e70, 0.0}}, {{0, 1, 2}}})};

    EXPECT_THROW(MeshInstance(large, Transform::scaling({1e300, 1.0, 1.0})), std::invalid_argument);
}

} // namespace
} // namespace albedo
