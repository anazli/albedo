#pragma once

#include "albedo/color/rgb.hpp"
#include "albedo/scene/hit_record.hpp"

namespace albedo {

/// The same color everywhere on a surface.
struct ConstantTexture {
    /// The color, per channel.
    Rgb color{};

    /// `color`, wherever `hit` lies.
    [[nodiscard]] Rgb evaluate([[maybe_unused]] const HitRecord& hit) const noexcept
    {
        return color;
    }
};

} // namespace albedo
