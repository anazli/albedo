#include "albedo/geometry/triangle.hpp"

#include "support/expect_near.hpp"
#include "support/require_value.hpp"

#include <gtest/gtest.h>

#include <array>
#include <limits>
#include <stdexcept>

namespace albedo {
namespace {

using test_support::default_tolerance;
using test_support::exact_tolerance;
using test_support::expectNear;
using test_support::requireValue;

constexpr Interval positive_t{.min = 0.0, .max = std::numeric_limits<Real>::infinity()};

// Right triangle in the plane z = -5, counter-clockwise when seen from the origin,
// so its normal points at the origin (+z). A function rather than a namespace-scope
// constant so a throwing constructor fails a test, not startup.
Triangle facingTriangle()
{
    return Triangle{Point3r{-1.0, -1.0, -5.0}, Point3r{1.0, -1.0, -5.0}, Point3r{-1.0, 1.0, -5.0}};
}

// `facingTriangle` with texture coordinates that make every interpolation weight
// visible in the result.
Triangle facingTriangleWithTextureCoordinates()
{
    return Triangle{Point3r{-1.0, -1.0, -5.0},
                    Point3r{1.0, -1.0, -5.0},
                    Point3r{-1.0, 1.0, -5.0},
                    {{{0.2, 0.1}, {0.8, 0.1}, {0.2, 0.9}}}};
}

TEST(TriangleTest, reportsItsVertices)
{
    const Triangle triangle{facingTriangle()};

    expectNear(triangle.vertices()[0], Point3r{-1.0, -1.0, -5.0});
    expectNear(triangle.vertices()[1], Point3r{1.0, -1.0, -5.0});
    expectNear(triangle.vertices()[2], Point3r{-1.0, 1.0, -5.0});
}

TEST(TriangleTest, normalFollowsCounterClockwiseWinding)
{
    expectNear(facingTriangle().normal(), Normal3r{0.0, 0.0, 1.0});
}

TEST(TriangleTest, reversedWindingFlipsTheNormal)
{
    const Triangle triangle{Point3r{-1.0, -1.0, -5.0}, Point3r{-1.0, 1.0, -5.0},
                            Point3r{1.0, -1.0, -5.0}};

    expectNear(triangle.normal(), Normal3r{0.0, 0.0, -1.0});
}

TEST(TriangleTest, rejectsCoincidentVertices)
{
    EXPECT_THROW(Triangle(Point3r{1.0, 2.0, 3.0}, Point3r{1.0, 2.0, 3.0}, Point3r{0.0, 0.0, 0.0}),
                 std::invalid_argument);
}

TEST(TriangleTest, rejectsCollinearVertices)
{
    EXPECT_THROW(Triangle(Point3r{0.0, 0.0, 0.0}, Point3r{1.0, 1.0, 1.0}, Point3r{2.0, 2.0, 2.0}),
                 std::invalid_argument);
}

TEST(TriangleTest, rejectsNonFiniteVertices)
{
    constexpr Real infinity{std::numeric_limits<Real>::infinity()};
    constexpr Real nan{std::numeric_limits<Real>::quiet_NaN()};

    EXPECT_THROW(
        Triangle(Point3r{infinity, 0.0, 0.0}, Point3r{1.0, 0.0, 0.0}, Point3r{0.0, 1.0, 0.0}),
        std::invalid_argument);
    EXPECT_THROW(Triangle(Point3r{0.0, 0.0, 0.0}, Point3r{1.0, nan, 0.0}, Point3r{0.0, 1.0, 0.0}),
                 std::invalid_argument);
}

TEST(TriangleTest, rejectsTrianglesWhoseAreaCannotBeRepresented)
{
    // The normal comes from the cross product of two edges; its squared length
    // overflows for huge triangles and underflows for tiny ones.
    EXPECT_THROW(
        Triangle(Point3r{0.0, 0.0, 0.0}, Point3r{1e100, 0.0, 0.0}, Point3r{0.0, 1e100, 0.0}),
        std::invalid_argument);
    EXPECT_THROW(
        Triangle(Point3r{0.0, 0.0, 0.0}, Point3r{1e-100, 0.0, 0.0}, Point3r{0.0, 1e-100, 0.0}),
        std::invalid_argument);
}

TEST(TriangleTest, formsTriangleAcceptsWhatTheConstructorAccepts)
{
    EXPECT_TRUE(formsTriangle(Point3r{-1.0, -1.0, -5.0}, Point3r{1.0, -1.0, -5.0},
                              Point3r{-1.0, 1.0, -5.0}));
}

TEST(TriangleTest, formsTriangleRejectsWhatTheConstructorRejects)
{
    constexpr Real nan{std::numeric_limits<Real>::quiet_NaN()};

    EXPECT_FALSE(
        formsTriangle(Point3r{0.0, 0.0, 0.0}, Point3r{1.0, 1.0, 1.0}, Point3r{2.0, 2.0, 2.0}));
    EXPECT_FALSE(
        formsTriangle(Point3r{0.0, 0.0, 0.0}, Point3r{1.0, nan, 0.0}, Point3r{0.0, 1.0, 0.0}));
    EXPECT_FALSE(
        formsTriangle(Point3r{0.0, 0.0, 0.0}, Point3r{1e100, 0.0, 0.0}, Point3r{0.0, 1e100, 0.0}));
    EXPECT_FALSE(formsTriangle(Point3r{0.0, 0.0, 0.0}, Point3r{1e-100, 0.0, 0.0},
                               Point3r{0.0, 1e-100, 0.0}));
    EXPECT_FALSE(formsTriangle(Point3r{0.0, 0.0, 0.0}, Point3r{1.0, 0.0, 0.0},
                               Point3r{0.0, std::numeric_limits<Real>::infinity(), 0.0}));
}

TEST(TriangleTest, rayThroughInteriorHits)
{
    const Ray ray{.origin = {0.0, 0.0, 0.0}, .direction = {-0.1, -0.2, -1.0}};

    const ShapeHit hit{requireValue(facingTriangle().intersect(ray, positive_t))};

    EXPECT_NEAR(hit.t, 5.0, default_tolerance);
    expectNear(hit.point, Point3r{-0.5, -1.0, -5.0});
}

TEST(TriangleTest, hitPointLiesOnThePlane)
{
    // A slanted triangle and an oblique ray, so the point is not trivially exact.
    const Triangle triangle{Point3r{0.3, -0.7, -4.1}, Point3r{1.9, 0.2, -6.3},
                            Point3r{-1.1, 1.3, -5.2}};
    const Ray ray{.origin = {0.1, 0.2, 0.3}, .direction = {0.05, 0.07, -1.0}};

    const ShapeHit hit{requireValue(triangle.intersect(ray, positive_t))};

    EXPECT_NEAR(dot(hit.point - triangle.vertices()[0], triangle.normal()), 0.0, default_tolerance);
    expectNear(hit.point, ray.at(hit.t));
}

TEST(TriangleTest, hitReportsTheWindingNormalFromEitherSide)
{
    const Ray from_front{.origin = {-0.5, -0.5, 0.0}, .direction = {0.0, 0.0, -1.0}};
    const Ray from_back{.origin = {-0.5, -0.5, -10.0}, .direction = {0.0, 0.0, 1.0}};

    const ShapeHit front_hit{requireValue(facingTriangle().intersect(from_front, positive_t))};
    const ShapeHit back_hit{requireValue(facingTriangle().intersect(from_back, positive_t))};

    expectNear(front_hit.normal, Normal3r{0.0, 0.0, 1.0});
    expectNear(back_hit.normal, Normal3r{0.0, 0.0, 1.0});
    EXPECT_NEAR(back_hit.t, 5.0, default_tolerance);
}

TEST(TriangleTest, rayBesideTheHypotenuseMisses)
{
    // Just past the hypotenuse x + y = 0.
    const Ray ray{.origin = {0.01, 0.01, 0.0}, .direction = {0.0, 0.0, -1.0}};

    EXPECT_FALSE(facingTriangle().intersect(ray, positive_t).has_value());
}

TEST(TriangleTest, rayBesideTheBottomEdgeMisses)
{
    // Just past y = -1.
    const Ray ray{.origin = {0.0, -1.01, 0.0}, .direction = {0.0, 0.0, -1.0}};

    EXPECT_FALSE(facingTriangle().intersect(ray, positive_t).has_value());
}

TEST(TriangleTest, rayBesideTheLeftEdgeMisses)
{
    // Just past x = -1.
    const Ray ray{.origin = {-1.01, 0.0, 0.0}, .direction = {0.0, 0.0, -1.0}};

    EXPECT_FALSE(facingTriangle().intersect(ray, positive_t).has_value());
}

TEST(TriangleTest, rayThroughAVertexHits)
{
    const Ray ray{.origin = {1.0, -1.0, 0.0}, .direction = {0.0, 0.0, -1.0}};

    const ShapeHit hit{requireValue(facingTriangle().intersect(ray, positive_t))};

    EXPECT_NEAR(hit.t, 5.0, default_tolerance);
}

TEST(TriangleTest, rayParallelToThePlaneMisses)
{
    const Ray ray{.origin = {-0.5, -0.5, -5.0}, .direction = {1.0, 0.0, 0.0}};

    EXPECT_FALSE(facingTriangle().intersect(ray, positive_t).has_value());
}

TEST(TriangleTest, rayPointingAwayMisses)
{
    const Ray ray{.origin = {-0.5, -0.5, 0.0}, .direction = {0.0, 0.0, 1.0}};

    EXPECT_FALSE(facingTriangle().intersect(ray, positive_t).has_value());
}

TEST(TriangleTest, zeroLengthDirectionMisses)
{
    const Ray ray{.origin = {-0.5, -0.5, 0.0}, .direction = {0.0, 0.0, 0.0}};

    EXPECT_FALSE(facingTriangle().intersect(ray, positive_t).has_value());
}

TEST(TriangleTest, subnormalDirectionMisses)
{
    const Ray ray{.origin = {-0.5, -0.5, 0.0}, .direction = {0.0, 0.0, -1e-160}};

    EXPECT_FALSE(facingTriangle().intersect(ray, positive_t).has_value());
}

TEST(TriangleTest, nonFiniteDirectionMisses)
{
    const Ray ray{.origin = {-0.5, -0.5, 0.0},
                  .direction = {0.0, 0.0, -std::numeric_limits<Real>::infinity()}};

    EXPECT_FALSE(facingTriangle().intersect(ray, positive_t).has_value());
}

TEST(TriangleTest, nonFiniteOriginMisses)
{
    constexpr Real nan{std::numeric_limits<Real>::quiet_NaN()};
    constexpr Real infinity{std::numeric_limits<Real>::infinity()};
    const Ray nan_origin{.origin = {nan, -0.5, 0.0}, .direction = {0.0, 0.0, -1.0}};
    const Ray infinite_origin{.origin = {-0.5, -0.5, infinity}, .direction = {0.0, 0.0, -1.0}};

    EXPECT_FALSE(facingTriangle().intersect(nan_origin, positive_t).has_value());
    EXPECT_FALSE(facingTriangle().intersect(infinite_origin, positive_t).has_value());
}

TEST(TriangleTest, emptyIntervalMisses)
{
    const Ray ray{.origin = {-0.5, -0.5, 0.0}, .direction = {0.0, 0.0, -1.0}};

    EXPECT_FALSE(facingTriangle().intersect(ray, empty_interval).has_value());
}

TEST(TriangleTest, hitExactlyAtIntervalMaxIsRejected)
{
    const Ray ray{.origin = {-0.5, -0.5, 0.0}, .direction = {0.0, 0.0, -1.0}};

    EXPECT_FALSE(facingTriangle().intersect(ray, Interval{.min = 0.0, .max = 5.0}).has_value());
}

TEST(TriangleTest, originOnTheSurfaceDoesNotHitAtZero)
{
    // A ray leaving the surface must not re-hit it at t = 0.
    const Ray ray{.origin = {-0.5, -0.5, -5.0}, .direction = {0.0, 0.0, 1.0}};

    EXPECT_FALSE(facingTriangle().intersect(ray, positive_t).has_value());
}

TEST(TriangleTest, unnormalizedDirectionScalesParameter)
{
    const Ray ray{.origin = {-0.5, -0.5, 0.0}, .direction = {0.0, 0.0, -2.0}};

    const ShapeHit hit{requireValue(facingTriangle().intersect(ray, positive_t))};

    EXPECT_NEAR(hit.t, 2.5, default_tolerance);
    expectNear(hit.point, Point3r{-0.5, -0.5, -5.0});
}

TEST(TriangleTest, hitsAlongEveryDominantAxisAndSign)
{
    // The algorithm permutes axes by the ray's largest direction component and
    // swaps two of them for a negative one; every case must find the same hit.
    const Triangle triangle{Point3r{1.0, 0.0, 0.0}, Point3r{0.0, 1.0, 0.0}, Point3r{0.0, 0.0, 1.0}};
    const Point3r target{1.0 / 3.0, 1.0 / 3.0, 1.0 / 3.0};
    const std::array<Vector3r, 6> offsets{{{3.0, 0.2, 0.1},
                                           {-3.0, 0.2, 0.1},
                                           {0.1, 3.0, 0.2},
                                           {0.1, -3.0, 0.2},
                                           {0.2, 0.1, 3.0},
                                           {0.2, 0.1, -3.0}}};

    for (const Vector3r& offset : offsets) {
        const Ray ray{.origin = target + offset, .direction = -offset};

        const ShapeHit hit{requireValue(triangle.intersect(ray, positive_t))};

        EXPECT_NEAR(hit.t, 1.0, default_tolerance);
        expectNear(hit.point, target);
    }
}

TEST(TriangleTest, sharedEdgeLeavesNoGap)
{
    // Two triangles split a slanted quad along its diagonal. Rays aimed at points
    // on the diagonal must hit at least one of them: a gap there is exactly what
    // the watertight algorithm prevents. The origins make each axis dominant, with
    // both signs along z, to cover every permutation path.
    const Point3r a{-1.3, -0.9, -4.7};
    const Point3r b{1.1, -1.2, -5.9};
    const Point3r c{1.4, 1.0, -5.3};
    const Point3r d{-0.8, 1.3, -4.1};
    const Triangle first{a, b, c};
    const Triangle second{a, c, d};
    const std::array<Point3r, 4> origins{{{0.123, -0.271, 0.317},
                                          {0.217, 0.131, -12.71},
                                          {21.3, 0.311, -5.07},
                                          {0.173, -19.7, -4.93}}};
    constexpr int ray_count{1000};

    int misses{0};
    for (const Point3r& origin : origins) {
        for (int index{0}; index < ray_count; ++index) {
            const Real s{(index + 0.5) / ray_count};
            const Point3r on_edge{a + (c - a) * s};
            const Ray ray{.origin = origin, .direction = on_edge - origin};
            if (!first.intersect(ray, positive_t) && !second.intersect(ray, positive_t)) {
                ++misses;
            }
        }
    }

    EXPECT_EQ(misses, 0);
}

TEST(TriangleTest, rayExactlyOnASharedEdgeHitsBothTriangles)
{
    // The edge function of the shared edge is exactly zero, which counts as inside.
    const Triangle above{Point3r{-1.0, 0.0, -5.0}, Point3r{1.0, 0.0, -5.0},
                         Point3r{0.0, 1.0, -5.0}};
    const Triangle below{Point3r{1.0, 0.0, -5.0}, Point3r{-1.0, 0.0, -5.0},
                         Point3r{0.0, -1.0, -5.0}};
    const Ray ray{.origin = {0.25, 0.0, 0.0}, .direction = {0.0, 0.0, -1.0}};

    EXPECT_TRUE(above.intersect(ray, positive_t).has_value());
    EXPECT_TRUE(below.intersect(ray, positive_t).has_value());
}

TEST(TriangleTest, boundsEncloseTheVertices)
{
    const Triangle triangle{Point3r{0.5, -2.0, 1.0}, Point3r{-1.0, 3.0, 0.0},
                            Point3r{2.0, 0.0, -4.0}};

    const Bounds3r box{triangle.bounds()};

    expectNear(box.min, Point3r{-1.0, -2.0, -4.0});
    expectNear(box.max, Point3r{2.0, 3.0, 1.0});
}

TEST(TriangleTest, defaultTextureCoordinatesAreTheUnitSquaresLowerRightHalf)
{
    const std::array<Point2r, 3> uv{facingTriangle().textureCoordinates()};

    expectNear(uv[0], Point2r{0.0, 0.0}, exact_tolerance);
    expectNear(uv[1], Point2r{1.0, 0.0}, exact_tolerance);
    expectNear(uv[2], Point2r{1.0, 1.0}, exact_tolerance);
}

TEST(TriangleTest, reportsTheTextureCoordinatesGiven)
{
    const Triangle triangle{facingTriangleWithTextureCoordinates()};

    expectNear(triangle.textureCoordinates()[2], Point2r{0.2, 0.9}, exact_tolerance);
}

TEST(TriangleTest, hitInterpolatesTheTextureCoordinates)
{
    // Hits (0, -0.5, -5): halfway along the first edge and a quarter up the second.
    const Ray ray{.origin = {0.0, 0.0, 0.0}, .direction = {0.0, -0.1, -1.0}};

    const ShapeHit hit{
        requireValue(facingTriangleWithTextureCoordinates().intersect(ray, positive_t))};

    expectNear(hit.uv, Point2r{0.5, 0.3});
}

TEST(TriangleTest, hitAtAVertexTakesItsTextureCoordinates)
{
    const Ray ray{.origin = {0.0, 0.0, 0.0}, .direction = {0.2, -0.2, -1.0}};

    const ShapeHit hit{
        requireValue(facingTriangleWithTextureCoordinates().intersect(ray, positive_t))};

    expectNear(hit.uv, Point2r{0.8, 0.1});
}

TEST(TriangleTest, rejectsNonFiniteTextureCoordinates)
{
    constexpr Real nan{std::numeric_limits<Real>::quiet_NaN()};
    constexpr Real infinity{std::numeric_limits<Real>::infinity()};
    const Point3r p0{-1.0, -1.0, -5.0};
    const Point3r p1{1.0, -1.0, -5.0};
    const Point3r p2{-1.0, 1.0, -5.0};

    EXPECT_THROW(Triangle(p0, p1, p2, {{{nan, 0.0}, {1.0, 0.0}, {0.0, 1.0}}}),
                 std::invalid_argument);
    EXPECT_THROW(Triangle(p0, p1, p2, {{{0.0, 0.0}, {1.0, 0.0}, {0.0, -infinity}}}),
                 std::invalid_argument);
}

TEST(TriangleTest, hitShadesWithTheFaceNormal)
{
    const Triangle triangle{facingTriangle()};
    const Ray ray{.origin = {0.0, 0.0, 0.0}, .direction = {0.0, -0.1, -1.0}};

    const ShapeHit hit{requireValue(triangle.intersect(ray, positive_t))};

    // Exactly equal: shading code may rely on flat hits having one normal.
    EXPECT_EQ(hit.shading_normal, triangle.normal());
}

TEST(TriangleTest, barycentricCoordinatesOfAVertexSelectIt)
{
    const Triangle triangle{facingTriangle()};

    const auto [b0, b1, b2]{triangle.barycentricCoordinatesOf({1.0, -1.0, -5.0})};

    EXPECT_NEAR(b0, 0.0, default_tolerance);
    EXPECT_NEAR(b1, 1.0, default_tolerance);
    EXPECT_NEAR(b2, 0.0, default_tolerance);
}

TEST(TriangleTest, barycentricCoordinatesMatchTheTextureInterpolation)
{
    // (0, -0.5, -5) weighs the vertices 0.25, 0.5 and 0.25, as in
    // hitInterpolatesTheTextureCoordinates.
    const auto [b0, b1, b2]{facingTriangle().barycentricCoordinatesOf({0.0, -0.5, -5.0})};

    EXPECT_NEAR(b0, 0.25, default_tolerance);
    EXPECT_NEAR(b1, 0.5, default_tolerance);
    EXPECT_NEAR(b2, 0.25, default_tolerance);
}

TEST(TriangleTest, barycentricCoordinatesProjectPointsOffThePlane)
{
    const auto [b0, b1, b2]{facingTriangle().barycentricCoordinatesOf({0.0, -0.5, 3.0})};

    EXPECT_NEAR(b0, 0.25, default_tolerance);
    EXPECT_NEAR(b1, 0.5, default_tolerance);
    EXPECT_NEAR(b2, 0.25, default_tolerance);
}

TEST(TriangleTest, barycentricCoordinatesOfAPointOutsideIncludeANegativeWeight)
{
    const auto [b0, b1, b2]{facingTriangle().barycentricCoordinatesOf({2.0, 2.0, -5.0})};

    EXPECT_LT(b0, 0.0);
    EXPECT_NEAR(b0 + b1 + b2, 1.0, default_tolerance);
}

} // namespace
} // namespace albedo
