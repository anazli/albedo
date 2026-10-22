#include "albedo/render/render_image.hpp"

#include "albedo/camera/pixel_sample.hpp"
#include "albedo/color/rgb.hpp"
#include "albedo/core/point2.hpp"
#include "albedo/core/real.hpp"
#include "albedo/estimator/radiance_estimator.hpp"
#include "albedo/render/tile.hpp"
#include "albedo/sampling/sample_generator.hpp"

#include <algorithm>
#include <format>
#include <limits>
#include <stdexcept>
#include <thread>
#include <vector>

namespace albedo {

namespace {

// Small enough that the threads stay busy until near the end of the image,
// large enough that handing out tiles costs nothing next to rendering them.
constexpr int tile_size{16};

// Adds one jittered sample per grid cell of `pixel` to `image`.
void renderPixel(const RenderJob& job, Point2i pixel, PixelBuffer& image)
{
    const int samples{job.samples_per_axis};
    const Real width{static_cast<Real>(image.width())};
    const Real height{static_cast<Real>(image.height())};
    SampleGenerator generator{SampleGenerator::forPixel(job.seed, pixel, image.width())};
    for (int sy{0}; sy < samples; ++sy) {
        for (int sx{0}; sx < samples; ++sx) {
            const Point2r jitter{generator.next2D()};
            const Real offset_x{(sx + jitter.x) / samples};
            const Real offset_y{(sy + jitter.y) / samples};
            const PixelSample sample{
                .film = {(pixel.x + offset_x) / width, (pixel.y + offset_y) / height}};
            const Ray ray{job.camera.get().generateRay(sample)};
            image.addSample(pixel, radiance(job.estimator, ray, job.scene.get(), generator));
        }
    }
}

} // namespace

int availableThreadCount() noexcept
{
    const unsigned int hardware_threads{std::thread::hardware_concurrency()};
    if (hardware_threads == 0) {
        return 1;
    }
    return static_cast<int>(
        std::min(hardware_threads, static_cast<unsigned int>(std::numeric_limits<int>::max())));
}

void renderImage(const RenderJob& job, PixelBuffer& image)
{
    if (job.samples_per_axis <= 0) {
        throw std::invalid_argument{
            std::format("samples per axis must be positive, got {}", job.samples_per_axis)};
    }
    if (job.thread_count <= 0) {
        throw std::invalid_argument{
            std::format("thread count must be positive, got {}", job.thread_count)};
    }
    const std::vector<Tile> tiles{splitIntoTiles(image.width(), image.height(), tile_size)};
    // Tiles never overlap, so each thread writes only its own pixels of `image`.
    forEachTileInParallel(tiles, job.thread_count, [&](const Tile& tile) {
        for (int y{tile.min.y}; y < tile.max.y; ++y) {
            for (int x{tile.min.x}; x < tile.max.x; ++x) {
                renderPixel(job, {.x = x, .y = y}, image);
            }
        }
    });
}

} // namespace albedo
