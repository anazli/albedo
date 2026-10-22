#pragma once

#include "albedo/geometry/shape.hpp"
#include "albedo/material/material.hpp"

namespace albedo {

/// One thing in a scene: where it is and what it is made of. The material is
/// held by value; sharing materials by id can come when scenes grow large.
struct SceneObject {
    /// The object's geometry.
    Shape shape;
    /// What the object's surface is made of.
    Material material;
};

} // namespace albedo
