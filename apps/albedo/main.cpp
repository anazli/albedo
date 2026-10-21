#include "albedo/camera/perspective_camera.hpp"
#include "albedo/core/point3.hpp"
#include "albedo/core/real.hpp"
#include "albedo/geometry/sphere.hpp"
#include "albedo/image/pixel_buffer.hpp"
#include "albedo/image/ppm.hpp"
#include "albedo/render/render_image.hpp"
#include "albedo/scene/scene.hpp"

#include <algorithm>
#include <charconv>
#include <cmath>
#include <cstdlib>
#include <exception>
#include <filesystem>
#include <format>
#include <iostream>
#include <optional>
#include <span>
#include <string_view>
#include <system_error>

namespace {

constexpr int default_image_width{960};
constexpr int min_image_width{16};
// Deliberate cap on what the command line can request: 8192 x 4608 pixels need
// about 1.2 GB of accumulators (32 bytes per pixel), within PixelBuffer's limit.
constexpr int max_image_width{8192};
constexpr int samples_per_axis{4};
constexpr albedo::Real aspect_ratio{16.0 / 9.0};

constexpr std::string_view usage{"usage: albedo <output.ppm> [width]\n"
                                 "  width: image width in pixels, 16 to 8192 (default 960)\n"};

// Parses the optional width argument. The whole string must be a base-10 integer
// in range; anything else (signs, spaces, trailing text, overflow) is rejected.
std::optional<int> parseImageWidth(std::string_view text)
{
    int width{};
    const auto [end, error]{std::from_chars(text.data(), text.data() + text.size(), width)};
    if (error != std::errc{} || end != text.data() + text.size() || width < min_image_width ||
        width > max_image_width) {
        return std::nullopt;
    }
    return width;
}

// A large sphere as the ground, a main sphere and smaller ones at varied
// depths, so the normal colors show both curvature and perspective.
albedo::Scene buildScene()
{
    using albedo::Point3r;
    using albedo::Sphere;
    albedo::Scene scene;
    scene.add(Sphere{Point3r{0.0, -1000.0, 0.0}, 1000.0});
    scene.add(Sphere{Point3r{0.0, 1.0, 0.0}, 1.0});
    scene.add(Sphere{Point3r{-2.3, 0.7, -0.6}, 0.7});
    scene.add(Sphere{Point3r{2.2, 0.55, 0.4}, 0.55});
    scene.add(Sphere{Point3r{1.1, 0.3, 1.9}, 0.3});
    scene.add(Sphere{Point3r{-1.0, 0.25, 2.3}, 0.25});
    scene.add(Sphere{Point3r{4.2, 1.2, -4.0}, 1.2});
    scene.add(Sphere{Point3r{-4.6, 1.5, -5.5}, 1.5});
    return scene;
}

albedo::PerspectiveCamera buildCamera(int width, int height)
{
    return albedo::PerspectiveCamera{
        albedo::CameraSettings{.look_from = {0.0, 1.4, 8.0},
                               .look_at = {0.0, 0.9, 0.0},
                               .up = {0.0, 1.0, 0.0},
                               .vertical_fov_degrees = 35.0,
                               .aspect_ratio = static_cast<albedo::Real>(width) / height}};
}

// Renders the scene into a `width` x (`width` * 9 / 16) image and writes it to `output`.
void renderTo(const std::filesystem::path& output, int width)
{
    const int height{std::max(1, static_cast<int>(std::lround(width / aspect_ratio)))};
    const albedo::Scene scene{buildScene()};
    const albedo::PerspectiveCamera camera{buildCamera(width, height)};
    albedo::PixelBuffer image{width, height};
    albedo::renderImage({.camera = camera, .scene = scene, .samples_per_axis = samples_per_axis},
                        image);
    albedo::writePpm(output, image);
}

} // namespace

int main(int argc, char* argv[])
{
    const std::span args{argv, static_cast<std::size_t>(argc)};
    if (args.size() != 2 && args.size() != 3) {
        std::cerr << usage;
        return EXIT_FAILURE;
    }
    const std::optional<int> width{args.size() == 3 ? parseImageWidth(args[2])
                                                    : default_image_width};
    if (!width) {
        std::cerr << std::format("albedo: invalid width '{}'\n{}", args[2], usage);
        return EXIT_FAILURE;
    }

    try {
        // On Windows `argv` is in the ANSI code page, so a path with characters
        // outside it arrives mangled. Supporting that needs `wmain`; not worth it yet.
        const std::filesystem::path output{args[1]};
        renderTo(output, *width);
        std::cout << std::format("albedo: wrote {}\n", output.string());
    } catch (const std::exception& error) {
        std::cerr << std::format("albedo: {}\n", error.what());
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
