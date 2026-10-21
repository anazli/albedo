#include "albedo/image/pixel_buffer.hpp"

#include <cassert>
#include <format>
#include <stdexcept>

namespace albedo {

namespace {

// Validates before `pixels` is sized, so an invalid size never reaches the allocator.
std::size_t validatedPixelCount(int width, int height)
{
    if (!PixelBuffer::isValidSize(width, height)) {
        throw std::invalid_argument{std::format(
            "invalid image size {}x{}: dimensions must be positive and the image at most {} pixels",
            width, height, PixelBuffer::max_pixel_count)};
    }
    return static_cast<std::size_t>(width) * static_cast<std::size_t>(height);
}

} // namespace

PixelBuffer::PixelBuffer(int width, int height)
    : image_width{width}, image_height{height}, pixels(validatedPixelCount(width, height))
{
}

void PixelBuffer::addSample(Point2i pixel, Rgb sample) noexcept
{
    assert(contains(pixel) && "pixel outside the image");
    Accumulator& accumulator{pixels[indexOf(pixel)]};
    accumulator.sum += sample;
    ++accumulator.sample_count;
}

Rgb PixelBuffer::average(Point2i pixel) const noexcept
{
    assert(contains(pixel) && "pixel outside the image");
    const Accumulator& accumulator{pixels[indexOf(pixel)]};
    if (accumulator.sample_count == 0) {
        return {};
    }
    return accumulator.sum / static_cast<Real>(accumulator.sample_count);
}

bool PixelBuffer::contains(Point2i pixel) const noexcept
{
    return pixel.x >= 0 && pixel.x < image_width && pixel.y >= 0 && pixel.y < image_height;
}

std::size_t PixelBuffer::indexOf(Point2i pixel) const noexcept
{
    return static_cast<std::size_t>(pixel.y) * static_cast<std::size_t>(image_width) +
           static_cast<std::size_t>(pixel.x);
}

} // namespace albedo
