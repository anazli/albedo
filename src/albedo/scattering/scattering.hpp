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

/// The two normals of a surface point that scattering needs, named so they
/// cannot be passed the wrong way round.
struct SurfaceNormals {
    /// The unit normal the lobe is oriented around.
    Normal3r shading{};
    /// The actual surface's normal; it need not have unit length.
    Normal3r geometric{};
};

/// How light scatters at one surface point: a lobe placed in the local frame
/// of the point's shading normal. Takes and returns world-space unit directions,
/// with `wo` pointing towards the viewer and `wi` towards the light.
///
/// A shading normal leaning away from the geometric one leaves directions that
/// lie on one side of the actual surface but the other side of the shading
/// frame. Those scatter nothing: `evaluate` and `pdf` give zero when `wo` or
/// `wi` is one, and `sample` gives nothing. Otherwise light reflected in the
/// shading frame could leave through the surface, or light from behind it could
/// arrive as if from the front.
class Scattering {
public:
    /// Orients `lobe` around `normal`, which is also the geometric normal.
    /// Precondition: `normal` has unit length.
    Scattering(Lobe lobe, Normal3r normal) noexcept;

    /// Orients `lobe` around `normals.shading` on a surface whose actual normal is
    /// `normals.geometric`. Precondition: `normals.shading` has unit length, and
    /// `normals.geometric` is not zero.
    Scattering(Lobe lobe, SurfaceNormals normals) noexcept;

    /// The scattering function f(wo, wi), without the cosine factor.
    /// Throws only what `evaluate(const Lobe&, ...)` throws.
    [[nodiscard]] Rgb evaluate(Vector3r wo, Vector3r wi) const;

    /// A world-space incident direction for the random numbers `u_choice` in
    /// [0, 1) and `u` in [0, 1]^2, or nothing when the lobe produces no sample
    /// (see `sample(const Lobe&, ...)` for what each number decides).
    /// Throws only what `sample(const Lobe&, ...)` throws.
    [[nodiscard]] std::optional<ScatterSample> sample(Vector3r wo, Real u_choice, Point2r u) const;

    /// Solid-angle density with which `sample` draws `wi` given `wo`.
    /// Throws only what `pdf(const Lobe&, ...)` throws.
    [[nodiscard]] Real pdf(Vector3r wo, Vector3r wi) const;

private:
    // True when `direction` lies on different sides of the shading frame and the
    // geometric surface. Never true when the two normals are the same.
    [[nodiscard]] bool crossesSurface(Vector3r direction) const noexcept;

    Lobe lobe;
    OrthonormalBasis basis;
    Normal3r geometric;
};

} // namespace albedo
