#pragma once

#include "albedo/camera/perspective_camera.hpp"
#include "albedo/image/pixel_buffer.hpp"
#include "albedo/scene/scene.hpp"

#include <functional>

namespace albedo {

/// What to render. `std::reference_wrapper` instead of reference members keeps
/// the aggregate assignable and refuses temporaries, so a job cannot be built
/// from a camera or scene that dies at the end of the full-expression. The
/// camera and scene must still outlive the job.
struct RenderJob {
    /// The camera to look through.
    std::reference_wrapper<const PerspectiveCamera> camera;
    /// What to look at.
    std::reference_wrapper<const Scene> scene;
    /// Each pixel is sampled on an n x n grid of evenly spaced points.
    int samples_per_axis{1};
};

/// Renders `job` into `image` with the normals estimator, adding
/// `samples_per_axis`^2 samples to every pixel. A fixed grid rather than random
/// positions keeps the output deterministic until a sampler exists.
/// Throws `std::invalid_argument` if `samples_per_axis` is not positive, and
/// otherwise only what `Scene::intersect` throws.
void renderImage(const RenderJob& job, PixelBuffer& image);

} // namespace albedo
