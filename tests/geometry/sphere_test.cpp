#include "albedo/geometry/sphere.hpp"

#include "support/expect_near.hpp"
#include "support/require_value.hpp"

#include <gtest/gtest.h>

#include <limits>
#include <stdexcept>

namespace albedo {
namespace {

using test_support::default_tolerance;
using test_support::expectNear;
using test_support::requireValue;

constexpr Interval positive_t{.min = 0.0, .max = std::numeric_limits<Real>::infinity()};

// Unit sphere at (0, 0, -5), seen from the origin along -z. A function rather than
// a namespace-scope constant so a throwing constructor fails a test, not startup.
Sphere unitSphere()
{
    return Sphere{Point3r{0.0, 0.0, -5.0}, 1.0};
}

TEST(SphereTest, reportsCenterAndRadius)
{
    const Sphere sphere{unitSphere()};

    expectNear(sphere.center(), Point3r{0.0, 0.0, -5.0});
    EXPECT_NEAR(sphere.radius(), 1.0, default_tolerance);
}

TEST(SphereTest, rejectsNonPositiveRadius)
{
    EXPECT_THROW(Sphere(Point3r{}, 0.0), std::invalid_argument);
    EXPECT_THROW(Sphere(Point3r{}, -1.0), std::invalid_argument);
}

TEST(SphereTest, rejectsNonFiniteRadius)
{
    EXPECT_THROW(Sphere(Point3r{}, std::numeric_limits<Real>::infinity()), std::invalid_argument);
    EXPECT_THROW(Sphere(Point3r{}, std::numeric_limits<Real>::quiet_NaN()), std::invalid_argument);
}

TEST(SphereTest, rejectsNonFiniteCenter)
{
    constexpr Real infinity{std::numeric_limits<Real>::infinity()};
    constexpr Real nan{std::numeric_limits<Real>::quiet_NaN()};

    EXPECT_THROW(Sphere(Point3r{infinity, 0.0, 0.0}, 1.0), std::invalid_argument);
    EXPECT_THROW(Sphere(Point3r{0.0, nan, 0.0}, 1.0), std::invalid_argument);
}

TEST(SphereTest, rejectsRadiusWhoseSquareOverflowsOrUnderflows)
{
    // The intersection math squares the radius; outside this range every ray would miss.
    EXPECT_THROW(Sphere(Point3r{}, 1e200), std::invalid_argument);
    EXPECT_THROW(Sphere(Point3r{}, 1e-200), std::invalid_argument);
}

TEST(SphereTest, rayThroughCenterHitsNearSide)
{
    const Ray ray{.origin = {0.0, 0.0, 0.0}, .direction = {0.0, 0.0, -1.0}};

    const ShapeHit hit{requireValue(unitSphere().intersect(ray, positive_t))};

    EXPECT_NEAR(hit.t, 4.0, default_tolerance);
    expectNear(hit.point, Point3r{0.0, 0.0, -4.0});
}

TEST(SphereTest, normalPointsOutwardWithUnitLength)
{
    const Ray ray{.origin = {0.0, 0.0, 0.0}, .direction = {0.0, 0.0, -1.0}};

    const ShapeHit hit{requireValue(unitSphere().intersect(ray, positive_t))};

    expectNear(hit.normal, Normal3r{0.0, 0.0, 1.0});
}

TEST(SphereTest, rayThatPassesBesideMisses)
{
    const Ray ray{.origin = {0.0, 1.5, 0.0}, .direction = {0.0, 0.0, -1.0}};

    EXPECT_FALSE(unitSphere().intersect(ray, positive_t).has_value());
}

TEST(SphereTest, rayJustOutsideTangentMisses)
{
    const Ray ray{.origin = {0.0, 1.0 + 1e-9, 0.0}, .direction = {0.0, 0.0, -1.0}};

    EXPECT_FALSE(unitSphere().intersect(ray, positive_t).has_value());
}

TEST(SphereTest, rayPointingAwayMisses)
{
    const Ray ray{.origin = {0.0, 0.0, 0.0}, .direction = {0.0, 0.0, 1.0}};

    EXPECT_FALSE(unitSphere().intersect(ray, positive_t).has_value());
}

TEST(SphereTest, zeroLengthDirectionMisses)
{
    const Ray ray{.origin = {0.0, 0.0, 0.0}, .direction = {0.0, 0.0, 0.0}};

    EXPECT_FALSE(unitSphere().intersect(ray, positive_t).has_value());
}

TEST(SphereTest, emptyIntervalMisses)
{
    const Ray ray{.origin = {0.0, 0.0, 0.0}, .direction = {0.0, 0.0, -1.0}};

    EXPECT_FALSE(unitSphere().intersect(ray, empty_interval).has_value());
}

TEST(SphereTest, tangentRayTouchesAtOnePoint)
{
    const Ray ray{.origin = {0.0, 1.0, 0.0}, .direction = {0.0, 0.0, -1.0}};

    const ShapeHit hit{requireValue(unitSphere().intersect(ray, positive_t))};

    EXPECT_NEAR(hit.t, 5.0, 1e-6);
    expectNear(hit.normal, Normal3r{0.0, 1.0, 0.0}, 1e-6);
}

TEST(SphereTest, rayFromInsideHitsFarSideWithOutwardNormal)
{
    const Ray ray{.origin = {0.0, 0.0, -5.0}, .direction = {1.0, 0.0, 0.0}};

    const ShapeHit hit{requireValue(unitSphere().intersect(ray, positive_t))};

    EXPECT_NEAR(hit.t, 1.0, default_tolerance);
    expectNear(hit.normal, Normal3r{1.0, 0.0, 0.0});
}

TEST(SphereTest, nearHitOutsideIntervalFallsBackToFarHit)
{
    const Ray ray{.origin = {0.0, 0.0, 0.0}, .direction = {0.0, 0.0, -1.0}};

    const ShapeHit hit{
        requireValue(unitSphere().intersect(ray, Interval{.min = 4.5, .max = 100.0}))};

    EXPECT_NEAR(hit.t, 6.0, default_tolerance);
}

TEST(SphereTest, hitsBeyondIntervalAreIgnored)
{
    const Ray ray{.origin = {0.0, 0.0, 0.0}, .direction = {0.0, 0.0, -1.0}};

    EXPECT_FALSE(unitSphere().intersect(ray, Interval{.min = 0.0, .max = 3.9}).has_value());
}

TEST(SphereTest, hitExactlyAtIntervalMaxIsRejected)
{
    // Open interval: t == max is outside, so the ray reaches the far side only if max allows.
    const Ray ray{.origin = {0.0, 0.0, 0.0}, .direction = {0.0, 0.0, -1.0}};

    EXPECT_FALSE(unitSphere().intersect(ray, Interval{.min = 0.0, .max = 4.0}).has_value());
}

TEST(SphereTest, originOnSurfaceSkipsHitAtZero)
{
    // A ray leaving a surface at t = 0 must not re-hit it there.
    const Ray ray{.origin = {0.0, 0.0, -4.0}, .direction = {0.0, 0.0, -1.0}};

    const ShapeHit hit{requireValue(unitSphere().intersect(ray, positive_t))};

    EXPECT_NEAR(hit.t, 2.0, default_tolerance);
}

TEST(SphereTest, originOnSurfaceHeadingOutwardMisses)
{
    const Ray ray{.origin = {0.0, 0.0, -4.0}, .direction = {0.0, 0.0, 1.0}};

    EXPECT_FALSE(unitSphere().intersect(ray, positive_t).has_value());
}

TEST(SphereTest, unnormalizedDirectionScalesParameter)
{
    const Ray ray{.origin = {0.0, 0.0, 0.0}, .direction = {0.0, 0.0, -2.0}};

    const ShapeHit hit{requireValue(unitSphere().intersect(ray, positive_t))};

    EXPECT_NEAR(hit.t, 2.0, default_tolerance);
    expectNear(hit.point, Point3r{0.0, 0.0, -4.0});
}

TEST(SphereTest, distantSmallSphereIsHitAccurately)
{
    // With the textbook discriminant h^2 - a c, c = |oc|^2 - r^2 rounds to 1e12 and the
    // ray appears to graze the sphere at t = 1e6; the rewritten discriminant keeps it.
    const Sphere far_sphere{Point3r{0.0, 0.0, -1e6}, 1e-3};
    const Ray ray{.origin = {0.0, 0.0, 0.0}, .direction = {0.0, 0.0, -1.0}};

    const ShapeHit hit{requireValue(far_sphere.intersect(ray, positive_t))};

    EXPECT_NEAR(hit.t, 1e6 - 1e-3, 1e-6);
}

TEST(SphereTest, distantSmallSphereNormalHasUnitLength)
{
    // The hit point's absolute error is large relative to a tiny radius, so dividing
    // by the radius alone would give a normal noticeably off unit length.
    const Sphere far_sphere{Point3r{0.0, 3e6, -1e7}, 1e-4};
    const Ray ray{.origin = {0.0, 0.0, 0.0}, .direction = {0.0, 3e6 + 5e-5, -1e7}};

    const ShapeHit hit{requireValue(far_sphere.intersect(ray, positive_t))};

    EXPECT_NEAR(length(asVector(hit.normal)), 1.0, default_tolerance);
}

TEST(SphereTest, nearRootStaysAccurateWhenRootsDifferHugely)
{
    // A ray starting ~2e-9 outside the unit sphere: the near root is ~2e-9, the far
    // one ~2. The origin has dyadic coordinates (integers / 2^14), so c = |o|^2 - 1 is
    // computed exactly and any error comes from the root formula alone. There,
    // (-h - sqrt(D)) / a cancels catastrophically (relative error ~1.5e-7) while the
    // c / q form is accurate to ~1e-16. Reference value from 60-digit arithmetic.
    const Sphere sphere{Point3r{0.0, 0.0, 0.0}, 1.0};
    const Ray ray{.origin = {0.2957763671875, 0.15106201171875, 0.9432373046875},
                  .direction = {-0.3411177909433509, -0.17474016395585198, -0.9432373046875}};
    constexpr Real exact_near_root{1.8315314704767794e-09};

    const ShapeHit hit{requireValue(sphere.intersect(ray, positive_t))};

    EXPECT_NEAR(hit.t, exact_near_root, exact_near_root * 1e-12);
}

TEST(SphereTest, sphereBelowPositionalResolutionMisses)
{
    // At x = 1 doubles are 2.2e-16 apart, so a sphere of radius 1e-17 there has no
    // representable surface point distinct from its center; no normal can be formed.
    const Sphere tiny_sphere{Point3r{1.0, 0.0, 0.0}, 1e-17};
    const Ray ray{.origin = {0.0, 0.0, 0.0}, .direction = {1.0, 0.0, 0.0}};

    EXPECT_FALSE(tiny_sphere.intersect(ray, positive_t).has_value());
}

TEST(SphereTest, subnormalDirectionMisses)
{
    // |d|^2 is subnormal: too few significant bits for a meaningful intersection.
    const Ray ray{.origin = {0.0, 0.0, 0.0}, .direction = {0.0, 0.0, -1e-160}};

    EXPECT_FALSE(unitSphere().intersect(ray, positive_t).has_value());
}

TEST(SphereTest, boundsEncloseTheSphere)
{
    const Bounds3r box{unitSphere().bounds()};

    expectNear(box.min, Point3r{-1.0, -1.0, -6.0});
    expectNear(box.max, Point3r{1.0, 1.0, -4.0});
}

} // namespace
} // namespace albedo
