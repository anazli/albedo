#pragma once

#include "albedo/color/rgb.hpp"
#include "albedo/core/normal3.hpp"
#include "albedo/core/orthonormal_basis.hpp"
#include "albedo/core/point2.hpp"
#include "albedo/core/real.hpp"
#include "albedo/core/vector3.hpp"
#include "albedo/scattering/lobe.hpp"
#include "albedo/scattering/scatter_sample.hpp"

#include <optional>

namespace albedo {

/// How light scatters at one surface point: a lobe placed in the local frame
/// of the point's normal. Takes and returns world-space unit directions, with
/// `wo` pointing towards the viewer and `wi` towards the light.
class Scattering {
public:
    /// Orients `lobe` around `normal`. Precondition: `normal` has unit length.
    Scattering(Lobe lobe, Normal3r normal) noexcept;

    /// The scattering function f(wo, wi), without the cosine factor.
    /// Throws only what `evaluate(const Lobe&, ...)` throws.
    [[nodiscard]] Rgb evaluate(Vector3r wo, Vector3r wi) const;

    /// A world-space incident direction for the random point `u` in [0, 1]^2, or
    /// nothing when the lobe cannot scatter light towards `wo`.
    /// Throws only what `sample(const Lobe&, ...)` throws.
    [[nodiscard]] std::optional<ScatterSample> sample(Vector3r wo, Point2r u) const;

    /// Solid-angle density with which `sample` draws `wi` given `wo`.
    /// Throws only what `pdf(const Lobe&, ...)` throws.
    [[nodiscard]] Real pdf(Vector3r wo, Vector3r wi) const;

private:
    Lobe lobe;
    OrthonormalBasis basis;
};

} // namespace albedo
