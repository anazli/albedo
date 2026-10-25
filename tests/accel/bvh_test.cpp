#include "albedo/accel/bvh.hpp"

#include "albedo/geometry/sphere.hpp"
#include "albedo/geometry/triangle.hpp"
#include "albedo/sampling/pcg32.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <cstddef>
#include <limits>
#include <optional>
#include <stdexcept>
#include <vector>

namespace albedo {
namespace {

constexpr Interval positive_t{.min = 0.0, .max = std::numeric_limits<Real>::infinity()};

// Looking down -z from the origin.
constexpr Ray forward_ray{.origin = {0.0, 0.0, 0.0}, .direction = {0.0, 0.0, -1.0}};

// Stands in for a primitive that is hit wherever its box is: isolates the culling.
std::optional<Real> alwaysHitAtFive(std::size_t /*primitive*/, Interval range)
{
    return range.surrounds(5.0) ? std::optional<Real>{5.0} : std::nullopt;
}

std::vector<Bounds3r> boundsOf(const std::vector<Sphere>& spheres)
{
    std::vector<Bounds3r> boxes;
    boxes.reserve(spheres.size());
    for (const Sphere& sphere : spheres) {
        boxes.push_back(sphere.bounds());
    }
    return boxes;
}

Real uniform(Pcg32& random, Real low, Real high)
{
    return low + (high - low) * random.nextReal();
}

Point3r randomPoint(Pcg32& random, Real half_extent)
{
    return {uniform(random, -half_extent, half_extent), uniform(random, -half_extent, half_extent),
            uniform(random, -half_extent, half_extent)};
}

std::vector<Sphere> randomSpheres(std::size_t count, Pcg32& random)
{
    std::vector<Sphere> spheres;
    spheres.reserve(count);
    for (std::size_t index{0}; index < count; ++index) {
        spheres.emplace_back(randomPoint(random, 10.0), uniform(random, 0.05, 0.5));
    }
    return spheres;
}

// Intersects the spheres one by one, as a reference.
struct SphereIntersector {
    const std::vector<Sphere>* spheres{};
    Ray ray{};

