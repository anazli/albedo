#pragma once

#include "albedo/core/normal3.hpp"
#include "albedo/core/vector3.hpp"

namespace albedo {

/// A right-handed orthonormal frame (tangent, bitangent, normal).
///
/// Scattering is evaluated in local space where the normal is +z, so this
/// converts directions between world space and that local space.
class OrthonormalBasis {
public:
    /// Builds a frame whose normal axis is `n`.
    /// Precondition: `n` has unit length.
    [[nodiscard]] static OrthonormalBasis fromNormal(Normal3r n) noexcept;

    /// Local x axis in world space.
    [[nodiscard]] Vector3r tangent() const noexcept { return tangent_axis; }

    /// Local y axis in world space.
    [[nodiscard]] Vector3r bitangent() const noexcept { return bitangent_axis; }

    /// Local z axis in world space.
    [[nodiscard]] Vector3r normal() const noexcept { return normal_axis; }

    /// `world` expressed in the frame's coordinates.
    [[nodiscard]] Vector3r toLocal(Vector3r world) const noexcept
    {
        return {dot(world, tangent_axis), dot(world, bitangent_axis), dot(world, normal_axis)};
    }

    /// `local` frame coordinates expressed in world space.
    [[nodiscard]] Vector3r toWorld(Vector3r local) const noexcept
    {
        return tangent_axis * local.x + bitangent_axis * local.y + normal_axis * local.z;
    }

private:
    // Private so only fromNormal can create a basis; it assigns the axes by name.
    OrthonormalBasis() = default;

    Vector3r tangent_axis{};
    Vector3r bitangent_axis{};
    Vector3r normal_axis{};
};

} // namespace albedo
