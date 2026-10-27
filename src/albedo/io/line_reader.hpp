#pragma once

#include <cstddef>
#include <iosfwd>
#include <optional>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace albedo {

/// What `LineReader::next` throws for a line longer than the reader's limit.
class LineTooLongError : public std::runtime_error {
public:
    /// `line` is the 1-based number of the offending line, `limit` the longest
    /// line allowed, in bytes, and `format` names the input for the message.
    LineTooLongError(std::size_t line, std::size_t limit, std::string_view format);

    /// The 1-based number of the offending line.
    [[nodiscard]] std::size_t line() const noexcept { return line_number; }

    /// The longest line allowed, in bytes.
    [[nodiscard]] std::size_t limit() const noexcept { return max_length; }

private:
    std::size_t line_number;
    std::size_t max_length;
};

/// Reads text one line at a time without ever buffering more than a fixed
/// number of bytes, so a hostile or corrupt file cannot make a parser exhaust
/// memory with one endless line. Lines end at `\n`, a trailing `\r` is dropped,
/// and the last line may lack the `\n`. Only streams with an empty `exceptions()`
/// mask are fully supported; see `next`.
class LineReader {
public:
    /// Reads from `input`, which must outlive the reader, allowing lines of up to
    /// `max_line_length` bytes. `format` names the input in error messages, for
    /// example "OBJ". Throws `std::runtime_error` if `input` is already failed and
    /// `std::invalid_argument` if a buffer one line long cannot even be sized (near
    /// the largest `std::streamsize`); merely large values can still fail to
    /// allocate, with `std::bad_alloc`.
    LineReader(std::istream& input, std::size_t max_line_length, std::string_view format);

    // A copy would share the stream but count lines on its own.
    LineReader(const LineReader&) = delete;
    LineReader& operator=(const LineReader&) = delete;
    LineReader(LineReader&&) noexcept = default;
    LineReader& operator=(LineReader&&) noexcept = default;
    ~LineReader() = default;

    /// The next line without its terminator, or nothing at the end of input. The
    /// view is valid until the next call. Throws `LineTooLongError` for a line over
    /// the limit and `std::runtime_error` if reading fails; after either, every
    /// further call returns nothing. With a non-empty `exceptions()` mask the
    /// stream's own exceptions escape instead: `std::ios_base::failure` wherever
    /// reading sets a masked bit (failbit is set at the end of input and for an
    /// overlong line), or whatever the stream buffer throws if badbit is masked;
    /// with eofbit masked, a last line without `\n` is lost.
    [[nodiscard]] std::optional<std::string_view> next();

    /// The 1-based number of the line `next` last returned; zero before the first.
    [[nodiscard]] std::size_t lineNumber() const noexcept { return line_number; }

private:
    // Non-owning; a pointer rather than a reference so the reader stays movable.
    std::istream* stream;
    std::size_t max_length;
    std::string format_name;
    // One byte more than the limit for a '\r' before the '\n', and one for the
    // terminating null that getline writes.
    std::vector<char> buffer;
    std::size_t line_number{0};
    bool finished{false};
};

} // namespace albedo
