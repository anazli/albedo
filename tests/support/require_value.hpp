#pragma once

#include <format>
#include <optional>
#include <source_location>
#include <stdexcept>

namespace albedo::test_support {

/// The value inside `optional`, or a thrown `std::logic_error` naming the calling
/// file and line (GoogleTest reports it as a test failure) when it is empty. Used
/// instead of `ASSERT_TRUE` followed by `*optional`, because clang-tidy cannot see
/// through the assert macro.
template <typename T>
T requireValue(std::optional<T> optional,
               std::source_location location = std::source_location::current())
{
    if (!optional) {
        throw std::logic_error{std::format("{}:{}: expected the optional to hold a value",
                                           location.file_name(), location.line())};
    }
    return *optional;
}

} // namespace albedo::test_support
