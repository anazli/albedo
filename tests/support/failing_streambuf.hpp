#pragma once

#include <cstddef>
#include <ios>
#include <iterator>
#include <stdexcept>
#include <streambuf>
#include <string>
#include <utility>

namespace albedo::test_support {

/// A stream buffer that serves `text` and then fails the next read, as a disk
/// or network error would. An `std::istream` over it sets badbit instead of
/// passing the exception on, unless badbit is in its `exceptions()` mask.
class FailingStreambuf : public std::streambuf {
public:
    explicit FailingStreambuf(std::string text) : contents{std::move(text)}
    {
        char* const begin{contents.data()};
        setg(begin, begin, std::next(begin, static_cast<std::ptrdiff_t>(contents.size())));
    }

    // A copy would keep pointers into the original's text.
    FailingStreambuf(const FailingStreambuf&) = delete;
    FailingStreambuf& operator=(const FailingStreambuf&) = delete;
    FailingStreambuf(FailingStreambuf&&) = delete;
    FailingStreambuf& operator=(FailingStreambuf&&) = delete;
    ~FailingStreambuf() override = default;

protected:
    int_type underflow() override { throw std::runtime_error{"simulated read failure"}; }

private:
    std::string contents;
};

} // namespace albedo::test_support
