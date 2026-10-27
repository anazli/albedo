#pragma once

#include "albedo/scattering/lambert_lobe.hpp"
#include "albedo/scattering/scattering.hpp"
#include "albedo/scene/hit_record.hpp"
#include "albedo/texture/texture.hpp"

namespace albedo {

/// A matte surface, whose color may vary over it.
struct DiffuseMaterial {
    /// Fraction of incident light reflected, per channel, wherever a hit lies; see
    /// `LambertLobe::albedo`. Black by default.
    Texture albedo;

    /// Lambertian scattering around the hit's normal, with the albedo the texture
    /// gives at the hit. Precondition: the normal has unit length. Throws only what
    /// `evaluate(const Texture&, ...)` throws.
    [[nodiscard]] Scattering scatteringAt(const HitRecord& hit) const
    {
        return {LambertLobe{.albedo = evaluate(albedo, hit)}, hit.normal};
    }
};

} // namespace albedo
