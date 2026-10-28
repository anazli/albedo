#pragma once

#include "albedo/geometry/triangle_mesh.hpp"

namespace albedo::test_support {

/// A tent 20 units long along z, with its ridge on the z axis at y = 0 and two
/// slopes falling at 45 degrees to eaves at x = -10 and x = 10, y = -10. Each
/// slope is two triangles, split so that both ridge positions belong to as much
/// left slope as right: their smooth normals are exactly +y, while the slopes'
/// own normals are (-1, 1, 0) and (1, 1, 0), normalized. Positions: 0 and 1 the
/// ridge (z = -10 and 10), 2 and 3 the left eave, 4 and 5 the right eave.
inline TriangleMesh tentMesh()
{
    return TriangleMesh{{{0.0, 0.0, -10.0},
                         {0.0, 0.0, 10.0},
                         {-10.0, -10.0, -10.0},
                         {-10.0, -10.0, 10.0},
                         {10.0, -10.0, -10.0},
                         {10.0, -10.0, 10.0}},
                        {{2, 3, 1}, {2, 1, 0}, {0, 1, 4}, {1, 5, 4}}};
}

} // namespace albedo::test_support
