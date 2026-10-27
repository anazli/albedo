#include "albedo/io/path_text.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <string>

namespace albedo {
namespace {

TEST(PathTextTest, givesAsciiPathsAsWritten)
{
    EXPECT_EQ(pathText(std::filesystem::path{"scenes/room.scene"}), "scenes/room.scene");
}

TEST(PathTextTest, givesOtherCharactersAsUtf8)
{
    // "café" with the e-acute as two UTF-8 bytes, C3 A9.
    const std::filesystem::path path{u8"café.obj"};

    EXPECT_EQ(pathText(path), std::string{"caf\xC3\xA9.obj"});
}

TEST(PathTextTest, emptyPathGivesEmptyText)
{
    EXPECT_EQ(pathText(std::filesystem::path{}), std::string{});
}

} // namespace
} // namespace albedo
