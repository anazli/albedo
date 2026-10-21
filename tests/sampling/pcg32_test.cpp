#include "albedo/sampling/pcg32.hpp"

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <cstdint>

namespace albedo {
namespace {

TEST(Pcg32Test, matchesReferenceOutput)
{
    // First outputs of pcg32 seeded with (42, 54), as printed by the reference
    // implementation's demo program (pcg-c-basic, pcg32-demo.c).
    constexpr std::array<std::uint32_t, 6> expected{0xa15c02b7, 0x7b47f409, 0xba1d3330,
                                                    0x83d2f293, 0xbfa4784b, 0xcbed606e};
    Pcg32 generator{{.seed = 42, .stream = 54}};

    for (const std::uint32_t value : expected) {
        EXPECT_EQ(generator.nextUint32(), value);
    }
}

TEST(Pcg32Test, isUsableAtCompileTime)
{
    static_assert(Pcg32{{.seed = 42, .stream = 54}}.nextUint32() == 0xa15c02b7);
}

TEST(Pcg32Test, sameSeedAndStreamGiveSameSequence)
{
    Pcg32 first{{.seed = 7, .stream = 3}};
    Pcg32 second{{.seed = 7, .stream = 3}};

    for (int i{0}; i < 100; ++i) {
        EXPECT_EQ(first.nextUint32(), second.nextUint32());
    }
}

TEST(Pcg32Test, differentStreamsGiveDifferentSequences)
{
    Pcg32 first{{.seed = 7, .stream = 3}};
    Pcg32 second{{.seed = 7, .stream = 4}};

    int equal_outputs{0};
    for (int i{0}; i < 100; ++i) {
        equal_outputs += first.nextUint32() == second.nextUint32() ? 1 : 0;
    }

    EXPECT_LT(equal_outputs, 3);
}

TEST(Pcg32Test, nextRealIsInUnitInterval)
{
    Pcg32 generator{{.seed = 1, .stream = 1}};

    for (int i{0}; i < 100'000; ++i) {
        const Real value{generator.nextReal()};
        ASSERT_GE(value, 0.0);
        ASSERT_LT(value, 1.0);
    }
}

TEST(Pcg32Test, nextRealIsUniformlyDistributed)
{
    // Chi-square test over 16 equal bins with a fixed seed. With 15 degrees of
    // freedom, chi-square stays below 37.7 with probability 0.999.
    constexpr int bins{16};
    constexpr int draws{160'000};
    std::array<int, bins> counts{};
    Pcg32 generator{{.seed = 2024, .stream = 11}};

    for (int i{0}; i < draws; ++i) {
        ++counts.at(static_cast<std::size_t>(generator.nextReal() * bins));
    }

    constexpr Real expected{static_cast<Real>(draws) / bins};
    Real chi_square{0};
    for (const int count : counts) {
        chi_square += (count - expected) * (count - expected) / expected;
    }
    EXPECT_LT(chi_square, 37.7);
}

} // namespace
} // namespace albedo
