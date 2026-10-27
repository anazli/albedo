#include "albedo/geometry/mesh_geometry.hpp"

#include "albedo/geometry/triangle_mesh.hpp"
#include "support/expect_near.hpp"
#include "support/require_value.hpp"

#include <gtest/gtest.h>

#include <limits>
#include <stdexcept>

namespace albedo {
namespace {

using test_support::default_tolerance;
using test_support::exact_tolerance;
using test_support::expectNear;
using test_support::requireValue;

constexpr Interval positive_t{.min = 0.0, .max = std::numeric_limits<Real>::infinity()};

// Looking down -z from the origin.
constexpr Ray forward_ray{.origin = {0.0, 0.0, 0.0}, .direction = {0.0, 0.0, -1.0}};

// A 2 x 2 square facing +z at depth `z`, with texture coordinates spanning the
// unit square, plus a face without area that the geometry must skip.
TriangleMesh squareAt(Real z)
{
    return TriangleMesh{{{-1.0, -1.0, z}, {1.0, -1.0, z}, {1.0, 1.0, z}, {-1.0, 1.0, z}},
                        {{0, 1, 2}, {0, 2, 3}, {0, 0, 1}},
                        {{0.0, 0.0}, {1.0, 0.0}, {1.0, 1.0}, {0.0, 1.0}},
                        {{0, 1, 2}, {0, 2, 3}, {0, 0, 1}}};
}

// Two squares one behind the other, so only the nearer may be reported.
TriangleMesh twoSquares()
{
    return TriangleMesh{{{-1.0, -1.0, -8.0},
                         {1.0, -1.0, -8.0},
                         {1.0, 1.0, -8.0},
                         {-1.0, 1.0, -8.0},
                         {-1.0, -1.0, -5.0},
                         {1.0, -1.0, -5.0},
                         {1.0, 1.0, -5.0},
                         {-1.0, 1.0, -5.0}},
                        {{0, 1, 2}, {0, 2, 3}, {4, 5, 6}, {4, 6, 7}}};
}

TEST(MeshGeometryTest, keepsTheTrianglesWithArea)
{
    const MeshGeometry geometry{squareAt(-5.0)};

    EXPECT_EQ(geometry.triangleCount(), 2U);
}

TEST(MeshGeometryTest, rejectsAnEmptyMesh)
{
    EXPECT_THROW(MeshGeometry(TriangleMesh{{}, {}}), std::invalid_argument);
}

TEST(MeshGeometryTest, rejectsAMeshWhoseFacesHaveNoArea)
{
    EXPECT_THROW(MeshGeometry(TriangleMesh{{{0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}}, {{0, 0, 1}}}),
                 std::invalid_argument);
}

TEST(MeshGeometryTest, boundsEncloseTheTriangles)
{
    const Bounds3r box{MeshGeometry{twoSquares()}.bounds()};

    expectNear(box.min, Point3r{-1.0, -1.0, -8.0}, exact_tolerance);
    expectNear(box.max, Point3r{1.0, 1.0, -5.0}, exact_tolerance);
}

TEST(MeshGeometryTest, reportsTheNearestTriangle)
{
    const ShapeHit hit{requireValue(MeshGeometry{twoSquares()}.intersect(forward_ray, positive_t))};

    EXPECT_NEAR(hit.t, 5.0, default_tolerance);
    expectNear(hit.point, Point3r{0.0, 0.0, -5.0});
    expectNear(hit.normal, Normal3r{0.0, 0.0, 1.0});
}

TEST(MeshGeometryTest, respectsTheParameterRange)
{
    const MeshGeometry geometry{twoSquares()};

    const ShapeHit farther{
        requireValue(geometry.intersect(forward_ray, Interval{.min = 6.0, .max = 20.0}))};

    EXPECT_NEAR(farther.t, 8.0, default_tolerance);
    EXPECT_FALSE(geometry.intersect(forward_ray, Interval{.min = 0.0, .max = 4.0}).has_value());
}

TEST(MeshGeometryTest, rayPastTheTrianglesMisses)
{
    const Ray beside{.origin = {3.0, 0.0, 0.0}, .direction = {0.0, 0.0, -1.0}};

    EXPECT_FALSE(MeshGeometry{twoSquares()}.intersect(beside, positive_t).has_value());
}

TEST(MeshGeometryTest, hitCarriesTheTextureCoordinatesOfTheTriangle)
{
    const Ray ray{.origin = {0.5, -0.5, 0.0}, .direction = {0.0, 0.0, -1.0}};

    const ShapeHit hit{requireValue(MeshGeometry{squareAt(-5.0)}.intersect(ray, positive_t))};

    expectNear(hit.uv, Point2r{0.75, 0.25});
}

} // namespace
} // namespace albedo
