#include "albedo/estimator/surface_ray.hpp"

#include "support/expect_near.hpp"

#include <gtest/gtest.h>

namespace albedo {
namespace {

using test_support::expectNear;

const HitRecord hit{
    .t = 1.0, .point = {1.0, 2.0, 3.0}, .normal = {0.0, 1.0, 0.0}, .front_face = true};

TEST(SurfaceRayTest, rayLeavingInFrontStartsInFrontOfTheSurface)
{
    const Ray ray{rayLeaving(hit, {0.0, 0.6, 0.8})};

    expectNear(ray.origin, Point3r{1.0, 2.0 + surface_offset, 3.0});
    expectNear(ray.direction, Vector3r{0.0, 0.6, 0.8});
}

TEST(SurfaceRayTest, rayLeavingBehindStartsBehindTheSurface)
{
    const Ray ray{rayLeaving(hit, {0.0, -0.6, 0.8})};

    expectNear(ray.origin, Point3r{1.0, 2.0 - surface_offset, 3.0});
}

} // namespace
} // namespace albedo
