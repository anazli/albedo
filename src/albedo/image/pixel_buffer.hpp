#pragma once

#include "albedo/color/rgb.hpp"
#include "albedo/core/point2.hpp"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace albedo {

/// Accumulates radiance samples per pixel and reports their average.
///
/// Pixel (0, 0) is the top-left corner; x grows to the right and y downwards.
/// Each pixel keeps its own sample count, so pixels may receive different
/// numbers of samples.
class PixelBuffer {
public:
    /// Largest supported image, in pixels (e.g. 16384 x 16384). Larger sizes are
    /// rejected up front instead of attempting an allocation of many GiB.
    static constexpr std::size_t max_pixel_count{std::size_t{1} << 28};

    /// Creates a black image.
    /// Throws `std::invalid_argument` unless `isValidSize(width, height)`.
    PixelBuffer(int width, int height);

    /// True when both dimensions are positive and the image has at most
    /// `max_pixel_count` pixels. Never overflows, whatever the inputs.
    [[nodiscard]] static constexpr bool isValidSize(int width, int height) noexcept
    {
        if (width <= 0 || height <= 0) {
            return false;
        }
        const auto columns{static_cast<std::size_t>(width)};
        const auto rows{static_cast<std::size_t>(height)};
        return columns <= max_pixel_count / rows;
    }

    /// Width in pixels.
    [[nodiscard]] int width() const noexcept { return image_width; }

    /// Height in pixels.
    [[nodiscard]] int height() const noexcept { return image_height; }

    /// Adds one radiance sample to `pixel`.
    /// Precondition: `pixel` lies inside the image.
    void addSample(Point2i pixel, Rgb sample) noexcept;

    /// The mean of the samples added to `pixel`, or black if it has none.
    /// Precondition: `pixel` lies inside the image.
    [[nodiscard]] Rgb average(Point2i pixel) const noexcept;

private:
    struct Accumulator {
        Rgb sum{};
        std::int64_t sample_count{};
    };

    [[nodiscard]] bool contains(Point2i pixel) const noexcept;
    [[nodiscard]] std::size_t indexOf(Point2i pixel) const noexcept;

    int image_width;
    int image_height;
    std::vector<Accumulator> pixels;
};

} // namespace albedo
