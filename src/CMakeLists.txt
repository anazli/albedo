add_library(albedo_core STATIC)
target_sources(albedo_core
    PRIVATE
        albedo/camera/perspective_camera.cpp
        albedo/color/srgb.cpp
        albedo/core/orthonormal_basis.cpp
        albedo/estimator/normals_estimator.cpp
        albedo/geometry/sphere.cpp
        albedo/image/pixel_buffer.cpp
        albedo/image/ppm.cpp
        albedo/render/render_image.cpp
        albedo/scene/scene.cpp
    PUBLIC
        FILE_SET HEADERS
        BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
        FILES
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
            albedo/core/vector3.hpp
            albedo/estimator/normals_estimator.hpp
            albedo/geometry/shape.hpp
            albedo/geometry/shape_hit.hpp
            albedo/geometry/sphere.hpp
            albedo/image/pixel_buffer.hpp
            albedo/image/ppm.hpp
            albedo/render/render_image.hpp
            albedo/scene/hit_record.hpp
            albedo/scene/scene.hpp
)
target_compile_features(albedo_core PUBLIC cxx_std_20)
albedo_set_warnings(albedo_core)
albedo_enable_sanitizers(albedo_core)
