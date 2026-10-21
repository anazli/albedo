#include "albedo/render/render_image.hpp"

#include "albedo/camera/pixel_sample.hpp"
#include "albedo/color/rgb.hpp"
#include "albedo/core/point2.hpp"
#include "albedo/core/real.hpp"
#include "albedo/estimator/normals_estimator.hpp"

#include <format>
#include <stdexcept>

namespace albedo {

namespace {

// Adds every grid sample of `pixel` to `image`.
void renderPixel(const RenderJob& job, Point2i pixel, PixelBuffer& image)
{
    const int samples{job.samples_per_axis};
    const Real width{static_cast<Real>(image.width())};
    const Real height{static_cast<Real>(image.height())};
    for (int sy{0}; sy < samples; ++sy) {
        for (int sx{0}; sx < samples; ++sx) {
            // Sample centers sit at (i + 0.5) / n inside the pixel on each axis.
            const Real offset_x{(sx + 0.5) / samples};
            const Real offset_y{(sy + 0.5) / samples};
            const PixelSample sample{
                .film = {(pixel.x + offset_x) / width, (pixel.y + offset_y) / height}};
            const Ray ray{job.camera.get().generateRay(sample)};
            image.addSample(pixel, NormalsEstimator::radiance(ray, job.scene.get()));
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
