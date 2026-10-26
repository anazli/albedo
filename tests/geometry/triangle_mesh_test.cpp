#include "albedo/geometry/triangle_mesh.hpp"

#include "support/expect_near.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <limits>
#include <stdexcept>
#include <vector>

namespace albedo {
namespace {

using test_support::exact_tolerance;
using test_support::expectNear;

// A unit square in the plane z = 0 split along its diagonal into two
// counter-clockwise triangles seen from +z.
TriangleMesh unitSquare()
{
    return TriangleMesh{{{0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, {1.0, 1.0, 0.0}, {0.0, 1.0, 0.0}},
                        {{0, 1, 2}, {0, 2, 3}}};
}

TEST(TriangleMeshTest, holdsItsPositionsAndFaces)
{
    const TriangleMesh mesh{unitSquare()};

    ASSERT_EQ(mesh.positions().size(), 4U);
    expectNear(mesh.positions()[2], Point3r{1.0, 1.0, 0.0}, exact_tolerance);
    ASSERT_EQ(mesh.faces().size(), 2U);
    EXPECT_EQ(mesh.faces()[1], (TriangleMesh::Face{0, 2, 3}));
}

TEST(TriangleMeshTest, emptyMeshHasNoTriangles)
{
    const TriangleMesh mesh{{}, {}};

    EXPECT_TRUE(mesh.triangles().empty());
}

TEST(TriangleMeshTest, buildsOneTrianglePerFaceInOrder)
{
    const std::vector<Triangle> triangles{unitSquare().triangles()};

    ASSERT_EQ(triangles.size(), 2U);
    expectNear(triangles[0].vertices()[1], Point3r{1.0, 0.0, 0.0}, exact_tolerance);
    expectNear(triangles[1].vertices()[2], Point3r{0.0, 1.0, 0.0}, exact_tolerance);
}

TEST(TriangleMeshTest, trianglesFollowTheFaceWinding)
{
    const TriangleMesh mesh{{{0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}},
                            {{0, 1, 2}, {0, 2, 1}}};

    const std::vector<Triangle> triangles{mesh.triangles()};

    ASSERT_EQ(triangles.size(), 2U);
    expectNear(triangles[0].normal(), Normal3r{0.0, 0.0, 1.0});
    expectNear(triangles[1].normal(), Normal3r{0.0, 0.0, -1.0});
}

TEST(TriangleMeshTest, neighboursShareBitwiseIdenticalVertices)
{
    // The watertight triangle test relies on shared edges having identical vertices.
    const std::vector<Triangle> triangles{unitSquare().triangles()};

    ASSERT_EQ(triangles.size(), 2U);
    expectNear(triangles[0].vertices()[0], triangles[1].vertices()[0], exact_tolerance);
    expectNear(triangles[0].vertices()[2], triangles[1].vertices()[1], exact_tolerance);
}

TEST(TriangleMeshTest, skipsFacesWithoutArea)
{
    // Repeated and collinear vertices, as exporters sometimes write.
    const TriangleMesh mesh{{{0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {2.0, 0.0, 0.0}},
                            {{0, 0, 1}, {0, 1, 3}, {0, 1, 2}}};

    const std::vector<Triangle> triangles{mesh.triangles()};

    ASSERT_EQ(triangles.size(), 1U);
    expectNear(triangles[0].vertices()[2], Point3r{0.0, 1.0, 0.0}, exact_tolerance);
}

TEST(TriangleMeshTest, skipsFacesWhoseAreaIsNotRepresentable)
{
    // Finite positions the constructor accepts, but whose edge cross product
    // overflows (first face) or underflows (second face): skipped, not thrown on.
    const TriangleMesh mesh{{{0.0, 0.0, 0.0},
                             {1e200, 0.0, 0.0},
                             {0.0, 1e200, 0.0},
                             {1e-160, 0.0, 0.0},
                             {0.0, 1e-160, 0.0}},
                            {{0, 1, 2}, {0, 3, 4}}};

    EXPECT_TRUE(mesh.triangles().empty());
}

TEST(TriangleMeshTest, allowsUnreferencedPositions)
{
    const TriangleMesh mesh{{{0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}, {5.0, 5.0, 5.0}},
                            {{0, 1, 2}}};

    EXPECT_EQ(mesh.triangles().size(), 1U);
}

TEST(TriangleMeshTest, rejectsAnIndexPastTheLastPosition)
{
    EXPECT_THROW(TriangleMesh({{0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}}, {{0, 1, 3}}),
                 std::out_of_range);
}

TEST(TriangleMeshTest, rejectsTheLargestIndex)
{
    // Guards against index arithmetic that wraps around.
    constexpr std::size_t largest{std::numeric_limits<std::size_t>::max()};

    EXPECT_THROW(
        TriangleMesh({{0.0, 0.0, 0.0}, {1.0, 0.0, 0.0}, {0.0, 1.0, 0.0}}, {{largest, 1, 2}}),
        std::out_of_range);
}

TEST(TriangleMeshTest, rejectsFacesWithoutPositions)
{
    EXPECT_THROW(TriangleMesh({}, {{0, 0, 0}}), std::out_of_range);
}

TEST(TriangleMeshTest, rejectsNonFinitePositions)
{
    constexpr Real nan{std::numeric_limits<Real>::quiet_NaN()};
    constexpr Real infinity{std::numeric_limits<Real>::infinity()};

    EXPECT_THROW(TriangleMesh({{nan, 0.0, 0.0}}, {}), std::invalid_argument);
    EXPECT_THROW(TriangleMesh({{0.0, 0.0, 0.0}, {0.0, -infinity, 0.0}}, {}), std::invalid_argument);
}

} // namespace
} // namespace albedo
