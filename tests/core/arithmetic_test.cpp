#include "albedo/core/arithmetic.hpp"

#include <gtest/gtest.h>

#include <cstddef>

namespace albedo {
namespace {

TEST(ArithmeticTest, acceptsSignedIntegersAndFloatingPoint)
{
    static_assert(Arithmetic<int>);
    static_assert(Arithmetic<long long>);
    static_assert(Arithmetic<signed char>);
    static_assert(Arithmetic<float>);
    static_assert(Arithmetic<double>);
}

TEST(ArithmeticTest, rejectsBool)
{
    static_assert(!Arithmetic<bool>);
    static_assert(!Arithmetic<const bool>);
}

TEST(ArithmeticTest, rejectsCvQualifiedTypes)
{
    static_assert(!Arithmetic<const int>);
    static_assert(!Arithmetic<volatile double>);
}

TEST(ArithmeticTest, rejectsUnsignedTypes)
{
    // Negation and point differences would wrap around.
    static_assert(!Arithmetic<unsigned>);
    static_assert(!Arithmetic<std::size_t>);
}

TEST(ArithmeticTest, rejectsCharacterTypes)
{
    static_assert(!Arithmetic<char>);
    static_assert(!Arithmetic<wchar_t>);
    static_assert(!Arithmetic<char8_t>);
    static_assert(!Arithmetic<char16_t>);
    static_assert(!Arithmetic<char32_t>);
}

} // namespace
} // namespace albedo
