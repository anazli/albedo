#include "albedo/io/line_reader.hpp"

#include <format>
#include <istream>
#include <limits>

namespace albedo {

namespace {

// The longest line limit whose buffer (two bytes more) a std::streamsize can size.
constexpr auto largest_line_limit{
    static_cast<std::size_t>(std::numeric_limits<std::streamsize>::max()) - 2};

// `max_line_length`, if `input` can be read and the buffer sized.
std::size_t checkedLimit(const std::istream& input, std::size_t max_line_length,
                         std::string_view format)
{
    if (input.fail()) {
        throw std::runtime_error{std::format("{} input stream is not readable", format)};
    }
    if (max_line_length > largest_line_limit) {
        throw std::invalid_argument{std::format("{} line length limit is too large", format)};
    }
    return max_line_length;
}

} // namespace

LineTooLongError::LineTooLongError(std::size_t line, std::size_t limit, std::string_view format)
    : std::runtime_error{std::format("{} line {} is longer than {} bytes", format, line, limit)},
      line_number{line}, max_length{limit}
{
}

LineReader::LineReader(std::istream& input, std::size_t max_line_length, std::string_view format)
    : stream{&input}, max_length{checkedLimit(input, max_line_length, format)}, format_name{format},
      buffer(max_length + 2)
{
}

std::optional<std::string_view> LineReader::next()
{
    if (finished) {
        return std::nullopt;
    }
    stream->getline(buffer.data(), static_cast<std::streamsize>(buffer.size()));
    if (stream->bad()) {
        finished = true;
        throw std::runtime_error{std::format("failed to read {} input", format_name)};
    }
    const std::streamsize extracted{stream->gcount()};
    if (stream->fail()) {
        if (stream->eof() && extracted == 0) {
            finished = true;
            return std::nullopt;
        }
        // getline stops with failbit set, but not eofbit, when the buffer fills.
        finished = true;
        throw LineTooLongError{line_number + 1, max_length, format_name};
    }
    // The count includes the '\n' unless the input ended without one.
    auto length{static_cast<std::size_t>(stream->eof() ? extracted : extracted - 1)};
    if (length > 0 && buffer[length - 1] == '\r') {
        --length;
    }
    if (length > max_length) {
        finished = true;
        throw LineTooLongError{line_number + 1, max_length, format_name};
    }
    ++line_number;
    finished = stream->eof();
    return std::string_view{buffer.data(), length};
}

} // namespace albedo
