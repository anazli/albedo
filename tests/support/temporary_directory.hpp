#pragma once

#include "albedo/io/path_text.hpp"
#include "support/temporary_file.hpp"

#include <filesystem>
#include <fstream>
#include <stdexcept>
#include <string_view>
#include <system_error>

namespace albedo::test_support {

/// A directory in the test output directory, removed with everything in it
/// when the test ends.
class TemporaryDirectory {
public:
    /// Removes anything left at the path by an earlier, aborted run, then
    /// creates the directory.
    explicit TemporaryDirectory(std::string_view name) : directory{testOutputDirectory() / name}
    {
        std::filesystem::remove_all(directory);
        std::filesystem::create_directories(directory);
    }

    TemporaryDirectory(const TemporaryDirectory&) = delete;
    TemporaryDirectory(TemporaryDirectory&&) = delete;
    TemporaryDirectory& operator=(const TemporaryDirectory&) = delete;
    TemporaryDirectory& operator=(TemporaryDirectory&&) = delete;

    ~TemporaryDirectory()
    {
        std::error_code ignored;
        std::filesystem::remove_all(directory, ignored);
    }

    /// Where the directory lives.
    [[nodiscard]] const std::filesystem::path& path() const noexcept { return directory; }

    /// Writes `text` to the file `relative` inside the directory, creating any
    /// directories on the way, and returns its full path.
    [[nodiscard]] std::filesystem::path write(const std::filesystem::path& relative,
                                              std::string_view text) const
    {
        const std::filesystem::path file{directory / relative};
        std::filesystem::create_directories(file.parent_path());
        std::ofstream out{file, std::ios::binary};
        out << text;
        out.close();
        if (!out) {
            throw std::runtime_error{"cannot write test fixture " + pathText(file)};
        }
        return file;
    }

private:
    std::filesystem::path directory;
};

/// Makes `directory` the current directory until the end of the scope. The
/// current directory belongs to the whole process: tests using this must not
/// run concurrently in one process. ctest runs each test in its own process,
/// and a direct run of the test binary runs them one after another.
class CurrentDirectory {
public:
    explicit CurrentDirectory(const std::filesystem::path& directory)
        : previous{std::filesystem::current_path()}
    {
        std::filesystem::current_path(directory);
    }

    CurrentDirectory(const CurrentDirectory&) = delete;
    CurrentDirectory(CurrentDirectory&&) = delete;
    CurrentDirectory& operator=(const CurrentDirectory&) = delete;
    CurrentDirectory& operator=(CurrentDirectory&&) = delete;

    ~CurrentDirectory()
    {
        std::error_code ignored;
        std::filesystem::current_path(previous, ignored);
    }

private:
    std::filesystem::path previous;
};

} // namespace albedo::test_support
