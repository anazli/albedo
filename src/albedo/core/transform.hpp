#pragma once

#include "albedo/core/bounds3.hpp"
#include "albedo/core/normal3.hpp"
#include "albedo/core/point3.hpp"
#include "albedo/core/ray.hpp"
#include "albedo/core/real.hpp"
#include "albedo/core/vector3.hpp"

#include <array>

namespace albedo {

namespace detail {

/// A 3x3 matrix, indexed `[row][column]`.
using Matrix3 = std::array<std::array<Real, 3>, 3>;

/// The map p -> linear p + offset. `linear` multiplies column vectors.
struct AffineMap {
    Matrix3 linear{{{1, 0, 0}, {0, 1, 0}, {0, 0, 1}}};
    Vector3r offset{};
};

/// An affine map together with the map that undoes it.
struct InvertibleMap {
    AffineMap map{};
    AffineMap inverse{};
};

} // namespace detail

/// An affine map of space (any combination of translations, rotations and
/// scalings), kept together with its inverse. Transforms are built only from
/// factories whose inverse is known in closed form, and by composing them, so no
/// matrix is ever inverted numerically; the inverse is exact up to rounding. The
/// factories' and composition's finiteness checks keep every map invertible.
class Transform {
public:
    /// The identity.
    Transform() = default;

    /// Moves every point by `offset`. Throws `std::invalid_argument` unless every
    /// component of `offset` is finite.
    [[nodiscard]] static Transform translation(Vector3r offset);

    /// Scales each axis by its factor; a negative factor mirrors that axis.
    /// Throws `std::invalid_argument` unless every factor is finite and non-zero
    /// with a finite reciprocal.
    [[nodiscard]] static Transform scaling(Vector3r factors);

    /// Turns space by `degrees` about `axis` through the origin, counter-clockwise
    /// when looking from the tip of `axis` towards the origin (the right-hand
    /// rule). `axis` need not have unit length. Throws `std::invalid_argument`
    /// unless `degrees` is finite and the axis's squared length is a normal
    /// floating-point number (roughly 1e-154 to 1e154 in length for double).
    [[nodiscard]] static Transform rotation(Vector3r axis, Real degrees);

    /// The transform applying `first`, then `*this`: `(a * b).apply(p)` is
    /// `a.apply(b.apply(p))`, up to rounding. Throws `std::invalid_argument` if an
    /// entry of the product, or of its inverse, is not finite.
    [[nodiscard]] Transform operator*(const Transform& first) const;

    /// The transform undoing this one.
    [[nodiscard]] Transform inverse() const noexcept;

    /// `p` moved by the transform.
    [[nodiscard]] Point3r apply(Point3r p) const noexcept;

    /// `v` turned and scaled by the transform; translation does not affect it.
    [[nodiscard]] Vector3r apply(Vector3r v) const noexcept;

    /// A normal of the transformed surface, given a normal `n` of the original:
    /// `n` multiplied by the inverse transpose, which keeps it perpendicular to the
    /// surface under non-uniform scaling. Not normalized; callers normalize it.
    [[nodiscard]] Normal3r apply(Normal3r n) const noexcept;

    /// `ray` with its origin and direction transformed. The direction is not
    /// normalized, so `apply(ray).at(t)` is `apply(ray.at(t))` for any `t`.
    [[nodiscard]] Ray apply(const Ray& ray) const noexcept;

    /// The smallest axis-aligned box enclosing the transformed `box`; an empty box
    /// stays empty. It contains `apply(corner)` for each corner exactly, rounding
    /// included. A transformed coordinate may overflow to infinity.
    /// Precondition: an empty `box`, or one with finite coordinates.
    [[nodiscard]] Bounds3r apply(const Bounds3r& box) const noexcept;

private:
    // Precondition: `invertible.inverse` undoes `invertible.map`.
    explicit Transform(const detail::InvertibleMap& invertible) noexcept : maps{invertible} {}

    detail::InvertibleMap maps{};
};

} // namespace albedo
