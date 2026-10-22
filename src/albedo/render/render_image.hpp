#pragma once

#include "albedo/camera/perspective_camera.hpp"
#include "albedo/estimator/radiance_estimator.hpp"
#include "albedo/image/pixel_buffer.hpp"
#include "albedo/scene/scene.hpp"

#include <cstdint>
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
    /// Each pixel is split into an n x n grid of cells, with one sample at a
    /// random point of each cell.
    int samples_per_axis{1};
    /// Selects the random numbers; the same job and seed give the same image.
    std::uint64_t seed{0};
    /// How the radiance along each camera ray is estimated.
    RadianceEstimator estimator{ScatterOnlyEstimator{}};
};

/// Renders `job` into `image`, adding `samples_per_axis`^2 samples to every
/// pixel. Stratifying the samples (one per grid cell, jittered inside it)
/// spreads them more evenly than independent random positions, at no extra cost.
/// Each pixel draws from its own `SampleGenerator`: first a sample's position in
/// its cell, then whatever the estimator needs, cells in row-major order.
/// Throws `std::invalid_argument` if `samples_per_axis` is not positive, and
/// otherwise only what the estimator throws.
void renderImage(const RenderJob& job, PixelBuffer& image);

} // namespace albedo
