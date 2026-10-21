include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.15.2
)
# Match the CRT of the project on MSVC instead of GoogleTest's static default.
set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
FetchContent_MakeAvailable(googletest)

add_executable(rt_tests
    core/real_test.cpp
)
target_link_libraries(rt_tests PRIVATE rt_core GTest::gtest_main)
rt_set_warnings(rt_tests)
rt_enable_sanitizers(rt_tests)

include(GoogleTest)
gtest_discover_tests(rt_tests)
