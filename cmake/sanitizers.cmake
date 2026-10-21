option(RT_ENABLE_SANITIZERS "Build Debug configurations with AddressSanitizer and UBSan" ON)

# Enables ASan + UBSan on a target for Debug builds. MSVC only supports ASan,
# so UBSan is omitted there.
function(rt_enable_sanitizers target)
    if(NOT RT_ENABLE_SANITIZERS)
        return()
    endif()

    if(MSVC)
        target_compile_options(${target} PRIVATE $<$<CONFIG:Debug>:/fsanitize=address>)
    else()
        set(flags -fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=all)
        target_compile_options(${target} PRIVATE $<$<CONFIG:Debug>:${flags}>)
        target_link_options(${target} PRIVATE $<$<CONFIG:Debug>:${flags}>)
    endif()
endfunction()
