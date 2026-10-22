#include "albedo/estimator/normals_estimator.hpp"

#include "albedo/material/material.hpp"
#include "support/expect_near.hpp"

#include <gtest/gtest.h>

namespace albedo {
namespace {

using test_support::expectNear;

// Any material will do where only geometry matters.
const Material matte{DiffuseMaterial{.albedo = {0.5, 0.5, 0.5}}};

constexpr Ray down_minus_z{.origin = {0.0, 0.0, 0.0}, .direction = {0.0, 0.0, -1.0}};

TEST(NormalsEstimatorTest, hitIsColoredByItsNormal)
{
    // The visible point of a sphere straight ahead has normal +z: (0.5, 0.5, 1).
    Scene scene;
    scene.add(Sphere{Point3r{0.0, 0.0, -5.0}, 1.0}, matte);

    expectNear(NormalsEstimator::radiance(down_minus_z, scene), Rgb{0.5, 0.5, 1.0});
}

TEST(NormalsEstimatorTest, sideHitShowsTheSideNormal)
{
    // A ray coming in along -x hits where the normal is +x: (1, 0.5, 0.5).
    Scene scene;
    scene.add(Sphere{Point3r{0.0, 0.0, 0.0}, 1.0}, matte);
    const Ray from_right{.origin = {5.0, 0.0, 0.0}, .direction = {-1.0, 0.0, 0.0}};

    expectNear(NormalsEstimator::radiance(from_right, scene), Rgb{1.0, 0.5, 0.5});
}

TEST(NormalsEstimatorTest, normalShownIsTheOneFacingTheRay)
{
    // From inside, the far wall's outward normal points away; the shown normal faces back.
    Scene scene;
    scene.add(Sphere{Point3r{0.0, 0.0, 0.0}, 2.0}, matte);

    expectNear(NormalsEstimator::radiance(down_minus_z, scene), Rgb{0.5, 0.5, 1.0});
}

TEST(NormalsEstimatorTest, ignoresShapesBehindTheRayOrigin)
{
    // Camera rays only look forwards: a sphere behind the origin must not be seen.
    Scene scene;
    scene.add(Sphere{Point3r{0.0, 0.0, 5.0}, 1.0}, matte);

    expectNear(NormalsEstimator::radiance(down_minus_z, scene), Rgb{0.75, 0.85, 1.0});
}

TEST(NormalsEstimatorTest, missLookingStraightUpIsSkyBlue)
{
    const Scene scene;
    const Ray up{.origin = {0.0, 0.0, 0.0}, .direction = {0.0, 1.0, 0.0}};

    expectNear(NormalsEstimator::radiance(up, scene), Rgb{0.5, 0.7, 1.0});
}

TEST(NormalsEstimatorTest, missLookingStraightDownIsWhite)
{
    const Scene scene;
    const Ray down{.origin = {0.0, 0.0, 0.0}, .direction = {0.0, -1.0, 0.0}};

    expectNear(NormalsEstimator::radiance(down, scene), Rgb{1.0, 1.0, 1.0});
}

TEST(NormalsEstimatorTest, missAtTheHorizonIsHalfway)
{
    const Scene scene;

    expectNear(NormalsEstimator::radiance(down_minus_z, scene), Rgb{0.75, 0.85, 1.0});
}

TEST(NormalsEstimatorTest, skyDependsOnlyOnDirectionNotItsLength)
{
    const Scene scene;
    const Ray long_up{.origin = {0.0, 0.0, 0.0}, .direction = {0.0, 7.0, 0.0}};

    expectNear(NormalsEstimator::radiance(long_up, scene), Rgb{0.5, 0.7, 1.0});
}

} // namespace
} // namespace albedo
