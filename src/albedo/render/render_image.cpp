#include "albedo/render/render_image.hpp"

#include "albedo/camera/pixel_sample.hpp"
#include "albedo/color/rgb.hpp"
#include "albedo/core/point2.hpp"
#include "albedo/core/real.hpp"
#include "albedo/estimator/radiance_estimator.hpp"
#include "albedo/sampling/sample_generator.hpp"

#include <format>
#include <stdexcept>

namespace albedo {

namespace {

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

void renderImage(const RenderJob& job, PixelBuffer& image)
{
    if (job.samples_per_axis <= 0) {
        throw std::invalid_argument{
            std::format("samples per axis must be positive, got {}", job.samples_per_axis)};
    }
    for (int y{0}; y < image.height(); ++y) {
        for (int x{0}; x < image.width(); ++x) {
            renderPixel(job, {.x = x, .y = y}, image);
        }
    }
}

} // namespace albedo
