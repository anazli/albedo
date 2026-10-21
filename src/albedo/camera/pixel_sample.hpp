#pragma once

#include "albedo/core/point2.hpp"

namespace albedo {

/// Where on the image a camera ray should go.
struct PixelSample {
    /// Position on the film in [0, 1]^2: (0, 0) is the top-left corner of the
    /// image and (1, 1) the bottom-right, independent of the resolution.
    Point2r film{};
};

} // namespace albedo
