#include "albedo/camera/perspective_camera.hpp"
#include "albedo/core/real.hpp"
#include "albedo/estimator/path_estimator.hpp"
#include "albedo/image/pixel_buffer.hpp"
#include "albedo/image/ppm.hpp"
#include "albedo/io/path_text.hpp"
#include "albedo/render/render_image.hpp"
#include "albedo/scene/scene_file.hpp"

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
#include <stdexcept>
#include <string_view>
#include <system_error>

namespace {

constexpr int default_image_width{960};
constexpr int min_image_width{16};
// Deliberate cap on what the command line can request: 8192 x 4608 pixels need
// about 1.2 GB of accumulators (32 bytes per pixel), within PixelBuffer's limit.
constexpr int max_image_width{8192};
constexpr int samples_per_axis{16};
constexpr albedo::Real aspect_ratio{16.0 / 9.0};

constexpr std::string_view usage{"usage: albedo <scene> <output.ppm> [width]\n"
                                 "  scene: a scene file, such as assets/showcase.scene\n"
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

// The scene's camera at the image's aspect ratio. The scene file checked it at
// the default ratio only, so it can still fail here; the error names the file.
albedo::PerspectiveCamera cameraFor(albedo::CameraSettings settings, int width, int height,
                                    const std::filesystem::path& scene_path)
{
    settings.aspect_ratio = static_cast<albedo::Real>(width) / height;
    try {
        return albedo::PerspectiveCamera{settings};
    } catch (const std::invalid_argument& error) {
        throw std::runtime_error{std::format("{}: the camera cannot render this image: {}",
                                             albedo::pathText(scene_path), error.what())};
    }
}

// The scene in the file at `path`; a parse error is reported with the path.
albedo::SceneDescription sceneAt(const std::filesystem::path& path)
{
    try {
        return albedo::readScene(path);
    } catch (const albedo::SceneParseError& error) {
        throw std::runtime_error{std::format("{}: {}", albedo::pathText(path), error.what())};
    }
}

// What to render and where to write it. A struct, so the two paths are named
// at the call site and cannot be swapped there.
struct RenderRequest {
    std::filesystem::path scene;
    std::filesystem::path output;
    int width{default_image_width};
};

// Renders the requested scene into a `width` x (`width` * 9 / 16) image and
// writes it to the output path.
void render(const RenderRequest& request)
{
    // From the command line the paths can still name one file; writing the image
    // would then destroy the scene.
    std::error_code error;
    if (std::filesystem::equivalent(request.scene, request.output, error)) {
        throw std::runtime_error{std::format("{}: the output would overwrite the scene file",
                                             albedo::pathText(request.output))};
    }
    const int height{std::max(1, static_cast<int>(std::lround(request.width / aspect_ratio)))};
    const albedo::SceneDescription description{sceneAt(request.scene)};
    const albedo::PerspectiveCamera camera{
        cameraFor(description.camera, request.width, height, request.scene)};
    albedo::PixelBuffer image{request.width, height};
    albedo::renderImage({.camera = camera,
                         .scene = description.scene,
                         .samples_per_axis = samples_per_axis,
                         .estimator = albedo::PathEstimator{},
                         .thread_count = albedo::availableThreadCount()},
                        image);
    albedo::writePpm(request.output, image);
}

} // namespace

int main(int argc, char* argv[])
{
    const std::span args{argv, static_cast<std::size_t>(argc)};
    if (args.size() < 3 || args.size() > 4) {
        std::cerr << usage;
        return EXIT_FAILURE;
    }
    const std::optional<int> width{args.size() == 4 ? parseImageWidth(args[3])
                                                    : default_image_width};
    if (!width) {
        std::cerr << std::format("albedo: invalid width '{}'\n{}", args[3], usage);
        return EXIT_FAILURE;
    }

    try {
        // On Windows `argv` is in the ANSI code page, so a path with characters
        // outside it arrives mangled. Supporting that needs `wmain`; not worth it yet.
        const RenderRequest request{.scene = args[1], .output = args[2], .width = *width};
        render(request);
        std::cout << std::format("albedo: wrote {}\n", albedo::pathText(request.output));
    } catch (const std::exception& error) {
        std::cerr << std::format("albedo: {}\n", error.what());
        return EXIT_FAILURE;
    }
    return EXIT_SUCCESS;
}
