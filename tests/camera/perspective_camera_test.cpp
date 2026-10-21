#include "albedo/camera/perspective_camera.hpp"

#include "support/expect_near.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <stdexcept>

namespace albedo {
namespace {

using test_support::default_tolerance;
using test_support::expectNear;

// Standing at (0, 1, 3), looking at (0, 1, 0): forward is -z, right is +x, up is +y.
constexpr CameraSettings settings{.look_from = {0.0, 1.0, 3.0},
                                  .look_at = {0.0, 1.0, 0.0},
                                  .up = {0.0, 1.0, 0.0},
                                  .vertical_fov_degrees = 90.0,
                                  .aspect_ratio = 2.0};

void expectRejected(const CameraSettings& bad)
{
    EXPECT_THROW(PerspectiveCamera{bad}, std::invalid_argument);
}

Real angleBetween(Vector3r a, Vector3r b)
{
    return std::acos(dot(normalized(a), normalized(b)));
}

TEST(PerspectiveCameraTest, raysStartAtTheCameraPosition)
{
    const PerspectiveCamera camera{settings};

    const Ray ray{camera.generateRay({.film = {0.2, 0.7}})};

    expectNear(ray.origin, settings.look_from);
}

TEST(PerspectiveCameraTest, raysHaveUnitDirection)
{
    const PerspectiveCamera camera{settings};

    const Ray ray{camera.generateRay({.film = {0.1, 0.9}})};

    EXPECT_NEAR(length(ray.direction), 1.0, default_tolerance);
}

TEST(PerspectiveCameraTest, filmCenterLooksAtTheTarget)
{
    const PerspectiveCamera camera{settings};

    const Ray ray{camera.generateRay({.film = {0.5, 0.5}})};

    expectNear(ray.direction, Vector3r{0.0, 0.0, -1.0});
}

TEST(PerspectiveCameraTest, topEdgeIsHalfTheVerticalFieldOfViewUp)
{
    const PerspectiveCamera camera{settings};

    const Ray ray{camera.generateRay({.film = {0.5, 0.0}})};

    // 90 degree vertical field of view: the top edge is 45 degrees above forward.
    expectNear(ray.direction, normalized(Vector3r{0.0, 1.0, -1.0}));
}

TEST(PerspectiveCameraTest, rightEdgeAngleFollowsTheAspectRatio)
{
    const PerspectiveCamera camera{settings};

    const Ray ray{camera.generateRay({.film = {1.0, 0.5}})};

    // Half width = aspect * tan(45 degrees) = 2, so the right edge is atan(2) to the right.
    EXPECT_NEAR(angleBetween(ray.direction, Vector3r{0.0, 0.0, -1.0}), std::atan(2.0),
                default_tolerance);
    EXPECT_GT(ray.direction.x, 0.0);
}

TEST(PerspectiveCameraTest, filmOriginIsTheTopLeftCorner)
{
    const PerspectiveCamera camera{settings};

    const Ray ray{camera.generateRay({.film = {0.0, 0.0}})};

    EXPECT_LT(ray.direction.x, 0.0);
    EXPECT_GT(ray.direction.y, 0.0);
}

TEST(PerspectiveCameraTest, rejectsFieldOfViewOutsideOpenRange)
{
    for (const Real fov : {0.0, -10.0, 180.0, 200.0, std::numeric_limits<Real>::quiet_NaN()}) {
        CameraSettings bad{settings};
        bad.vertical_fov_degrees = fov;

        SCOPED_TRACE(fov);
        expectRejected(bad);
    }
}

TEST(PerspectiveCameraTest, rejectsNonPositiveOrNonFiniteAspectRatio)
{
    for (const Real aspect : {0.0, -1.0, std::numeric_limits<Real>::infinity(),
                              std::numeric_limits<Real>::quiet_NaN()}) {
        CameraSettings bad{settings};
        bad.aspect_ratio = aspect;

        SCOPED_TRACE(aspect);
        expectRejected(bad);
    }
}

TEST(PerspectiveCameraTest, rejectsTargetAtTheCameraPosition)
{
    CameraSettings bad{settings};
    bad.look_at = bad.look_from;

    expectRejected(bad);
}

TEST(PerspectiveCameraTest, rejectsUpParallelToTheViewDirection)
{
    CameraSettings bad{settings};
    bad.up = {0.0, 0.0, 1.0};

    expectRejected(bad);
}

TEST(PerspectiveCameraTest, rejectsUpNearlyParallelToTheViewDirection)
{
    CameraSettings bad{settings};
    bad.up = {1e-8, 0.0, 1.0};

    expectRejected(bad);
}

TEST(PerspectiveCameraTest, rejectsZeroUp)
{
    CameraSettings bad{settings};
    bad.up = {0.0, 0.0, 0.0};

    expectRejected(bad);
}

TEST(PerspectiveCameraTest, rejectsNonFiniteUp)
{
    for (const Real component :
         {std::numeric_limits<Real>::infinity(), std::numeric_limits<Real>::quiet_NaN()}) {
        CameraSettings bad{settings};
        bad.up = {0.0, component, 0.0};

        SCOPED_TRACE(component);
        expectRejected(bad);
    }
}

TEST(PerspectiveCameraTest, rejectsNonFinitePosition)
{
    for (const Real component :
         {std::numeric_limits<Real>::infinity(), std::numeric_limits<Real>::quiet_NaN()}) {
        CameraSettings bad_from{settings};
        bad_from.look_from.x = component;
        CameraSettings bad_at{settings};
        bad_at.look_at.y = component;

        SCOPED_TRACE(component);
        expectRejected(bad_from);
        expectRejected(bad_at);
    }
}

TEST(PerspectiveCameraTest, rejectsAspectRatioThatOverflowsTheFilm)
{
    // Passes a "positive and finite" check, but a ray to the film corner would
    // have a direction whose squared length overflows, so it could not be normalized.
    CameraSettings bad{settings};
    bad.aspect_ratio = 1e160;

    expectRejected(bad);
}

TEST(PerspectiveCameraTest, rejectsSubnormalAspectRatio)
{
    CameraSettings bad{settings};
    bad.aspect_ratio = 1e-310;

    expectRejected(bad);
}

TEST(PerspectiveCameraTest, scaleOfUpDoesNotChangeTheRays)
{
    // A tiny up vector at a shallow angle to the view: its cross product with the
    // view direction would be subnormal unless up is normalized first.
    CameraSettings unit_up{settings};
    unit_up.up = {0.0, 1e-3, 1.0};
    CameraSettings tiny_up{settings};
    tiny_up.up = {0.0, 2e-157, 2e-154};
    const PerspectiveCamera reference{unit_up};
    const PerspectiveCamera camera{tiny_up};

    const Ray expected{reference.generateRay({.film = {0.0, 0.0}})};
    const Ray ray{camera.generateRay({.film = {0.0, 0.0}})};

    expectNear(ray.direction, expected.direction);
}

TEST(PerspectiveCameraTest, upNeedNotBePerpendicularOrUnitLength)
{
    CameraSettings tilted{settings};
    tilted.up = {0.0, 5.0, 2.0};
    const PerspectiveCamera camera{tilted};

    const Ray ray{camera.generateRay({.film = {0.5, 0.0}})};

    expectNear(ray.direction, normalized(Vector3r{0.0, 1.0, -1.0}));
}

TEST(PerspectiveCameraDeathTest, generateRayAssertsOnNonFiniteFilm)
{
    const PerspectiveCamera camera{settings};
    constexpr Real nan{std::numeric_limits<Real>::quiet_NaN()};

    EXPECT_DEBUG_DEATH(static_cast<void>(camera.generateRay({.film = {nan, 0.5}})), "");
}

TEST(PerspectiveCameraDeathTest, generateRayAssertsOnFilmOutsideUnitSquare)
{
    // Only rays through [0, 1]^2 are guaranteed to be normalizable.
    const PerspectiveCamera camera{settings};

    EXPECT_DEBUG_DEATH(static_cast<void>(camera.generateRay({.film = {1e200, 0.5}})), "");
    EXPECT_DEBUG_DEATH(static_cast<void>(camera.generateRay({.film = {0.5, -0.1}})), "");
}

} // namespace
} // namespace albedo
