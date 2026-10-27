#include "albedo/io/tokens.hpp"

#include "support/require_value.hpp"

#include <gtest/gtest.h>

#include <limits>
#include <optional>
#include <source_location>
#include <string>
#include <string_view>
#include <vector>

namespace albedo {
namespace {

using test_support::requireValue;

std::vector<std::string> tokensOf(std::string_view line)
{
    Tokens tokens{line};
    std::vector<std::string> result;
    while (const auto token{tokens.next()}) {
        result.emplace_back(*token);
    }
    return result;
}

TEST(TokensTest, emptyLineHasNoTokens)
{
    EXPECT_TRUE(tokensOf("").empty());
    EXPECT_TRUE(tokensOf(" \t\r\v\f ").empty());
}

TEST(TokensTest, splitsAtRunsOfSpacesAndTabs)
{
    EXPECT_EQ(tokensOf("  v\t1   2 \t3  "), (std::vector<std::string>{"v", "1", "2", "3"}));
}

TEST(TokensTest, keepsEveryOtherCharacterInsideTokens)
{
    EXPECT_EQ(tokensOf("a/b//c x#y"), (std::vector<std::string>{"a/b//c", "x#y"}));
}

TEST(TokensTest, staysEmptyOnceExhausted)
{
    Tokens tokens{"only"};

    static_cast<void>(tokens.next());

    EXPECT_FALSE(tokens.next().has_value());
    EXPECT_FALSE(tokens.next().has_value());
}

// The value `token` parses to; fails the test, naming the caller's line, if it
// does not parse.
Real parsed(std::string_view token, std::source_location location = std::source_location::current())
{
    return requireValue(parseReal(token), location);
}

TEST(ParseRealTest, parsesDecimalAndExponentForms)
{
    EXPECT_DOUBLE_EQ(parsed("1.5"), 1.5);
    EXPECT_DOUBLE_EQ(parsed("-2"), -2.0);
    EXPECT_DOUBLE_EQ(parsed("1e-3"), 1e-3);
    EXPECT_DOUBLE_EQ(parsed(".25"), 0.25);
}

TEST(ParseRealTest, acceptsALeadingPlus)
{
    EXPECT_DOUBLE_EQ(parsed("+0.5"), 0.5);
}

TEST(ParseRealTest, acceptsSubnormalValues)
{
    EXPECT_DOUBLE_EQ(parsed("1e-310"), 1e-310);
}

TEST(ParseRealTest, rejectsValuesThatUnderflowToZero)
{
    // A deliberate tripwire: the documented behaviour depends on the standard
    // library, so a platform reading such values as zero should fail here.
    EXPECT_FALSE(parseReal("1e-400").has_value());
}

TEST(ParseRealTest, rejectsMalformedNumbers)
{
    for (const std::string_view token : {"", "+", "-", "++1", "+-1", "1.5x", "x1", "1 2", "0x10"}) {
        EXPECT_FALSE(parseReal(token).has_value()) << "token: '" << token << "'";
    }
}

TEST(ParseRealTest, rejectsNonFiniteAndOverflowingNumbers)
{
    for (const std::string_view token : {"nan", "inf", "-inf", "infinity", "1e999", "-1e999"}) {
        EXPECT_FALSE(parseReal(token).has_value()) << "token: '" << token << "'";
    }
}

TEST(ParseIntegerTest, parsesSignedDecimalIntegers)
{
    EXPECT_EQ(parseInteger("42"), 42);
    EXPECT_EQ(parseInteger("-7"), -7);
    EXPECT_EQ(parseInteger("-9223372036854775808"), std::numeric_limits<long long>::min());
}

TEST(ParseIntegerTest, rejectsMalformedAndOutOfRangeIntegers)
{
    for (const std::string_view token :
         {"", "+1", "1.5", "1x", "-", "99999999999999999999999", "9223372036854775808"}) {
        EXPECT_FALSE(parseInteger(token).has_value()) << "token: '" << token << "'";
    }
}

} // namespace
} // namespace albedo
