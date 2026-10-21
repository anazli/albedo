#include "albedo/color/rgb.hpp"
#include "albedo/core/real.hpp"
#include "albedo/image/pixel_buffer.hpp"
#include "albedo/image/ppm.hpp"

#include <cstdlib>
#include <exception>
#include <filesystem>
#include <format>
#include <iostream>
#include <span>

namespace {

constexpr int image_width{256};
constexpr int image_height{256};

// Red grows to the right, green downwards: a quick visual check of the image
// orientation and of the sRGB output path.
albedo::PixelBuffer renderGradient()
{
    using albedo::Real;
    albedo::PixelBuffer image{image_width, image_height};
    for (int y{0}; y < image_height; ++y) {
        for (int x{0}; x < image_width; ++x) {
            const albedo::Rgb color{.r = static_cast<Real>(x) / (image_width - 1),
                                    .g = static_cast<Real>(y) / (image_height - 1),
                                    .b = 0.25};
            image.addSample({.x = x, .y = y}, color);
        }
    }
    return image;
}

} // namespace

int main(int argc, char* argv[])
{
    const std::span args{argv, static_cast<std::size_t>(argc)};
    if (args.size() != 2) {
        std::cerr << "usage: albedo <output.ppm>\n";
        return EXIT_FAILURE;
    }

    try {
        // On Windows `argv` is in the ANSI code page, so a path with characters
        // outside it arrives mangled. Supporting that needs `wmain`; not worth it yet.
        const std::filesystem::path output{args[1]};
        albedo::writePpm(output, renderGradient());
        std::cout << std::format("albedo: wrote {}\n", output.string());
    } catch (const std::exception& error) {
        std::cerr << std::format("albedo: {}\n", error.what());
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
