#include "albedo/geometry/mesh_geometry.hpp"

#include "albedo/geometry/triangle_mesh.hpp"
#include "support/expect_near.hpp"
#include "support/require_value.hpp"
#include "support/tent_mesh.hpp"

#include <gtest/gtest.h>

#include <cmath>
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

// Straight down onto the tent's ridge.
constexpr Ray onto_the_ridge{.origin = {0.0, 5.0, 0.0}, .direction = {0.0, -1.0, 0.0}};
// Straight down onto the left slope, halfway between ridge and eave.
constexpr Ray onto_the_left_slope{.origin = {-5.0, 5.0, 0.0}, .direction = {0.0, -1.0, 0.0}};

TEST(MeshGeometryTest, isFlatUnlessAskedToBeSmooth)
{
    EXPECT_EQ(MeshGeometry{test_support::tentMesh()}.shading(), MeshShading::Flat);
    EXPECT_EQ((MeshGeometry{test_support::tentMesh(), MeshShading::Smooth}.shading()),
              MeshShading::Smooth);
}

TEST(MeshGeometryTest, flatHitsShadeWithTheTriangleNormal)
{
    const MeshGeometry geometry{test_support::tentMesh()};

    const ShapeHit hit{requireValue(geometry.intersect(onto_the_left_slope, positive_t))};

    // Exactly equal: flat hits must keep a single normal.
    EXPECT_EQ(hit.shading_normal, hit.normal);
}

TEST(MeshGeometryTest, smoothHitOnTheRidgeShadesWithTheRidgeNormal)
{
    const MeshGeometry geometry{test_support::tentMesh(), MeshShading::Smooth};

    const ShapeHit hit{requireValue(geometry.intersect(onto_the_ridge, positive_t))};

    expectNear(hit.shading_normal, Normal3r{0.0, 1.0, 0.0});
    EXPECT_NEAR(std::abs(hit.normal.x), std::sqrt(0.5), default_tolerance);
}

TEST(MeshGeometryTest, smoothHitInterpolatesBetweenRidgeAndEave)
{
    // Halfway down the slope the ridge's +y and the eave's slope normal weigh
    // the same.
    const MeshGeometry geometry{test_support::tentMesh(), MeshShading::Smooth};

    const ShapeHit hit{requireValue(geometry.intersect(onto_the_left_slope, positive_t))};

    expectNear(hit.shading_normal, normalized(asNormal(Vector3r{0.0, 1.0, 0.0} +
                                                       normalized(Vector3r{-1.0, 1.0, 0.0}))));
    expectNear(hit.normal, normalized(Normal3r{-1.0, 1.0, 0.0}));
}

TEST(MeshGeometryTest, smoothHitUsesTheNormalsOfTheNearestTriangle)
{
    // `twoSquares` with the far square wound the other way, so its corner
    // normals face -z: only the near square's +z may be reported.
    const TriangleMesh squares{{{-1.0, -1.0, -8.0},
                                {1.0, -1.0, -8.0},
                                {1.0, 1.0, -8.0},
                                {-1.0, 1.0, -8.0},
                                {-1.0, -1.0, -5.0},
                                {1.0, -1.0, -5.0},
                                {1.0, 1.0, -5.0},
                                {-1.0, 1.0, -5.0}},
                               {{0, 2, 1}, {0, 3, 2}, {4, 5, 6}, {4, 6, 7}}};
    const MeshGeometry geometry{squares, MeshShading::Smooth};

    const ShapeHit hit{requireValue(geometry.intersect(forward_ray, positive_t))};

    EXPECT_NEAR(hit.t, 5.0, default_tolerance);
    expectNear(hit.shading_normal, Normal3r{0.0, 0.0, 1.0});
}

TEST(MeshGeometryTest, smoothCornerNormalsThatNearlyCancelShadeWithTheTriangleNormal)
{
    // A thin triangle facing +z along the edge from (0, 0, 0) to (1, 0, 0), whose
    // ends each also belong to a huge wall: one facing +x, the other -x, both
    // tilted 1e-8 towards +y. Midway along the edge the corner normals blend to
    // a vector about 1e-8 long, mostly rounding; the triangle's own normal is
    // shown instead of that vector's (0, 0.7, 0.7).
    const TriangleMesh mesh{{{0.0, 0.0, 0.0},
                             {1.0, 0.0, 0.0},
                             {0.5, 0.01, 0.0},
                             {0.0, 0.0, -1000.0},
                             {-1e-5, 1000.0, 0.0},
                             {1.0, 0.0, -1000.0},
                             {1.0 + 1e-5, 1000.0, 0.0}},
                            {{0, 1, 2}, {0, 3, 4}, {1, 6, 5}}};
    const MeshGeometry geometry{mesh, MeshShading::Smooth};
    const Ray onto_the_edge{.origin = {0.5, 0.0, 5.0}, .direction = {0.0, 0.0, -1.0}};

    const ShapeHit hit{requireValue(geometry.intersect(onto_the_edge, positive_t))};

    expectNear(hit.shading_normal, Normal3r{0.0, 0.0, 1.0});
}

} // namespace
} // namespace albedo
