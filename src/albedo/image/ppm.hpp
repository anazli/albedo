#pragma once

#include <filesystem>
#include <iosfwd>

namespace albedo {

class PixelBuffer;

/// Writes `image` to `out` as a binary PPM (P6): each pixel's average radiance
/// is sRGB-encoded and quantized to 8 bits per channel, rows top to bottom.
/// `out` should be opened in binary mode.
/// Throws `std::runtime_error` if the stream is or ends up in a failed state.
void writePpm(std::ostream& out, const PixelBuffer& image);

/// Writes `image` to the file at `path` as a binary PPM, replacing any existing file.
/// Throws `std::runtime_error` naming the path if the file cannot be opened or written;
/// after a failed write the partial file is removed (an existing file at `path` is
/// lost either way, since it is truncated when opened). If `path` is a symlink, the
/// link is kept and its target may be left partially written.
void writePpm(const std::filesystem::path& path, const PixelBuffer& image);

} // namespace albedo
