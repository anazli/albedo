#pragma once

#include "albedo/color/rgb.hpp"
#include "albedo/core/real.hpp"
#include "albedo/scattering/conductor_lobe.hpp"
#include "albedo/scattering/scattering.hpp"
#include "albedo/scene/hit_record.hpp"

namespace albedo {

/// A metal, given by its complex refractive index and surface roughness.
/// Precondition: `eta` positive and `k` non-negative per channel; `roughness`
/// finite and non-negative.
struct ConductorMaterial {
    /// Real part of the refractive index relative to air, per channel.
    Rgb eta{1, 1, 1};
    /// Absorption (imaginary part of the index), per channel.
    Rgb k{};
    /// Perceptual roughness in [0, 1]: 1 is very rough, and anything below about
    /// 0.01 gives a perfect mirror (see `ConductorLobe::smooth_alpha`).
    Real roughness{};

    /// Conductor scattering around the hit's normal. Roughness is squared into
    /// the GGX width, which spreads perceived roughness evenly over [0, 1]
    /// (Burley, "Physically-Based Shading at Disney", SIGGRAPH 2012 course).
    /// Precondition: the normal has unit length.
    [[nodiscard]] Scattering scatteringAt(const HitRecord& hit) const noexcept
    {
        return {ConductorLobe{.eta = eta, .k = k, .alpha = roughness * roughness}, hit.normal};
    }
};

} // namespace albedo
