#include "albedo/render/render_image.hpp"

#include "albedo/estimator/normals_estimator.hpp"
#include "albedo/material/material.hpp"
#include "albedo/sampling/sample_generator.hpp"
#include "support/expect_near.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <stdexcept>
#include <utility>
#include <vector>

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

// A sphere filling the view, so the normals estimator's color changes with
// every film position and shows where each sample went.
Scene viewFillingScene()
{
    Scene scene;
    scene.add(Sphere{Point3r{0.0, 0.0, -3.0}, 2.9}, matte);
    return scene;
}

// The average the normals estimator should give `pixel` of `image` when rendering
// `job`: sample (sx, sy) of an n x n grid lands at a uniformly random point of its
// cell, drawn from the pixel's generator in row-major order.
Rgb expectedNormalsAverage(const RenderJob& job, const PixelBuffer& image, Point2i pixel)
{
    const int samples{job.samples_per_axis};
    SampleGenerator generator{SampleGenerator::forPixel(job.seed, pixel, image.width())};
    Rgb sum{};
    for (int sy{0}; sy < samples; ++sy) {
        for (int sx{0}; sx < samples; ++sx) {
            const Point2r offset{generator.next2D()};
            const Point2r film{(pixel.x + (sx + offset.x) / samples) / image.width(),
                               (pixel.y + (sy + offset.y) / samples) / image.height()};
            sum += radianceAt(job.camera.get(), job.scene.get(), film);
        }
    }
    return sum / static_cast<Real>(samples * samples);
}

TEST(RenderImageTest, oneSamplePerPixelIsJitteredInsideThePixel)
{
    const PerspectiveCamera camera{testCamera()};
    const Scene scene{viewFillingScene()};
    PixelBuffer image{2, 2};
    const RenderJob job{
        .camera = camera, .scene = scene, .samples_per_axis = 1, .estimator = NormalsEstimator{}};

    renderImage(job, image);

    expectNear(image.average({.x = 1, .y = 0}),
               expectedNormalsAverage(job, image, {.x = 1, .y = 0}));
    expectNear(image.average({.x = 0, .y = 1}),
               expectedNormalsAverage(job, image, {.x = 0, .y = 1}));
}

TEST(RenderImageTest, jitteredSamplesStayInsideTheirPixel)
{
    // Independent of how the jitter is computed: in an empty scene the normals
    // sky's green channel depends on the normalized direction's y, which across a
    // pixel is extreme on its top and bottom edges, at the center or the ends of
    // the film's x range. A sample inside the pixel lies between those extremes.
    const PerspectiveCamera camera{testCamera()};
    const Scene scene;
    constexpr int height{4};
    PixelBuffer image{1, height};
    const RenderJob job{
        .camera = camera, .scene = scene, .samples_per_axis = 1, .estimator = NormalsEstimator{}};

    renderImage(job, image);

    for (int y{0}; y < height; ++y) {
        SCOPED_TRACE(::testing::Message() << "row " << y);
        std::vector<Real> edge_values;
        for (const Real film_y :
             {static_cast<Real>(y) / height, static_cast<Real>(y + 1) / height}) {
            for (const Real film_x : {0.0, 0.5, 1.0}) {
                edge_values.push_back(radianceAt(camera, scene, {film_x, film_y}).g);
            }
        }
        const auto [lowest, highest]{std::ranges::minmax(edge_values)};
        const Real sample{image.average({.x = 0, .y = y}).g};
        EXPECT_GT(sample, lowest);
        EXPECT_LT(sample, highest);
    }
}

TEST(RenderImageTest, estimatorDrawsFromThePixelGeneratorAfterTheJitter)
{
    // Each sample takes its jitter from the pixel's generator, then the estimator
    // continues from the same generator; later parallel rendering relies on it.
    const PerspectiveCamera camera{testCamera()};
    Scene scene{viewFillingScene()};
    scene.setSky({.nadir = {0.1, 0.2, 0.3}, .zenith = {0.9, 0.8, 0.7}});
    PixelBuffer image{1, 1};
    const RenderJob job{.camera = camera, .scene = scene, .samples_per_axis = 2, .seed = 9};

    renderImage(job, image);

    SampleGenerator generator{SampleGenerator::forPixel(job.seed, {.x = 0, .y = 0}, 1)};
    Rgb sum{};
    for (int sy{0}; sy < 2; ++sy) {
        for (int sx{0}; sx < 2; ++sx) {
            const Point2r jitter{generator.next2D()};
            const Ray ray{camera.generateRay({.film = {(sx + jitter.x) / 2, (sy + jitter.y) / 2}})};
            sum += radiance(job.estimator, ray, scene, generator);
        }
    }
    expectNear(image.average({.x = 0, .y = 0}), sum / 4.0);
}

