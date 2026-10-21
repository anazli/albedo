cmake_minimum_required(VERSION 3.24)

project(albedo LANGUAGES CXX)

set(CMAKE_CXX_EXTENSIONS OFF)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(warnings)
include(sanitizers)

add_subdirectory(src)
add_subdirectory(apps)

option(ALBEDO_BUILD_TESTS "Build the unit tests" ON)
if(ALBEDO_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
