#pragma once

#include "albedo/material/basic_material.hpp"
#include "albedo/material/conductor_material.hpp"
#include "albedo/material/dielectric_material.hpp"
#include "albedo/material/diffuse_material.hpp"
#include "albedo/material/mix_material.hpp"

namespace albedo {

/// Any material the renderer supports. A closed set, like `Shape`; scatter
/// from it with `scatteringAt(const MaterialVariant<...>&, ...)`.
using Material =
    MaterialVariant<DiffuseMaterial, ConductorMaterial, DielectricMaterial, MixMaterial>;

} // namespace albedo
