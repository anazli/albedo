#pragma once

#include "albedo/color/rgb.hpp"
#include "albedo/scattering/lambert_lobe.hpp"
#include "albedo/scattering/scattering.hpp"
#include "albedo/scene/hit_record.hpp"

namespace albedo {

/// A matte surface of one color everywhere.
struct DiffuseMaterial {
    /// Fraction of incident light reflected, per channel; see `LambertLobe::albedo`.
    Rgb albedo{};

    /// Lambertian scattering around the hit's normal. Precondition: the normal has
    /// unit length.
    [[nodiscard]] Scattering scatteringAt(const HitRecord& hit) const noexcept
    {
        return {LambertLobe{.albedo = albedo}, hit.normal};
    }
};

} // namespace albedo
