# Header-only for now; becomes a STATIC library with the first .cpp file.
add_library(albedo_core INTERFACE)
target_sources(albedo_core INTERFACE
    FILE_SET HEADERS
    BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
    FILES
        albedo/core/arithmetic.hpp
        albedo/core/normal3.hpp
        albedo/core/point2.hpp
        albedo/core/point3.hpp
        albedo/core/real.hpp
        albedo/core/vector3.hpp
)
target_compile_features(albedo_core INTERFACE cxx_std_20)
