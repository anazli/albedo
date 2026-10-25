#pragma once

#include <filesystem>
#include <string_view>
#include <system_error>

namespace albedo::test_support {

/// Directory for files the tests write. Tests run with the build tree as working
/// directory, so separate build trees never share it, unlike the system temp
/// directory. Parallel test processes of one tree do share it, so every test must
/// use its own file name.
inline std::filesystem::path testOutputDirectory()
{
    const std::filesystem::path directory{std::filesystem::current_path() / "test_output"};
    std::filesystem::create_directories(directory);
    return directory;
}

/// A file path in the test output directory, removed when the test ends.
class TemporaryFile {
public:
    /// Removes any file left at the path by an earlier, aborted run.
    explicit TemporaryFile(std::string_view name) : file_path{testOutputDirectory() / name}
    {
        std::filesystem::remove(file_path);
    }

    TemporaryFile(const TemporaryFile&) = delete;
    TemporaryFile(TemporaryFile&&) = delete;
    TemporaryFile& operator=(const TemporaryFile&) = delete;
    TemporaryFile& operator=(TemporaryFile&&) = delete;

    ~TemporaryFile()
    {
        std::error_code ignored;
        std::filesystem::remove(file_path, ignored);
    }

    /// Where the file lives.
    [[nodiscard]] const std::filesystem::path& path() const noexcept { return file_path; }

private:
    std::filesystem::path file_path;
};

} // namespace albedo::test_support
