# Header-only for now; becomes a STATIC library with the first .cpp file.
add_library(rt_core INTERFACE)
target_sources(rt_core INTERFACE
    FILE_SET HEADERS
    BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR}
    FILES
        rt/core/real.hpp
)
target_compile_features(rt_core INTERFACE cxx_std_20)
