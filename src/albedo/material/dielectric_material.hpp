#pragma once

#include "albedo/core/real.hpp"
#include "albedo/scattering/dielectric_lobe.hpp"
#include "albedo/scattering/scattering.hpp"
#include "albedo/scene/hit_record.hpp"

namespace albedo {

/// A transparent solid such as glass, surrounded by air (index 1); nested media
/// are not modelled. Precondition: `ior` positive and finite; `roughness` finite
/// and non-negative.
struct DielectricMaterial {
    /// Index of refraction of the material, e.g. about 1.5 for glass.
    Real ior{1.5};
    /// Perceptual roughness in [0, 1], squared into the GGX width as for
    /// `ConductorMaterial`; anything below about 0.01 is perfectly smooth.
    Real roughness{};

    /// Dielectric scattering around the hit's normal. A ray hitting the outside
    /// enters the material, one hitting the inside leaves it, so the index ratio
    /// inverts on back faces. Precondition: the normal has unit length.
    [[nodiscard]] Scattering scatteringAt(const HitRecord& hit) const noexcept
    {
        const Real eta{hit.front_face ? ior : 1 / ior};
        return {DielectricLobe{.eta = eta, .alpha = roughness * roughness}, hit.normal};
    }
};

} // namespace albedo
