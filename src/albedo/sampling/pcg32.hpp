#pragma once

#include "albedo/core/real.hpp"

#include <cstdint>
#include <limits>

namespace albedo {

/// The PCG32 pseudo-random number generator (PCG-XSH-RR with 64-bit state and
/// 32-bit output), from M. E. O'Neill, "PCG: A Family of Simple Fast
/// Space-Efficient Statistically Good Algorithms for Random Number Generation"
/// (2014), pcg-random.org.
///
/// Small, fast and statistically good, and it supports 2^63 independent
/// streams, which gives every pixel its own sequence. Unsigned arithmetic is
/// intentional here: the algorithm relies on modulo-2^64 wrap-around.
class Pcg32 {
public:
    /// Where to start: named fields, so the two integers cannot be swapped silently.
    struct Seeding {
        /// Starting position within the stream.
        std::uint64_t seed{};
        /// Which of the 2^63 independent sequences to use.
        std::uint64_t stream{};
    };

    /// Starts the sequence `seeding.stream` at position `seeding.seed`, following
    /// the reference `pcg32_srandom_r`.
    explicit constexpr Pcg32(Seeding seeding) noexcept : increment{(seeding.stream << 1U) | 1U}
    {
        // The reference seeding steps the generator and discards the output on purpose.
        static_cast<void>(nextUint32());
        state += seeding.seed;
        static_cast<void>(nextUint32());
    }

    /// The next 32 uniformly distributed bits.
    [[nodiscard]] constexpr std::uint32_t nextUint32() noexcept
    {
        const std::uint64_t old_state{state};
        state = old_state * multiplier + increment;
        // Output permutation: xorshift the high bits, then rotate by the top 5 bits.
        const auto xorshifted{static_cast<std::uint32_t>(((old_state >> 18U) ^ old_state) >> 27U)};
        const auto rotation{static_cast<std::uint32_t>(old_state >> 59U)};
        return (xorshifted >> rotation) | (xorshifted << ((32U - rotation) & 31U));
    }

    /// A uniformly distributed value in [0, 1). Every one of the 2^32 outputs maps
    /// to a distinct, exactly representable double, and 1 is never returned.
    [[nodiscard]] constexpr Real nextReal() noexcept
    {
        // Needs at least 32 significand bits, or outputs near 2^32 would round to 1.
        static_assert(std::numeric_limits<Real>::digits >= 32);
        constexpr Real two_to_minus_32{1.0 / 4294967296.0};
        return static_cast<Real>(nextUint32()) * two_to_minus_32;
    }

private:
    static constexpr std::uint64_t multiplier{6364136223846793005ULL};

    std::uint64_t state{0};
    std::uint64_t increment{1};
};

} // namespace albedo
