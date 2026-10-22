#pragma once

#include "albedo/color/rgb.hpp"
#include "albedo/core/real.hpp"
#include "albedo/core/vector3.hpp"

namespace albedo {

/// A direction towards a light, chosen by sampling the light from a receiving point.
struct LightSample {
    /// Unit direction from the receiver towards the light.
    Vector3r direction{};
    /// Radiance arriving at the receiver along `direction`, if nothing is in the
    /// way. For a `delta` light, the light's whole contribution (such as
    /// intensity / distance^2 for a point light) instead.
    Rgb radiance{};
    /// Distance from the receiver to the emitting point; infinite for the sky.
    /// A shadow ray must find nothing closer.
    Real distance{};
    /// Solid-angle density with which `direction` was chosen; positive. For a
    /// `delta` light, the probability of the choice (1) instead.
    Real pdf{};
    /// True when the light shines from a single direction (a point light), which
    /// no other sampling technique can find.
    bool delta{};
};

} // namespace albedo
