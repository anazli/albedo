add_executable(rt_render rt_render/main.cpp)
target_link_libraries(rt_render PRIVATE rt_core)
rt_set_warnings(rt_render)
rt_enable_sanitizers(rt_render)
