#include "albedo/core/transform.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <format>
#include <stdexcept>
#include <string_view>

namespace albedo {

namespace {

using detail::AffineMap;
using detail::InvertibleMap;
using Matrix = detail::Matrix3;
using Components = std::array<Real, 3>;

Components components(Vector3r v) noexcept
{
    return {v.x, v.y, v.z};
}

Components components(Point3r p) noexcept
{
    return {p.x, p.y, p.z};
}

// `matrix` times the column vector `v`.
Vector3r multiplied(const Matrix& matrix, Vector3r v) noexcept
{
    const auto& [r0, r1, r2]{matrix};
    return {r0[0] * v.x + r0[1] * v.y + r0[2] * v.z, r1[0] * v.x + r1[1] * v.y + r1[2] * v.z,
            r2[0] * v.x + r2[1] * v.y + r2[2] * v.z};
}

// The transpose of `matrix` times the column vector `v`.
Vector3r multipliedByTranspose(const Matrix& matrix, Vector3r v) noexcept
{
    const auto& [r0, r1, r2]{matrix};
    return {r0[0] * v.x + r1[0] * v.y + r2[0] * v.z, r0[1] * v.x + r1[1] * v.y + r2[1] * v.z,
            r0[2] * v.x + r1[2] * v.y + r2[2] * v.z};
}

Matrix transposed(const Matrix& matrix) noexcept
{
    Matrix result{};
    for (std::size_t row{0}; row < 3; ++row) {
        for (std::size_t column{0}; column < 3; ++column) {
            result.at(row).at(column) = matrix.at(column).at(row);
        }
    }
    return result;
}

// `outer` applied after `inner`.
AffineMap composed(const AffineMap& outer, const AffineMap& inner) noexcept
{
    AffineMap result{};
    for (std::size_t row{0}; row < 3; ++row) {
        for (std::size_t column{0}; column < 3; ++column) {
            Real sum{0};
            for (std::size_t k{0}; k < 3; ++k) {
                sum += outer.linear.at(row).at(k) * inner.linear.at(k).at(column);
            }
            result.linear.at(row).at(column) = sum;
        }
    }
    result.offset = multiplied(outer.linear, inner.offset) + outer.offset;
    return result;
}

bool isFinite(const AffineMap& map) noexcept
{
    const auto finite{[](Real value) { return std::isfinite(value); }};
    return std::ranges::all_of(
               map.linear,
               [&](const Components& row) { return std::ranges::all_of(row, finite); }) &&
           std::ranges::all_of(components(map.offset), finite);
}

// Throws unless `value` is finite, naming `what` in the message.
void checkFinite(Real value, std::string_view what)
{
    if (!std::isfinite(value)) {
        throw std::invalid_argument{std::format("transform {} {} is not finite", what, value)};
    }
}

// The rotation matrix for unit `axis` (Rodrigues' formula).
Matrix rotationMatrix(Vector3r axis, Real radians) noexcept
{
    const Real c{std::cos(radians)};
    const Real s{std::sin(radians)};
    const Real t{1 - c};
    const auto [x, y, z]{components(axis)};
    return {{{t * x * x + c, t * x * y - s * z, t * x * z + s * y},
             {t * x * y + s * z, t * y * y + c, t * y * z - s * x},
             {t * x * z - s * y, t * y * z + s * x, t * z * z + c}}};
}

struct AxisRange {
    Real min{};
    Real max{};
};

// The range a box's coordinates along one output axis cover: each entry of
// the matrix row scales one input interval, and the smallest and largest sums
// pick the matching end of each (Arvo, "Transforming Axis-Aligned Bounding
// Boxes", Graphics Gems, 1990). The sums are formed in the same order as
// `multiplied(...) + offset` in `Transform::apply(Point3r)`; rounded addition
// is monotonic, so every transformed corner then lies inside the range exactly.
// This relies on the products not being fused into FMAs differently in the two
// places: CMake builds this file with floating-point contraction off, as for
// triangle.cpp, and MSVC's /fp:precise does not contract.
AxisRange transformedRange(const Components& row, Real offset, const Bounds3r& box) noexcept
{
    const Components low{components(box.min)};
    const Components high{components(box.max)};
    AxisRange range{};
    for (std::size_t axis{0}; axis < 3; ++axis) {
        const Real a{row.at(axis) * low.at(axis)};
        const Real b{row.at(axis) * high.at(axis)};
        range.min = axis == 0 ? std::min(a, b) : range.min + std::min(a, b);
        range.max = axis == 0 ? std::max(a, b) : range.max + std::max(a, b);
    }
    range.min += offset;
    range.max += offset;
    return range;
}

} // namespace

Transform Transform::translation(Vector3r offset)
{
    for (const Real component : components(offset)) {
        checkFinite(component, "translation component");
    }
    return Transform{InvertibleMap{.map = {.offset = offset}, .inverse = {.offset = -offset}}};
}

Transform Transform::scaling(Vector3r factors)
{
    AffineMap forward{};
    AffineMap backward{};
    const Components scale{components(factors)};
    for (std::size_t axis{0}; axis < 3; ++axis) {
        const Real factor{scale.at(axis)};
        if (!std::isfinite(factor) || factor == 0 || !std::isfinite(1 / factor)) {
            throw std::invalid_argument{std::format(
                "transform scale factor {} must be finite and non-zero with a finite reciprocal",
                factor)};
        }
        forward.linear.at(axis).at(axis) = factor;
        backward.linear.at(axis).at(axis) = 1 / factor;
    }
    return Transform{InvertibleMap{.map = forward, .inverse = backward}};
}

Transform Transform::rotation(Vector3r axis, Real degrees)
{
    checkFinite(degrees, "rotation angle");
    if (!std::isnormal(lengthSquared(axis))) {
        throw std::invalid_argument{
            std::format("transform rotation axis ({}, {}, {}) has no usable direction", axis.x,
                        axis.y, axis.z)};
    }
    const Matrix forward{rotationMatrix(normalized(axis), degrees * (pi / 180))};
    // A rotation's inverse is its transpose.
    return Transform{
        InvertibleMap{.map = {.linear = forward}, .inverse = {.linear = transposed(forward)}}};
}

Transform Transform::operator*(const Transform& first) const
{
    const InvertibleMap product{.map = composed(maps.map, first.maps.map),
                                .inverse = composed(first.maps.inverse, maps.inverse)};
    if (!isFinite(product.map) || !isFinite(product.inverse)) {
        throw std::invalid_argument{"composed transform has a non-finite entry"};
    }
    return Transform{product};
}

Transform Transform::inverse() const noexcept
{
    return Transform{InvertibleMap{.map = maps.inverse, .inverse = maps.map}};
}

Point3r Transform::apply(Point3r p) const noexcept
{
    const Vector3r moved{multiplied(maps.map.linear, p - Point3r{}) + maps.map.offset};
    return Point3r{} + moved;
}

Vector3r Transform::apply(Vector3r v) const noexcept
{
    return multiplied(maps.map.linear, v);
}

Normal3r Transform::apply(Normal3r n) const noexcept
{
    return asNormal(multipliedByTranspose(maps.inverse.linear, asVector(n)));
}

Ray Transform::apply(const Ray& ray) const noexcept
{
    return {.origin = apply(ray.origin), .direction = apply(ray.direction)};
}

Bounds3r Transform::apply(const Bounds3r& box) const noexcept
{
    if (box.isEmpty()) {
        return {};
    }
    assert(isFinite(box.min) && isFinite(box.max) && "a box to transform must be finite");
    const Components offset{components(maps.map.offset)};
    std::array<AxisRange, 3> ranges{};
    for (std::size_t axis{0}; axis < 3; ++axis) {
        ranges.at(axis) = transformedRange(maps.map.linear.at(axis), offset.at(axis), box);
    }
    return {.min = {ranges[0].min, ranges[1].min, ranges[2].min},
            .max = {ranges[0].max, ranges[1].max, ranges[2].max}};
}

} // namespace albedo
