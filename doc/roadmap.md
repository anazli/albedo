# Albedo roadmap

Albedo is a physically based path tracer in C++20, built one step at a time.

## How we work

Every step follows test-driven development:

1. Write the tests for the step's behaviour first, including edge cases, and confirm they fail (or fail to compile).
2. Implement the minimum needed to make them pass.
3. Refactor, then run `/verify` (build, tests under ASan/UBSan, clang-format, clang-tidy).
4. Tick the step below with the completion date and stop for review before starting the next one.

A step is complete only when the build is green, all tests pass, and `/verify` is clean.

## Sources

The design draws on the literature: *Ray Tracing in One Weekend* (Shirley et al.), Veach's PhD thesis (1997), Heitz's microfacet papers (2014, 2018) and *Physically Based Rendering* (Pharr, Jakob, Humphreys) as one reference among several. No code or text is copied from any of them. Wherever a formula follows a specific source, a comment at that spot cites it.

## Naming vocabulary

| Concept | Name | API |
|---|---|---|
| Surface hit info | `HitRecord` (shape-level: `ShapeHit`) | point, normal, uv, `t`, front-face flag |
| Local basis | `OrthonormalBasis` | `toLocal`, `toWorld` |
| Scattering function at a hit | `Scattering` (lobe + basis) | `evaluate(wo, wi)`, `sample(wo, u_choice, u)`, `pdf(wo, wi)` |
| Scattering model | `Lobe` variant: `LambertLobe`, `ConductorLobe`, `DielectricLobe` | same three functions in local space |
| Material | `Material` variant | `scatteringAt(const HitRecord&)` |
| Light | `Light` variant: `PointLight`, `AreaLight`, `SkyLight` | `sampleIncidentLight`, `incidentLightPdf`, `emittedRadiance` |
| Integrator | `RadianceEstimator` variant: `NormalsEstimator`, `ScatterOnlyEstimator`, `PathEstimator` | `radiance(ray, scene, generator)` |
| Film | `PixelBuffer` | `addSample`, `resolve`, then `writePpm` |
| Camera sample | `PixelSample` | |
| Sampler | `SampleGenerator` (with `Pcg32`) | `next1D`, `next2D` |
| Primitive | `SceneObject` | shape, material and optional light ids |
| Texture | `Texture` variant: `ConstantTexture`, `CheckerTexture` | `evaluate(hit)` |
| Affine map | `Transform` (with its inverse) | `translation`, `scaling`, `rotation`, `*`, `inverse`, `apply` |
| Shared mesh | `MeshGeometry` (triangles + BVH), placed by `MeshInstance` | `intersect`, `bounds` |

## Steps

- [x] 0. Build skeleton: root CMake, `albedo_core`, `albedo` app, GoogleTest, warnings as errors, Debug ASan+UBSan, `.clang-format`, `.clang-tidy`, one trivial test. Linux and MSVC compatible. *(completed 2026-10-21)*
- [x] 1. Core math: generic `Vector3`, `Point3`, `Normal3`, `Point2`, `Bounds3` (templated on the scalar type), `Ray`, `Interval`, `OrthonormalBasis` *(completed 2026-10-21)*
- [x] 2. `Rgb`, `PixelBuffer`, `writePpm`, producing a gradient image *(completed 2026-10-21)*
- [x] 3. `PerspectiveCamera`, `Sphere`, `Scene`, `NormalsEstimator`, producing the first rendered image (`scenes/step-03-normals.png`) *(completed 2026-10-21)*
- [x] 4. `Pcg32`, `SampleGenerator`, sample warps, `LambertLobe`, `SkyLight`, `ScatterOnlyEstimator`; white furnace test; jittered sampling, producing `scenes/step-04-diffuse.png` *(completed 2026-10-22)*
- [x] 5. Parallel tile renderer (`std::jthread`); images identical across thread counts *(completed 2026-10-22)*
- [x] 6. `ConductorLobe` (GGX) and `DielectricLobe`, producing `scenes/step-06-metal-glass.png` *(completed 2026-10-22)*
- [x] 7. `PointLight`, `AreaLight`, `PathEstimator` (next-event estimation, MIS, Russian roulette), producing `scenes/step-07-path-tracing.png` *(completed 2026-10-22)*
- [x] 8. `Triangle`, `TriangleMesh`, BVH (SAH build), OBJ loader *(security-relevant: input parsing)*, producing `scenes/step-08-meshes.png` *(completed 2026-10-25)*
- [x] 9. Textures and `Transform`-based instancing, producing `scenes/step-09-textures-instancing.png` *(completed 2026-10-27)*
- [ ] 10. Text scene format and parser *(security-relevant: input parsing)*

Later options: better samplers, reconstruction filters, environment maps, PNG output, volumes.
