#include "albedo/texture/perlin_noise.hpp"

#include "albedo/core/vector3.hpp"

#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace albedo {

namespace {

constexpr std::size_t lattice_period{256};

// Perlin's reference permutation of 0..255, so the pattern matches the
// published implementation.
constexpr std::array<std::uint8_t, lattice_period> permutation{
    151, 160, 137, 91,  90,  15,  131, 13,  201, 95,  96,  53,  194, 233, 7,   225, 140, 36,  103,
    30,  69,  142, 8,   99,  37,  240, 21,  10,  23,  190, 6,   148, 247, 120, 234, 75,  0,   26,
    197, 62,  94,  252, 219, 203, 117, 35,  11,  32,  57,  177, 33,  88,  237, 149, 56,  87,  174,
    20,  125, 136, 171, 168, 68,  175, 74,  165, 71,  134, 139, 48,  27,  166, 77,  146, 158, 231,
    83,  111, 229, 122, 60,  211, 133, 230, 220, 105, 92,  41,  55,  46,  245, 40,  244, 102, 143,
    54,  65,  25,  63,  161, 1,   216, 80,  73,  209, 76,  132, 187, 208, 89,  18,  169, 200, 196,
    135, 130, 116, 188, 159, 86,  164, 100, 109, 198, 173, 186, 3,   64,  52,  217, 226, 250, 124,
    123, 5,   202, 38,  147, 118, 126, 255, 82,  85,  212, 207, 206, 59,  227, 47,  16,  58,  17,
    182, 189, 28,  42,  223, 183, 170, 213, 119, 248, 152, 2,   44,  154, 163, 70,  221, 153, 101,
    155, 167, 43,  172, 9,   129, 22,  39,  253, 19,  98,  108, 110, 79,  113, 224, 232, 178, 185,
    112, 104, 218, 246, 97,  228, 251, 34,  242, 193, 238, 210, 144, 12,  191, 179, 162, 241, 81,
    51,  145, 235, 249, 14,  239, 107, 49,  192, 214, 31,  181, 199, 106, 157, 184, 84,  204, 176,
    115, 121, 50,  45,  127, 4,   150, 254, 138, 236, 205, 93,  222, 114, 67,  29,  24,  72,  243,
    141, 128, 195, 78,  66,  215, 61,  156, 180};

// True when `values` holds each of 0..255 exactly once. An initializer list
// shorter than the array would otherwise pad it with zeros unnoticed.
constexpr bool isPermutation(const std::array<std::uint8_t, lattice_period>& values)
{
    std::array<bool, lattice_period> seen{};
    for (const std::uint8_t value : values) {
        if (seen.at(value)) {
            return false;
        }
        seen.at(value) = true;
    }
    return true;
}
static_assert(isPermutation(permutation));

// The pseudo-random value for lattice index `index`, wrapped to the period.
constexpr std::size_t hashed(std::size_t index) noexcept
{
    return permutation.at(index % lattice_period);
}

// Where a coordinate lies in its lattice cell: the cell's index, wrapped to the
// period, and the offset into the cell in [0, 1).
struct CellPosition {
    std::size_t cell{};
    Real offset{};
};

CellPosition cellPosition(Real coordinate) noexcept
{
    const Real floor{std::floor(coordinate)};
    // Reduced in floating point first: the cell index itself can exceed every
    // integer type. fmod is exact, and the result lies in (-256, 256).
    Real wrapped{std::fmod(floor, static_cast<Real>(lattice_period))};
    if (wrapped < 0) {
        wrapped += static_cast<Real>(lattice_period);
    }
    return {.cell = static_cast<std::size_t>(wrapped), .offset = coordinate - floor};
}

// Perlin's quintic fade curve, 6t^5 - 15t^4 + 10t^3: flat at both ends, so the
// noise has continuous first and second derivatives across cells.
constexpr Real fade(Real t) noexcept
{
    return t * t * t * (t * (t * 6 - 15) + 10);
}

constexpr Real lerp(Real t, Real a, Real b) noexcept
{
    return a + t * (b - a);
}

// The dot product of `offset` with one of the 12 edge directions of a cube,
// picked by the low four bits of `hash` (Perlin 2002).
Real gradient(std::size_t hash, Vector3r offset) noexcept
{
    const std::size_t h{hash & 15U};
    const Real u{h < 8 ? offset.x : offset.y};
    Real v{offset.z};
    if (h < 4) {
        v = offset.y;
    } else if (h == 12 || h == 14) {
        v = offset.x;
    }
    return ((h & 1U) == 0 ? u : -u) + ((h & 2U) == 0 ? v : -v);
}

} // namespace

Real perlinNoise(Point3r point) noexcept
{
    if (!std::isfinite(point.x) || !std::isfinite(point.y) || !std::isfinite(point.z)) {
        return 0;
    }
    const auto [xi, x]{cellPosition(point.x)};
    const auto [yi, y]{cellPosition(point.y)};
    const auto [zi, z]{cellPosition(point.z)};
    const Real u{fade(x)};
    const Real v{fade(y)};
    const Real w{fade(z)};

    // Hashes for the eight corners of the cell, as in the reference code.
    const std::size_t a{hashed(xi) + yi};
    const std::size_t aa{hashed(a) + zi};
    const std::size_t ab{hashed(a + 1) + zi};
    const std::size_t b{hashed(xi + 1) + yi};
    const std::size_t ba{hashed(b) + zi};
    const std::size_t bb{hashed(b + 1) + zi};

    return lerp(
        w,
        lerp(v, lerp(u, gradient(hashed(aa), {x, y, z}), gradient(hashed(ba), {x - 1, y, z})),
             lerp(u, gradient(hashed(ab), {x, y - 1, z}), gradient(hashed(bb), {x - 1, y - 1, z}))),
        lerp(v,
             lerp(u, gradient(hashed(aa + 1), {x, y, z - 1}),
                  gradient(hashed(ba + 1), {x - 1, y, z - 1})),
             lerp(u, gradient(hashed(ab + 1), {x, y - 1, z - 1}),
                  gradient(hashed(bb + 1), {x - 1, y - 1, z - 1}))));
}

Real turbulence(Point3r point) noexcept
{
    Real sum{0};
    Real weight{1};
    Point3r octave_point{point};
    for (int octave{0}; octave < turbulence_octaves; ++octave) {
        sum += weight * std::abs(perlinNoise(octave_point));
        weight *= 0.5;
        octave_point = {octave_point.x * 2, octave_point.y * 2, octave_point.z * 2};
    }
    return sum;
}

} // namespace albedo
