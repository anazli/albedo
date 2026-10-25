#include "albedo/image/ppm.hpp"

#include "albedo/image/pixel_buffer.hpp"
#include "support/temporary_file.hpp"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>

namespace albedo {
namespace {

using test_support::TemporaryFile;
using test_support::testOutputDirectory;

// A 2x1 image: a fully red pixel next to a mid-gray one.
PixelBuffer twoPixelImage()
{
    PixelBuffer image{2, 1};
    image.addSample({.x = 0, .y = 0}, Rgb{1.0, 0.0, 0.0});
    image.addSample({.x = 1, .y = 0}, Rgb{0.5, 0.5, 0.5});
    return image;
}

// Header, then R G B bytes per pixel, rows top to bottom; 0.5 linear is sRGB 188.
const std::string two_pixel_ppm{"P6\n2 1\n255\n"
                                "\xFF\x00\x00"
                                "\xBC\xBC\xBC",
                                11 + 6};

std::string readBinary(const std::filesystem::path& path)
{
    std::ifstream file{path, std::ios::binary};
    return {std::istreambuf_iterator<char>{file}, std::istreambuf_iterator<char>{}};
}

TEST(PpmTest, writesHeaderAndPixelBytesToStream)
{
    std::ostringstream out{std::ios::binary};

    writePpm(out, twoPixelImage());

    EXPECT_EQ(out.str(), two_pixel_ppm);
}

TEST(PpmTest, writesRowsTopToBottom)
{
    PixelBuffer image{1, 2};
    image.addSample({.x = 0, .y = 0}, Rgb{1.0, 1.0, 1.0});
    std::ostringstream out{std::ios::binary};

    writePpm(out, image);

    EXPECT_EQ(out.str(), std::string("P6\n1 2\n255\n\xFF\xFF\xFF\x00\x00\x00", 11 + 6));
}

TEST(PpmTest, clampsValuesBrighterThanWhite)
{
    PixelBuffer image{1, 1};
    image.addSample({.x = 0, .y = 0}, Rgb{4.0, 1.0, -2.0});
    std::ostringstream out{std::ios::binary};

    writePpm(out, image);

    EXPECT_EQ(out.str(), std::string("P6\n1 1\n255\n\xFF\xFF\x00", 11 + 3));
}

TEST(PpmTest, throwsWhenStreamIsInFailedState)
{
    std::ostringstream out;
    out.setstate(std::ios::badbit);

    EXPECT_THROW(writePpm(out, twoPixelImage()), std::runtime_error);
}

TEST(PpmTest, writesFileWithIdenticalBytes)
{
    const TemporaryFile file{"albedo_ppm_test_writes_file.ppm"};

    writePpm(file.path(), twoPixelImage());

    EXPECT_EQ(readBinary(file.path()), two_pixel_ppm);
}

TEST(PpmTest, overwritesLongerExistingFile)
{
    const TemporaryFile file{"albedo_ppm_test_overwrites.ppm"};
    {
        std::ofstream existing{file.path(), std::ios::binary};
        existing << std::string(64, 'x');
    }

    writePpm(file.path(), twoPixelImage());

    EXPECT_EQ(readBinary(file.path()), two_pixel_ppm);
}

TEST(PpmTest, throwsWhenFileCannotBeOpened)
{
    const std::filesystem::path missing_directory{testOutputDirectory() /
                                                  "albedo_ppm_test_missing_dir_throws"};
    std::filesystem::remove_all(missing_directory);

    EXPECT_THROW(writePpm(missing_directory / "image.ppm", twoPixelImage()), std::runtime_error);
}

TEST(PpmTest, openFailureMessageNamesThePath)
{
    const std::filesystem::path missing_directory{testOutputDirectory() /
                                                  "albedo_ppm_test_missing_dir_message"};
    std::filesystem::remove_all(missing_directory);

    try {
        writePpm(missing_directory / "image.ppm", twoPixelImage());
        FAIL() << "expected std::runtime_error";
    } catch (const std::runtime_error& error) {
        EXPECT_NE(std::string_view{error.what()}.find("albedo_ppm_test_missing_dir_message"),
                  std::string_view::npos);
    }
}

// /dev/full accepts the open but fails every write with "no space left".
const std::filesystem::path full_device{"/dev/full"};

// What writePpm(std::ostream&, ...) reports; the path overload appends it as the cause.
constexpr std::string_view stream_failure_message{"failed to write PPM image"};

TEST(PpmTest, failureWhenClosingThrowsAndNamesThePath)
{
    if (!std::filesystem::exists(full_device)) {
        GTEST_SKIP() << "needs /dev/full (Linux)";
    }

    // 17 bytes stay in the stream buffer, so the failure surfaces on close.
    try {
        writePpm(full_device, twoPixelImage());
        FAIL() << "expected std::runtime_error";
    } catch (const std::runtime_error& error) {
        const std::string_view message{error.what()};
        EXPECT_NE(message.find("/dev/full"), std::string_view::npos);
        // Only the write-time branch appends the stream writer's message.
        EXPECT_EQ(message.find(stream_failure_message), std::string_view::npos);
    }
}

TEST(PpmTest, failureWhileWritingThrowsAndNamesThePath)
{
    if (!std::filesystem::exists(full_device)) {
        GTEST_SKIP() << "needs /dev/full (Linux)";
    }
    // ~196 KB overflows the stream buffer, so the failure surfaces while writing rows.
    const PixelBuffer large_image{256, 256};

    try {
        writePpm(full_device, large_image);
        FAIL() << "expected std::runtime_error";
    } catch (const std::runtime_error& error) {
        const std::string_view message{error.what()};
        EXPECT_NE(message.find("/dev/full"), std::string_view::npos);
        EXPECT_NE(message.find(stream_failure_message), std::string_view::npos);
    }
}

} // namespace
} // namespace albedo
