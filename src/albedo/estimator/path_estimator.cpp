#include "albedo/estimator/path_estimator.hpp"

#include "albedo/core/interval.hpp"
#include "albedo/core/normal3.hpp"
#include "albedo/core/point3.hpp"
#include "albedo/core/real.hpp"
#include "albedo/core/vector3.hpp"
#include "albedo/estimator/surface_ray.hpp"
#include "albedo/light/light.hpp"
#include "albedo/scattering/scattering.hpp"

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstddef>

namespace albedo {

namespace {

// A shadow ray stops this fraction short of the light, so it does not hit the
// emitting surface itself; never less than twice the ray's start offset, which
// it must cover.
constexpr Real shadow_margin{1e-6};

// How the path arrived at its current vertex, for weighting emission it hits.
struct Arrival {
    // Where the previous scattering happened.
    Point3r origin{};
    // Density with which that scattering chose the direction.
    Real pdf{};
    // True when no light sample could have found what lies ahead (the camera
    // ray, or a specular bounce), so emission counts in full.
    bool specular{};
};

// The densities with which two sampling techniques could produce one sample.
struct TechniquePdfs {
    // The technique that actually drew the sample.
    Real chosen{};
    // The other technique, which could also have drawn it.
    Real other{};
};

// Veach's power heuristic with exponent 2 (thesis, sec. 9.2.4): the weight of a
// sample drawn by the `chosen` technique. Not symmetric, hence the named fields.
Real powerHeuristic(TechniquePdfs pdfs) noexcept
{
    const Real chosen_squared{pdfs.chosen * pdfs.chosen};
    const Real total{chosen_squared + pdfs.other * pdfs.other};
    return total > 0 ? chosen_squared / total : 0;
}

Real lightChoiceProbability(const Scene& scene) noexcept
{
    return Real{1} / static_cast<Real>(scene.lights().size());
}

// The range along a shadow ray that must be empty for a light `distance` away.
// The sky is infinitely far: its whole ray must be empty, and no margin applies
// (inf - inf would be NaN, and NaN compares as an empty range).
Interval shadowRange(Real distance) noexcept
{
    if (std::isinf(distance)) {
        return {.min = 0, .max = distance};
    }
    const Real margin{std::max(distance * shadow_margin, 2 * surface_offset)};
    return {.min = 0, .max = distance - margin};
}

// Light from one uniformly chosen light, arriving at `hit` and scattered
// towards `wo`, weighted against finding it by sampling the material.
Rgb sampleDirectLight(const Scene& scene, const HitRecord& hit, const Scattering& scattering,
                      Vector3r wo, SampleGenerator& generator)
{
    // Both numbers are drawn even when unused, so later draws do not depend on it.
    const Real u_light{generator.next1D()};
    const Point2r u{generator.next2D()};
    const auto lights{scene.lights()};
    if (lights.empty()) {
        return {};
    }
    const auto index{std::min(static_cast<std::size_t>(u_light * static_cast<Real>(lights.size())),
                              lights.size() - 1)};
    const auto sample{sampleIncidentLight(lights[index], hit.point, u)};
    if (!sample || isBlack(sample->radiance)) {
        return {};
    }
    const Rgb f{scattering.evaluate(wo, sample->direction)};
    const Interval unobstructed{shadowRange(sample->distance)};
    if (isBlack(f) || (unobstructed.max > 0 &&
                       scene.occluded(rayLeaving(hit, sample->direction), unobstructed))) {
        return {};
    }
    const Real light_pdf{lightChoiceProbability(scene) * sample->pdf};
    const Real weight{sample->delta
                          ? Real{1}
                          : powerHeuristic({.chosen = light_pdf,
                                            .other = scattering.pdf(wo, sample->direction)})};
    const Real cos_theta{std::abs(dot(sample->direction, hit.normal))};
    return f * sample->radiance * (cos_theta * weight / light_pdf);
}

// Emission seen at `hit` along `direction`, weighted against having found it
// by sampling a light at the previous vertex.
Rgb weightedEmission(const Scene& scene, const HitRecord& hit, Vector3r direction,
                     const Arrival& arrival)
{
    const Rgb emitted{scene.emittedRadiance(hit)};
    if (isBlack(emitted) || arrival.specular) {
        return emitted;
    }
    const auto index{scene.lightIndexOf(hit)};
    assert(index && "only lights emit");
    const Real light_pdf{lightChoiceProbability(scene) *
                         incidentLightPdf(scene.lights()[*index], arrival.origin, direction)};
    return emitted * powerHeuristic({.chosen = arrival.pdf, .other = light_pdf});
}

// The sky seen along `direction` when the path escapes, weighted likewise.
Rgb weightedSky(const Scene& scene, Vector3r direction, const Arrival& arrival)
{
    const SkyLight sky{scene.sky()};
    const Rgb emitted{sky.emittedRadiance(direction)};
    if (isBlack(emitted) || arrival.specular) {
        return emitted;
    }
    // A sky that emits anything has been set, so it is among the scene's lights.
    const Real light_pdf{lightChoiceProbability(scene) *
                         SkyLight::incidentLightPdf(arrival.origin, direction)};
    return emitted * powerHeuristic({.chosen = arrival.pdf, .other = light_pdf});
}

// Russian roulette: ends the path with probability 1 - p, and otherwise divides
// the throughput by p to keep the estimate unbiased. Returns whether it survives.
// The throughput includes the 1 / eta^2 radiance scaling of refraction, so paths
// inside dense glass end somewhat more often than their importance warrants:
// extra variance, not bias. Dividing that scaling out, as pbrt does, could come
// if glass-heavy scenes need it.
bool survivesRoulette(Rgb& throughput, SampleGenerator& generator) noexcept
{
    const Real survival{std::min(PathEstimator::max_survival_probability,
                                 std::max({throughput.r, throughput.g, throughput.b}))};
    if (generator.next1D() >= survival) {
        return false;
    }
    throughput /= survival;
    return true;
}

} // namespace

Rgb PathEstimator::radiance(const Ray& ray, const Scene& scene, SampleGenerator& generator) const
{
    assert(max_depth >= 0 && roulette_start >= 0 && "path limits must not be negative");
    Ray path_ray{ray};
    Rgb throughput{1, 1, 1};
    Rgb result{};
    Arrival arrival{.origin = ray.origin, .pdf = 0, .specular = true};
    for (int depth{0};; ++depth) {
        const auto hit{scene.intersect(path_ray, in_front_of_origin)};
        if (!hit) {
            return result + throughput * weightedSky(scene, path_ray.direction, arrival);
        }
        result += throughput * weightedEmission(scene, *hit, path_ray.direction, arrival);
        if (depth >= max_depth) {
            return result;
        }
        const Scattering scattering{scene.scatteringAt(*hit)};
        const Vector3r wo{-path_ray.direction};
        result += throughput * sampleDirectLight(scene, *hit, scattering, wo, generator);
        const Real u_choice{generator.next1D()};
        const auto sample{scattering.sample(wo, u_choice, generator.next2D())};
        if (!sample) {
            return result;
        }
        throughput *= sample->weight;
        if (isBlack(throughput) ||
            (depth >= roulette_start && !survivesRoulette(throughput, generator))) {
            return result;
        }
        arrival = {.origin = hit->point, .pdf = sample->pdf, .specular = sample->specular};
        path_ray = rayLeaving(*hit, sample->direction);
    }
}

} // namespace albedo
