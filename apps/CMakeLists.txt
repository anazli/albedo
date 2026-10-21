add_executable(albedo albedo/main.cpp)
target_link_libraries(albedo PRIVATE albedo_core)
albedo_set_warnings(albedo)
albedo_enable_sanitizers(albedo)
