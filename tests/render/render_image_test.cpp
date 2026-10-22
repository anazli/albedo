#include "albedo/render/render_image.hpp"

#include "albedo/estimator/normals_estimator.hpp"
#include "albedo/material/material.hpp"
#include "support/expect_near.hpp"

#include <gtest/gtest.h>

#include <cmath>
#include <stdexcept>
#include <utility>

namespace albedo {
namespace {

using test_support::expectNear;

// Any material will do where only geometry matters.
const Material matte{DiffuseMaterial{.albedo = {0.5, 0.5, 0.5}}};

PerspectiveCamera testCamera()
{
    return PerspectiveCamera{CameraSettings{.look_from = {0.0, 0.0, 0.0},
                                            .look_at = {0.0, 0.0, -1.0},
                                            .up = {0.0, 1.0, 0.0},
                                            .vertical_fov_degrees = 90.0,
                                            .aspect_ratio = 2.0}};
}

// What the estimator returns for the camera ray through `film`.
Rgb radianceAt(const PerspectiveCamera& camera, const Scene& scene, Point2r film)
{
    return NormalsEstimator::radiance(camera.generateRay({.film = film}), scene);
}

TEST(RenderImageTest, oneSamplePerPixelUsesThePixelCenter)
{
    const PerspectiveCamera camera{testCamera()};
    const Scene scene;
    PixelBuffer image{2, 2};

    renderImage({.camera = camera, .scene = scene, .samples_per_axis = 1}, image);

    // Pixel (1, 0) covers film [0.5, 1] x [0, 0.5]; its center is (0.75, 0.25).
    expectNear(image.average({.x = 1, .y = 0}), radianceAt(camera, scene, {0.75, 0.25}));
    expectNear(image.average({.x = 0, .y = 1}), radianceAt(camera, scene, {0.25, 0.75}));
}

TEST(RenderImageTest, samplesFormAGridInsideEachPixel)
{
    const PerspectiveCamera camera{testCamera()};
    const Scene scene;
    PixelBuffer image{1, 1};

    renderImage({.camera = camera, .scene = scene, .samples_per_axis = 2}, image);

    // 2 x 2 grid: sample centers at 1/4 and 3/4 of the pixel on each axis.
    const Rgb expected{
        (radianceAt(camera, scene, {0.25, 0.25}) + radianceAt(camera, scene, {0.75, 0.25}) +
         radianceAt(camera, scene, {0.25, 0.75}) + radianceAt(camera, scene, {0.75, 0.75})) /
        4.0};
    expectNear(image.average({.x = 0, .y = 0}), expected);
}

TEST(RenderImageTest, rendersShapesInTheScene)
{
    // A sphere filling the view: every pixel sees a normal, none sees the sky.
    const PerspectiveCamera camera{testCamera()};
    Scene scene;
    scene.add(Sphere{Point3r{0.0, 0.0, -3.0}, 2.9}, matte);
    PixelBuffer image{1, 1};

    renderImage({.camera = camera, .scene = scene, .samples_per_axis = 1}, image);

    expectNear(image.average({.x = 0, .y = 0}), Rgb{0.5, 0.5, 1.0});
}

TEST(RenderImageTest, mapsPixelsToFilmOnNonSquareImageWithoutMirroring)
{
    // A 4 x 2 image (matching the camera's 2:1 aspect) and a sphere seen only by the
    // top-left pixel: swapping width and height, or mirroring x or y, changes a pixel.
    const PerspectiveCamera camera{testCamera()};
    Scene scene;
    scene.add(Sphere{Point3r{-4.5, 1.5, -3.0}, 1.0}, matte);
    PixelBuffer image{4, 2};

    renderImage({.camera = camera, .scene = scene, .samples_per_axis = 1}, image);

    for (int y{0}; y < 2; ++y) {
        for (int x{0}; x < 4; ++x) {
            const Point2r film_center{(x + 0.5) / 4, (y + 0.5) / 2};
            SCOPED_TRACE(::testing::Message() << "pixel " << x << ", " << y);
            expectNear(image.average({.x = x, .y = y}), radianceAt(camera, scene, film_center));
        }
    }
}

TEST(RenderImageTest, offCenterSphereBreaksBothMirrorSymmetries)
{
    // Guards the mapping test above against passing vacuously: the top-left pixel
    // must look different from its horizontal and its vertical mirror image.
    const PerspectiveCamera camera{testCamera()};
    Scene scene;
    scene.add(Sphere{Point3r{-4.5, 1.5, -3.0}, 1.0}, matte);

    const Rgb top_left{radianceAt(camera, scene, {0.125, 0.25})};
    const Rgb top_right{radianceAt(camera, scene, {0.875, 0.25})};
    const Rgb bottom_left{radianceAt(camera, scene, {0.125, 0.75})};

    EXPECT_GT(std::abs(luminance(top_left) - luminance(top_right)), 0.01);
    EXPECT_GT(std::abs(luminance(top_left) - luminance(bottom_left)), 0.01);
}

template <typename Camera, typename SceneType>
concept CanBuildJob = requires(Camera&& camera, SceneType&& scene) {
    RenderJob{.camera = std::forward<Camera>(camera), .scene = std::forward<SceneType>(scene)};
};

TEST(RenderImageTest, jobRefusesTemporaryCameraOrScene)
{
    static_assert(CanBuildJob<const PerspectiveCamera&, const Scene&>);
    static_assert(!CanBuildJob<PerspectiveCamera, const Scene&>);
    static_assert(!CanBuildJob<const PerspectiveCamera&, Scene>);
}

TEST(RenderImageTest, rejectsNonPositiveSamplesPerAxis)
{
    const PerspectiveCamera camera{testCamera()};
    const Scene scene;
    PixelBuffer image{1, 1};

    EXPECT_THROW(renderImage({.camera = camera, .scene = scene, .samples_per_axis = 0}, image),
                 std::invalid_argument);
    EXPECT_THROW(renderImage({.camera = camera, .scene = scene, .samples_per_axis = -2}, image),
                 std::invalid_argument);
}

} // namespace
} // namespace albedo
