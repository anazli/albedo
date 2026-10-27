#include "albedo/image/ppm.hpp"

#include "albedo/color/rgb.hpp"
#include "albedo/color/srgb.hpp"
#include "albedo/image/pixel_buffer.hpp"
#include "albedo/io/path_text.hpp"

#include <cstddef>
#include <format>
#include <fstream>
#include <initializer_list>
#include <ostream>
#include <stdexcept>
#include <string>
#include <system_error>

namespace albedo {

namespace {

// The 3 sRGB bytes of one row, as `char` because that is what streams write.
std::string encodeRow(const PixelBuffer& image, int y)
{
    std::string row;
    row.reserve(static_cast<std::size_t>(image.width()) * 3);
    for (int x{0}; x < image.width(); ++x) {
        const Rgb color{image.average({.x = x, .y = y})};
        for (const Real channel : {color.r, color.g, color.b}) {
            row.push_back(static_cast<char>(toByte(encodeSrgb(channel))));
        }
    }
    return row;
}

// Removes a half-written output so a failed write leaves no corrupt image behind.
// Only a regular file at `path` itself is removed: the target may be a device such
// as /dev/null, and a symlink belongs to the user (its target is left partial).
void discardPartialFile(std::ofstream& file, const std::filesystem::path& path) noexcept
{
    file.close();
    std::error_code ignored;
    if (std::filesystem::is_regular_file(std::filesystem::symlink_status(path, ignored))) {
        std::filesystem::remove(path, ignored);
    }
}

} // namespace

void writePpm(std::ostream& out, const PixelBuffer& image)
{
    out << std::format("P6\n{} {}\n255\n", image.width(), image.height());
    for (int y{0}; y < image.height(); ++y) {
        const std::string row{encodeRow(image, y)};
        out.write(row.data(), static_cast<std::streamsize>(row.size()));
    }
    if (!out) {
        throw std::runtime_error{"failed to write PPM image"};
    }
}

void writePpm(const std::filesystem::path& path, const PixelBuffer& image)
{
    // Binary mode: in text mode Windows would expand every 0x0A byte to "\r\n".
    std::ofstream file{path, std::ios::binary | std::ios::trunc};
    if (!file) {
        throw std::runtime_error{std::format("cannot open '{}' for writing", pathText(path))};
    }
    // The file was truncated on open, so on failure the old content is already
    // gone; remove the partial output rather than leave a corrupt image.
    try {
        writePpm(file, image);
        file.close();
    } catch (const std::runtime_error& error) {
        discardPartialFile(file, path);
        throw std::runtime_error{
            std::format("failed to write '{}': {}", pathText(path), error.what())};
    } catch (...) {
        discardPartialFile(file, path);
        throw;
    }
    if (!file) {
        discardPartialFile(file, path);
        throw std::runtime_error{std::format("failed to write '{}'", pathText(path))};
    }
}

} // namespace albedo
