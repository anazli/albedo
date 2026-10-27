#include "albedo/io/path_text.hpp"

#include <system_error>

namespace albedo {

std::string pathText(const std::filesystem::path& path)
{
    try {
        const std::u8string utf8{path.u8string()};
        std::string text;
        text.reserve(utf8.size());
        for (const char8_t unit : utf8) {
            text.push_back(static_cast<char>(unit));
        }
        return text;
    } catch (const std::system_error&) {
        return "<path not representable as UTF-8>";
    }
}

} // namespace albedo