TEST(RenderImageTest, samplesAreJitteredInsideTheCellsOfAGrid)
{
    const PerspectiveCamera camera{testCamera()};
    const Scene scene{viewFillingScene()};
    PixelBuffer image{1, 1};
    const RenderJob job{
        .camera = camera, .scene = scene, .samples_per_axis = 3, .estimator = NormalsEstimator{}};

    renderImage(job, image);

    expectNear(image.average({.x = 0, .y = 0}),
               expectedNormalsAverage(job, image, {.x = 0, .y = 0}));
}

TEST(RenderImageTest, seedSelectsTheRandomNumbers)
{
    const PerspectiveCamera camera{testCamera()};
    const Scene scene{viewFillingScene()};
    PixelBuffer image{1, 1};
    const RenderJob job{.camera = camera,
                        .scene = scene,
                        .samples_per_axis = 1,
                        .seed = 42,
                        .estimator = NormalsEstimator{}};

    renderImage(job, image);

    expectNear(image.average({.x = 0, .y = 0}),
               expectedNormalsAverage(job, image, {.x = 0, .y = 0}));
}

TEST(RenderImageTest, sameJobRendersTheSameImage)
{
    const PerspectiveCamera camera{testCamera()};
    Scene scene{viewFillingScene()};
    scene.setSky({.nadir = {0.2, 0.2, 0.2}, .zenith = {0.6, 0.8, 1.0}});
    const RenderJob job{.camera = camera, .scene = scene, .samples_per_axis = 2, .seed = 5};
    PixelBuffer first{4, 2};
    PixelBuffer second{4, 2};

    renderImage(job, first);
    renderImage(job, second);

    for (int y{0}; y < 2; ++y) {
        for (int x{0}; x < 4; ++x) {
            expectNear(first.average({.x = x, .y = y}), second.average({.x = x, .y = y}),
                       test_support::exact_tolerance);
        }
    }
}

TEST(RenderImageTest, defaultEstimatorRendersAWhiteFurnaceAsTheSky)
{
    // Only the scatter-only estimator makes a white object vanish under a uniform
    // sky; the normals estimator would show the sphere.
    constexpr Rgb sky_radiance{0.7, 0.8, 0.9};
    const PerspectiveCamera camera{testCamera()};
    Scene scene;
    scene.add(Sphere{Point3r{0.0, 0.0, -3.0}, 2.9}, DiffuseMaterial{.albedo = {1.0, 1.0, 1.0}});
    scene.setSky(SkyLight::uniform(sky_radiance));
    PixelBuffer image{2, 1};

    renderImage({.camera = camera, .scene = scene, .samples_per_axis = 2}, image);

    expectNear(image.average({.x = 0, .y = 0}), sky_radiance, 1e-9);
    expectNear(image.average({.x = 1, .y = 0}), sky_radiance, 1e-9);
}

TEST(RenderImageTest, rendersShapesInTheScene)
{
    // A sphere filling the view. The pixel just right of and below the image
    // center sees the sphere's front within a few hundredths of a unit of the
    // view axis, where the normal is +z to within about 0.01 wherever the
    // jittered sample lands: color (0.5, 0.5, 1).
    const PerspectiveCamera camera{testCamera()};
    Scene scene;
    scene.add(Sphere{Point3r{0.0, 0.0, -3.0}, 2.9}, matte);
    PixelBuffer image{64, 32};
    const RenderJob job{
        .camera = camera, .scene = scene, .samples_per_axis = 1, .estimator = NormalsEstimator{}};

    renderImage(job, image);

    expectNear(image.average({.x = 32, .y = 16}), Rgb{0.5, 0.5, 1.0}, 0.01);
}

TEST(RenderImageTest, mapsPixelsToFilmOnNonSquareImageWithoutMirroring)
{
    // A 4 x 2 image (matching the camera's 2:1 aspect) and a sphere seen only by the
    // top-left pixel: swapping width and height, or mirroring x or y, changes a pixel.
    const PerspectiveCamera camera{testCamera()};
    Scene scene;
    scene.add(Sphere{Point3r{-4.5, 1.5, -3.0}, 1.0}, matte);
    PixelBuffer image{4, 2};
    const RenderJob job{
        .camera = camera, .scene = scene, .samples_per_axis = 1, .estimator = NormalsEstimator{}};

    renderImage(job, image);

    for (int y{0}; y < 2; ++y) {
        for (int x{0}; x < 4; ++x) {
            SCOPED_TRACE(::testing::Message() << "pixel " << x << ", " << y);
            expectNear(image.average({.x = x, .y = y}),
                       expectedNormalsAverage(job, image, {.x = x, .y = y}));
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
