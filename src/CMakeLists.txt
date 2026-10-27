add_library(albedo_core STATIC)
target_sources(albedo_core
    PRIVATE
        albedo/accel/bvh.cpp
        albedo/camera/perspective_camera.cpp
        albedo/color/srgb.cpp
        albedo/core/orthonormal_basis.cpp
        albedo/core/transform.cpp
        albedo/estimator/normals_estimator.cpp
        albedo/estimator/path_estimator.cpp
        albedo/estimator/scatter_only_estimator.cpp
        albedo/geometry/mesh_geometry.cpp
        albedo/geometry/mesh_instance.cpp
        albedo/geometry/obj.cpp
        albedo/geometry/sphere.cpp
        albedo/geometry/triangle.cpp
        albedo/geometry/triangle_mesh.cpp
        albedo/image/pixel_buffer.cpp
        albedo/image/ppm.cpp
        albedo/io/line_reader.cpp
        albedo/io/path_text.cpp
        albedo/io/tokens.cpp
        albedo/light/area_light.cpp
        albedo/light/point_light.cpp
        albedo/light/sky_light.cpp
        albedo/render/render_image.cpp
        albedo/render/tile.cpp
        albedo/sampling/warps.cpp
        albedo/scattering/conductor_lobe.cpp
        albedo/scattering/dielectric_lobe.cpp
        albedo/scattering/fresnel.cpp
        albedo/scattering/ggx_distribution.cpp
        albedo/scattering/lambert_lobe.cpp
        albedo/scattering/scattering.cpp
        albedo/scattering/specular.cpp
        albedo/scene/scene.cpp
        albedo/scene/scene_file.cpp
        albedo/texture/checker_texture.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
        FILES
            albedo/accel/bvh.hpp
            albedo/camera/perspective_camera.hpp
            albedo/camera/pixel_sample.hpp
            albedo/color/rgb.hpp
            albedo/color/srgb.hpp
            albedo/core/arithmetic.hpp
            albedo/core/bounds3.hpp
            albedo/core/interval.hpp
            albedo/core/normal3.hpp
            albedo/core/orthonormal_basis.hpp
            albedo/core/point2.hpp
            albedo/core/point3.hpp
            albedo/core/ray.hpp
            albedo/core/real.hpp
            albedo/core/transform.hpp
            albedo/core/vector3.hpp
            albedo/estimator/normals_estimator.hpp
            albedo/estimator/path_estimator.hpp
            albedo/estimator/radiance_estimator.hpp
            albedo/estimator/scatter_only_estimator.hpp
            albedo/estimator/surface_ray.hpp
            albedo/geometry/mesh_geometry.hpp
            albedo/geometry/mesh_instance.hpp
            albedo/geometry/obj.hpp
            albedo/geometry/shape.hpp
            albedo/geometry/shape_hit.hpp
            albedo/geometry/sphere.hpp
            albedo/geometry/triangle.hpp
            albedo/geometry/triangle_mesh.hpp
            albedo/image/pixel_buffer.hpp
            albedo/image/ppm.hpp
            albedo/io/line_reader.hpp
            albedo/io/path_text.hpp
            albedo/io/tokens.hpp
            albedo/light/area_light.hpp
            albedo/light/light.hpp
            albedo/light/light_sample.hpp
            albedo/light/point_light.hpp
            albedo/light/sky_light.hpp
            albedo/material/conductor_material.hpp
            albedo/material/dielectric_material.hpp
            albedo/material/diffuse_material.hpp
            albedo/material/material.hpp
            albedo/render/render_image.hpp
            albedo/render/tile.hpp
            albedo/sampling/pcg32.hpp
            albedo/sampling/sample_generator.hpp
            albedo/sampling/warps.hpp
            albedo/scattering/conductor_lobe.hpp
            albedo/scattering/dielectric_lobe.hpp
            albedo/scattering/fresnel.hpp
            albedo/scattering/ggx_distribution.hpp
            albedo/scattering/lambert_lobe.hpp
            albedo/scattering/lobe.hpp
            albedo/scattering/scatter_sample.hpp
            albedo/scattering/scattering.hpp
            albedo/scattering/specular.hpp
            albedo/scene/hit_record.hpp
            albedo/scene/scene.hpp
            albedo/scene/scene_file.hpp
            albedo/scene/scene_object.hpp
            albedo/texture/checker_texture.hpp
            albedo/texture/constant_texture.hpp
            albedo/texture/texture.hpp
)
target_compile_features(albedo_core PUBLIC cxx_std_20)
# The watertight triangle test needs each product in its edge functions rounded
# separately; fusing them into an FMA breaks the exact antisymmetry between
# triangles sharing an edge. Transformed bounds rely on rounding exactly as a
# transformed point does, which contraction in only one of them would break.
# MSVC's default /fp:precise does not contract.
set_source_files_properties(albedo/geometry/triangle.cpp albedo/core/transform.cpp PROPERTIES
    COMPILE_OPTIONS "$<$<CXX_COMPILER_ID:GNU,Clang,AppleClang>:-ffp-contract=off>")
find_package(Threads REQUIRED)
target_link_libraries(albedo_core PRIVATE Threads::Threads)
albedo_set_warnings(albedo_core)
albedo_enable_sanitizers(albedo_core)
