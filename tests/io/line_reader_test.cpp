#include "albedo/io/line_reader.hpp"

#include "support/failing_streambuf.hpp"

#include <gtest/gtest.h>

#include <cstddef>
#include <ios>
#include <istream>
#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace albedo {
namespace {

using test_support::FailingStreambuf;

// Every line `text` holds, read with lines of up to `max_line_length` bytes.
std::vector<std::string> linesOf(std::string_view text, std::size_t max_line_length = 64)
{
    std::istringstream input{std::string{text}};
    LineReader reader{input, max_line_length, "test"};
    std::vector<std::string> lines;
    while (const auto line{reader.next()}) {
        lines.emplace_back(*line);
    }
    return lines;
}

// The line number a LineTooLongError reports, or zero if none is thrown.
std::size_t overlongLine(std::string_view text, std::size_t max_line_length)
{
    try {
        static_cast<void>(linesOf(text, max_line_length));
    } catch (const LineTooLongError& error) {
        return error.line();
    }
    return 0;
}

TEST(LineReaderTest, emptyInputHasNoLines)
{
    EXPECT_TRUE(linesOf("").empty());
}

TEST(LineReaderTest, readsLinesWithoutTheirTerminators)
{
    EXPECT_EQ(linesOf("first\nsecond\n"), (std::vector<std::string>{"first", "second"}));
}

TEST(LineReaderTest, acceptsWindowsLineEndingsAndNoFinalNewline)
{
    EXPECT_EQ(linesOf("first\r\nsecond"), (std::vector<std::string>{"first", "second"}));
}

TEST(LineReaderTest, keepsBlankLines)
{
    EXPECT_EQ(linesOf("\n\nthird\n"), (std::vector<std::string>{"", "", "third"}));
}

TEST(LineReaderTest, lineNumberIsZeroBeforeTheFirstLine)
{
    std::istringstream input{"a\n"};
    const LineReader reader{input, 64, "test"};

    EXPECT_EQ(reader.lineNumber(), 0U);
}

TEST(LineReaderTest, keepsReturningNothingAfterTheEnd)
{
    std::istringstream input{"a\nb"};
    LineReader reader{input, 64, "test"};
    static_cast<void>(reader.next());
    static_cast<void>(reader.next());

    EXPECT_FALSE(reader.next().has_value());
    EXPECT_FALSE(reader.next().has_value());
    EXPECT_EQ(reader.lineNumber(), 2U);
}

TEST(LineReaderTest, countsLinesFromOne)
{
    std::istringstream input{"a\nb\nc\n"};
    LineReader reader{input, 64, "test"};

    static_cast<void>(reader.next());
    static_cast<void>(reader.next());

    EXPECT_EQ(reader.lineNumber(), 2U);
}

TEST(LineReaderTest, acceptsLinesAtTheLengthLimitWithAnyEnding)
{
    // Exactly 16 bytes before the terminator.
    for (const std::string_view text :
         {"0123456789abcdef\n", "0123456789abcdef\r\n", "0123456789abcdef", "0123456789abcdef\r"}) {
        EXPECT_EQ(linesOf(text, 16), (std::vector<std::string>{"0123456789abcdef"}))
            << "text: " << text;
    }
}

TEST(LineReaderTest, rejectsALineOverflowingTheBuffer)
{
    EXPECT_EQ(overlongLine("ok\n0123456789abcdefghij\n", 16), 2U);
}

TEST(LineReaderTest, rejectsALineJustOverTheLimit)
{
    // 17 bytes fit the read buffer but exceed the limit, with or without a newline.
    EXPECT_EQ(overlongLine("ok\n0123456789abcdefg\n", 16), 2U);
    EXPECT_EQ(overlongLine("ok\n0123456789abcdefg", 16), 2U);
    EXPECT_EQ(overlongLine("ok\n0123456789abcdefg\r\n", 16), 2U);
}

TEST(LineReaderTest, overlongErrorNamesTheFormatLineAndLimit)
{
    try {
        static_cast<void>(linesOf("0123456789abcdefg\n", 16));
        FAIL() << "expected a LineTooLongError";
    } catch (const LineTooLongError& error) {
        EXPECT_EQ(error.limit(), 16U);
        EXPECT_STREQ(error.what(), "test line 1 is longer than 16 bytes");
    }
}

TEST(LineReaderTest, returnsNothingAfterAnOverlongLine)
{
    std::istringstream input{"0123456789abcdefg\nok\n"};
    LineReader reader{input, 16, "test"};
    EXPECT_THROW(static_cast<void>(reader.next()), LineTooLongError);

    EXPECT_FALSE(reader.next().has_value());
}

TEST(LineReaderTest, reportsAFailedRead)
{
    FailingStreambuf buffer{"first\n"};
    std::istream input{&buffer};
    LineReader reader{input, 64, "test"};
    static_cast<void>(reader.next());

    try {
        static_cast<void>(reader.next());
        FAIL() << "expected a runtime_error";
    } catch (const std::runtime_error& error) {
        EXPECT_STREQ(error.what(), "failed to read test input");
    }
}

TEST(LineReaderTest, failbitInTheExceptionMaskThrowsAtTheEnd)
{
    // Documented rather than wanted: getline sets failbit when it finds no line.
    std::istringstream input{"a\n"};
    input.exceptions(std::ios_base::failbit);
    LineReader reader{input, 64, "test"};

    EXPECT_EQ(reader.next(), std::optional<std::string_view>{"a"});
    EXPECT_THROW(static_cast<void>(reader.next()), std::ios_base::failure);
}

TEST(LineReaderTest, rejectsAStreamThatIsAlreadyFailed)
{
    std::istringstream input{"text\n"};
    input.setstate(std::ios_base::failbit);

    EXPECT_THROW(LineReader(input, 64, "test"), std::runtime_error);
}

TEST(LineReaderTest, rejectsALineLengthLimitTooLargeToBuffer)
{
    std::istringstream input{"text\n"};

    EXPECT_THROW(LineReader(input, std::numeric_limits<std::size_t>::max(), "test"),
                 std::invalid_argument);
}

TEST(LineReaderTest, rejectsTheSmallestLimitTooLargeToBuffer)
{
    // The buffer adds two bytes, and its size must fit std::streamsize.
    std::istringstream input{"text\n"};
    const auto smallest_too_large{
        static_cast<std::size_t>(std::numeric_limits<std::streamsize>::max()) - 1};

    EXPECT_THROW(LineReader(input, smallest_too_large, "test"), std::invalid_argument);
}

TEST(LineReaderTest, errorsNameTheFormat)
{
    std::istringstream input{"text\n"};
    input.setstate(std::ios_base::failbit);

    try {
        LineReader reader{input, 64, "Wavefront"};
        FAIL() << "expected a runtime_error";
    } catch (const std::runtime_error& error) {
        EXPECT_STREQ(error.what(), "Wavefront input stream is not readable");
    }
}

} // namespace
} // namespace albedo