    std::optional<Real> operator()(std::size_t primitive, Interval range) const
    {
        const auto hit{(*spheres)[primitive].intersect(ray, range)};
        return hit ? std::optional<Real>{hit->t} : std::nullopt;
    }
};

std::optional<std::size_t> bruteForceNearest(const std::vector<Sphere>& spheres, const Ray& ray,
                                             Interval range)
{
    std::optional<std::size_t> nearest;
    for (std::size_t index{0}; index < spheres.size(); ++index) {
        if (const auto hit{spheres[index].intersect(ray, range)}) {
            nearest = index;
            range.max = hit->t;
        }
    }
    return nearest;
}

TEST(BvhTest, emptyHierarchyFindsNothingAndCallsNothing)
{
    const Bvh bvh;
    int calls{0};
    const auto counting{[&](std::size_t primitive, Interval range) {
        ++calls;
        return alwaysHitAtFive(primitive, range);
    }};

    EXPECT_FALSE(bvh.nearestHit(forward_ray, positive_t, counting).has_value());
    EXPECT_FALSE(bvh.anyHit(forward_ray, positive_t, counting));
    EXPECT_EQ(calls, 0);
}

TEST(BvhTest, findsASinglePrimitive)
{
    const std::vector<Bounds3r> boxes{{.min = {-1.0, -1.0, -6.0}, .max = {1.0, 1.0, -4.0}}};
    const Bvh bvh{boxes};

    EXPECT_EQ(bvh.nearestHit(forward_ray, positive_t, alwaysHitAtFive),
              std::optional<std::size_t>{0});
    EXPECT_TRUE(bvh.anyHit(forward_ray, positive_t, alwaysHitAtFive));
}

TEST(BvhTest, reportsTheOriginalIndexOfTheNearestPrimitive)
{
    const std::vector<Sphere> spheres{
        Sphere{Point3r{0.0, 0.0, -20.0}, 1.0}, Sphere{Point3r{0.0, 0.0, -9.0}, 1.0},
        Sphere{Point3r{0.0, 0.0, -5.0}, 1.0}, Sphere{Point3r{3.0, 0.0, -2.0}, 1.0}};
    const Bvh bvh{boundsOf(spheres)};

    const auto nearest{bvh.nearestHit(forward_ray, positive_t,
                                      SphereIntersector{.spheres = &spheres, .ray = forward_ray})};

    EXPECT_EQ(nearest, std::optional<std::size_t>{2});
}

TEST(BvhTest, nearestHitMatchesABruteForceSearch)
{
    Pcg32 random{{.seed = 42, .stream = 7}};
    const std::vector<Sphere> spheres{randomSpheres(500, random)};
    const Bvh bvh{boundsOf(spheres)};
    constexpr int ray_count{500};

    int mismatches{0};
    int hits{0};
    for (int index{0}; index < ray_count; ++index) {
        // Aimed into the cloud of spheres, so a good share of rays hit something.
        const Point3r origin{randomPoint(random, 15.0)};
        const Ray ray{.origin = origin, .direction = randomPoint(random, 10.0) - origin};
        const auto expected{bruteForceNearest(spheres, ray, positive_t)};
        const auto actual{
            bvh.nearestHit(ray, positive_t, SphereIntersector{.spheres = &spheres, .ray = ray})};
        mismatches += expected != actual ? 1 : 0;
        hits += expected ? 1 : 0;
    }

    EXPECT_EQ(mismatches, 0);
    EXPECT_GT(hits, ray_count / 10); // The comparison means little if almost every ray misses.
}

TEST(BvhTest, anyHitMatchesABruteForceSearchWithinARange)
{
    Pcg32 random{{.seed = 9, .stream = 3}};
    const std::vector<Sphere> spheres{randomSpheres(300, random)};
    const Bvh bvh{boundsOf(spheres)};
    constexpr Interval short_range{.min = 0.0, .max = 4.0};
    constexpr int ray_count{500};

    int mismatches{0};
    for (int index{0}; index < ray_count; ++index) {
        const Ray ray{.origin = randomPoint(random, 12.0),
                      .direction = randomPoint(random, 1.0) - Point3r{}};
        const bool expected{bruteForceNearest(spheres, ray, short_range).has_value()};
        const bool actual{
            bvh.anyHit(ray, short_range, SphereIntersector{.spheres = &spheres, .ray = ray})};
        mismatches += expected != actual ? 1 : 0;
    }

    EXPECT_EQ(mismatches, 0);
}

TEST(BvhTest, hitsBeyondTheRangeAreNotReported)
{
    const std::vector<Bounds3r> boxes{{.min = {-1.0, -1.0, -6.0}, .max = {1.0, 1.0, -4.0}}};
    const Bvh bvh{boxes};
    constexpr Interval too_short{.min = 0.0, .max = 3.0};

    EXPECT_FALSE(bvh.nearestHit(forward_ray, too_short, alwaysHitAtFive).has_value());
    EXPECT_FALSE(bvh.anyHit(forward_ray, too_short, alwaysHitAtFive));
}

TEST(BvhTest, rangeShrinksToTheNearestHitSoFar)
{
    Pcg32 random{{.seed = 5, .stream = 1}};
    const std::vector<Sphere> spheres{randomSpheres(200, random)};
    const Bvh bvh{boundsOf(spheres)};
    const Ray ray{.origin = {-15.0, 0.1, 0.2}, .direction = {1.0, 0.01, -0.02}};
    Real nearest_t{positive_t.max};
    int widened{0};

    const auto checking{[&](std::size_t primitive, Interval range) -> std::optional<Real> {
        widened += range.max > nearest_t ? 1 : 0;
        const auto hit{spheres[primitive].intersect(ray, range)};
        if (!hit) {
            return std::nullopt;
        }
        nearest_t = hit->t;
        return hit->t;
    }};
    const auto nearest{bvh.nearestHit(ray, positive_t, checking)};

    EXPECT_EQ(nearest, bruteForceNearest(spheres, ray, positive_t));
    EXPECT_EQ(widened, 0);
}

TEST(BvhTest, skipsPrimitivesTheRayCannotReach)
{
    // A 10 x 10 x 10 grid of small spheres. A ray beside the grid tests none of
    // them, and one along a grid line tests a handful, not a thousand.
    std::vector<Sphere> spheres;
    for (int x{0}; x < 10; ++x) {
        for (int y{0}; y < 10; ++y) {
            for (int z{0}; z < 10; ++z) {
                spheres.emplace_back(Point3r{1.0 * x, 1.0 * y, -1.0 * z - 5.0}, 0.25);
            }
        }
    }
    const Bvh bvh{boundsOf(spheres)};
    const Ray beside{.origin = {20.0, 20.0, 0.0}, .direction = {0.0, 0.0, -1.0}};
    const Ray along_line{.origin = {3.0, 4.0, 0.0}, .direction = {0.0, 0.0, -1.0}};
    int beside_calls{0};
    int along_calls{0};

    const auto beside_hit{bvh.nearestHit(beside, positive_t, [&](std::size_t i, Interval range) {
        ++beside_calls;
        return SphereIntersector{.spheres = &spheres, .ray = beside}(i, range);
    })};
    const auto along_hit{bvh.nearestHit(along_line, positive_t, [&](std::size_t i, Interval range) {
        ++along_calls;
        return SphereIntersector{.spheres = &spheres, .ray = along_line}(i, range);
    })};

    EXPECT_FALSE(beside_hit.has_value());
    EXPECT_EQ(beside_calls, 0);
    EXPECT_EQ(along_hit, std::optional<std::size_t>{(3 * 10 + 4) * 10});
    EXPECT_LT(along_calls, 20);
}

TEST(BvhTest, buildsAndReachesEveryPrimitiveWhenAllBoxesCoincide)
{
    // No split separates identical boxes by position, yet every primitive must
    // still end up in some leaf the ray reaches.
    const std::vector<Bounds3r> boxes(1000, {.min = {-1.0, -1.0, -6.0}, .max = {1.0, 1.0, -4.0}});
    const Bvh bvh{boxes};
    constexpr std::size_t wanted{777};

    const auto nearest{
        bvh.nearestHit(forward_ray, positive_t, [](std::size_t primitive, Interval range) {
            return primitive == wanted ? alwaysHitAtFive(primitive, range) : std::nullopt;
        })};

    EXPECT_EQ(nearest, std::optional<std::size_t>{wanted});
}

TEST(BvhTest, handlesExponentiallySpacedPrimitives)
{
    // Spacing that tempts a surface area split to peel off one primitive at a
    // time, which would make the tree as deep as the primitive count.
    constexpr int count{300};
    std::vector<Sphere> spheres;
    for (int index{0}; index < count; ++index) {
        spheres.emplace_back(Point3r{std::ldexp(1.0, index), 0.0, 0.0}, std::ldexp(1.0, index - 2));
    }
    const Bvh bvh{boundsOf(spheres)};
    const Ray along_x{.origin = {-1.0, 0.0, 0.0}, .direction = {1.0, 0.0, 0.0}};
    constexpr std::size_t last{count - 1};

    const auto nearest{bvh.nearestHit(along_x, positive_t,
                                      SphereIntersector{.spheres = &spheres, .ray = along_x})};
    const auto farthest{
        bvh.nearestHit(along_x, positive_t, [&](std::size_t primitive, Interval range) {
            return primitive == last
                       ? SphereIntersector{.spheres = &spheres, .ray = along_x}(primitive, range)
                       : std::nullopt;
        })};

    EXPECT_EQ(nearest, std::optional<std::size_t>{0});
    EXPECT_EQ(farthest, std::optional<std::size_t>{last});
}

TEST(BvhTest, findsTrianglesWithFlatBoxes)
{
    // Axis-aligned triangles have boxes of zero thickness.
    const std::vector<Triangle> triangles{
        Triangle{Point3r{-1.0, -1.0, -5.0}, Point3r{1.0, -1.0, -5.0}, Point3r{-1.0, 1.0, -5.0}},
        Triangle{Point3r{-1.0, -1.0, -8.0}, Point3r{1.0, -1.0, -8.0}, Point3r{-1.0, 1.0, -8.0}}};
    std::vector<Bounds3r> boxes;
    boxes.reserve(triangles.size());
    for (const Triangle& triangle : triangles) {
        boxes.push_back(triangle.bounds());
    }
    const Bvh bvh{boxes};
    const Ray ray{.origin = {-0.5, -0.5, 0.0}, .direction = {0.0, 0.0, -1.0}};

    const auto nearest{bvh.nearestHit(ray, positive_t, [&](std::size_t i, Interval range) {
        const auto hit{triangles[i].intersect(ray, range)};
        return hit ? std::optional<Real>{hit->t} : std::nullopt;
    })};

    EXPECT_EQ(nearest, std::optional<std::size_t>{0});
}

TEST(BvhTest, rayInThePlaneOfABoxFaceIsNotCulled)
{
    // The ray's y direction is zero and its origin lies exactly on the box's
    // y faces, which turns the slab distances into 0 * infinity = NaN.
    const std::vector<Bounds3r> boxes{{.min = {0.0, 0.0, -6.0}, .max = {1.0, 1.0, -4.0}}};
    const Bvh bvh{boxes};
    const Ray on_min_face{.origin = {0.5, 0.0, 0.0}, .direction = {0.0, 0.0, -1.0}};
    const Ray on_max_face{.origin = {0.5, 1.0, 0.0}, .direction = {0.0, 0.0, -1.0}};
    const Ray just_outside{.origin = {0.5, 1.0 + 1e-12, 0.0}, .direction = {0.0, 0.0, -1.0}};

    EXPECT_TRUE(bvh.anyHit(on_min_face, positive_t, alwaysHitAtFive));
    EXPECT_TRUE(bvh.anyHit(on_max_face, positive_t, alwaysHitAtFive));
    EXPECT_FALSE(bvh.anyHit(just_outside, positive_t, alwaysHitAtFive));
}

TEST(BvhTest, rayInThePlaneOfAFlatBoxIsNotCulled)
{
    // An axis-aligned triangle's box has zero thickness in y; a ray lying in that
    // plane has both y slab distances 0 * infinity = NaN.
    const std::vector<Bounds3r> boxes{{.min = {0.0, 0.0, -6.0}, .max = {1.0, 0.0, -5.0}}};
    const Bvh bvh{boxes};
    const Ray in_plane{.origin = {0.25, 0.0, 0.0}, .direction = {0.0, 0.0, -1.0}};

    EXPECT_TRUE(bvh.anyHit(in_plane, positive_t, alwaysHitAtFive));
}

TEST(BvhTest, handlesCoordinatesNearTheLargestDouble)
{
    // Every box is finite, but the spread of their centres overflows to infinity.
    constexpr Real largest{std::numeric_limits<Real>::max()};
    constexpr Real half_width{1e300};
    std::vector<Bounds3r> boxes;
    for (int step{1}; step <= 16; ++step) {
        for (const Real sign : {1.0, -1.0}) {
            const Real center{sign * largest * (step / 17.0)};
            boxes.push_back(
                {.min = {center - half_width, -1.0, -1.0}, .max = {center + half_width, 1.0, 1.0}});
        }
    }
    const Bvh bvh{boxes};
    // Each box is hit where the ray enters it.
    const auto entering{[&](const Ray& ray) {
        return [&boxes, ray](std::size_t primitive, Interval range) -> std::optional<Real> {
            const Bounds3r& box{boxes[primitive]};
            const Real t{ray.direction.x > 0 ? box.min.x : -box.max.x};
            return range.surrounds(t) ? std::optional<Real>{t} : std::nullopt;
        };
    }};
    const Ray towards_positive{.origin = {}, .direction = {1.0, 0.0, 0.0}};
    const Ray towards_negative{.origin = {}, .direction = {-1.0, 0.0, 0.0}};

    EXPECT_EQ(bvh.nearestHit(towards_positive, positive_t, entering(towards_positive)),
              std::optional<std::size_t>{0});
    EXPECT_EQ(bvh.nearestHit(towards_negative, positive_t, entering(towards_negative)),
              std::optional<std::size_t>{1});
}

TEST(BvhTest, rejectsEmptyAndNonFiniteBoxes)
{
    constexpr Real infinity{std::numeric_limits<Real>::infinity()};
    constexpr Real nan{std::numeric_limits<Real>::quiet_NaN()};
    const Bounds3r valid{.min = {0.0, 0.0, 0.0}, .max = {1.0, 1.0, 1.0}};
    const std::vector<Bounds3r> with_empty{valid, Bounds3r{}};
    const std::vector<Bounds3r> with_infinite{
        valid, {.min = {0.0, 0.0, 0.0}, .max = {infinity, 1.0, 1.0}}};
    const std::vector<Bounds3r> with_nan{{.min = {nan, 0.0, 0.0}, .max = {1.0, 1.0, 1.0}}, valid};

    EXPECT_THROW(Bvh{with_empty}, std::invalid_argument);
    EXPECT_THROW(Bvh{with_infinite}, std::invalid_argument);
    EXPECT_THROW(Bvh{with_nan}, std::invalid_argument);
}

TEST(BvhTest, rayStartingInsideABoxFindsItsPrimitive)
{
    const std::vector<Bounds3r> boxes{{.min = {-1.0, -1.0, -6.0}, .max = {1.0, 1.0, 1.0}}};
    const Bvh bvh{boxes};

    EXPECT_TRUE(bvh.anyHit(forward_ray, positive_t, alwaysHitAtFive));
}

} // namespace
} // namespace albedo
