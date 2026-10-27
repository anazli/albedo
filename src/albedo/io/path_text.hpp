#pragma once

#include <filesystem>
#include <string>

namespace albedo {

/// `path` as UTF-8 text for messages. Unlike `path.string()`, which on Windows
/// throws for paths the narrow code page cannot represent, this gives a
/// placeholder for any path the platform cannot convert. Throws only
/// `std::bad_alloc`.
[[nodiscard]] std::string pathText(const std::filesystem::path& path);

} // namespace albedo
