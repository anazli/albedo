#pragma once

#include "albedo/geometry/shape.hpp"
#include "albedo/material/material.hpp"

#include <cstddef>
#include <optional>

namespace albedo {

/// One thing in a scene: where it is and what it is made of. The material is
/// held by value; sharing materials by id can come when scenes grow large. Each
/// triangle of a mesh added flattened holds its own copy, and `MixMaterial` makes
/// that copy about 190 bytes (mesh instances are one object each).
struct SceneObject {
    /// The object's geometry.
    Shape shape;
    /// What the object's surface is made of.
    Material material;
    /// Index of the area light in the scene's light list when the object emits.
    std::optional<std::size_t> light;
};

} // namespace albedo
