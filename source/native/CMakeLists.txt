cmake_minimum_required(VERSION 3.24)
project(DLSSGTransfusionNative LANGUAGES C CXX ASM_MASM RC)

set(SCATTER_EXPERIMENT "0" CACHE STRING "Scatter comparison: 0 baseline, 1 stricter consistency, 2 smaller spread, 3 center depth bias, 4 fixed 1080p working resolution, 5 relaxed 25% agreement, 6 geometric warp, 7 pure warp")
if(NOT SCATTER_EXPERIMENT MATCHES "^[01234567]$")
    message(FATAL_ERROR "SCATTER_EXPERIMENT must be 0, 1, 2, 3, 4, 5, 6, or 7")
endif()
set(STREAMLINE_ROOT "" CACHE PATH "Path to the NVIDIA Streamline SDK directory")
if(NOT EXISTS "${STREAMLINE_ROOT}/include/sl_dlss_g.h")
    message(FATAL_ERROR "Set STREAMLINE_ROOT to a Streamline SDK directory containing include/sl_dlss_g.h")
endif()

# Statically link C/C++ runtime (/MT) so the mod has zero external MSVC runtime dependency
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

set(COMMON_SOURCES
    crash_diagnostics.cpp
    patcher.cpp
    midpoint_fix.cpp
    dlssg_provider_policy.cpp
    proxy.cpp
    proxy_thunks.asm
)

set(DETOURS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/detours")

function(setup_mod_target TARGET_NAME OUTPUT_BASENAME DEF_FILE EXTENSION)
    set(RC_INTERNAL_NAME "${OUTPUT_BASENAME}${EXTENSION}")
    set(RC_ORIGINAL_FILENAME "${OUTPUT_BASENAME}${EXTENSION}")
    set(RC_FILE "${CMAKE_CURRENT_BINARY_DIR}/${TARGET_NAME}.rc")
    configure_file("${CMAKE_CURRENT_SOURCE_DIR}/transfusion.rc.in" "${RC_FILE}" @ONLY)

    add_library(${TARGET_NAME} SHARED ${COMMON_SOURCES} "${RC_FILE}")
    target_compile_features(${TARGET_NAME} PRIVATE cxx_std_20)
    target_compile_options(${TARGET_NAME} PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:/Zi>")
    target_link_options(${TARGET_NAME} PRIVATE /DEBUG:FULL /OPT:REF /OPT:ICF)
    target_compile_definitions(${TARGET_NAME} PRIVATE SCATTER_EXPERIMENT=${SCATTER_EXPERIMENT} UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_include_directories(${TARGET_NAME} PRIVATE "${STREAMLINE_ROOT}/include" "${DETOURS_DIR}")
    target_link_libraries(${TARGET_NAME} PRIVATE bcrypt version user32 "${DETOURS_DIR}/detours.lib")
    if(DEF_FILE)
        target_link_options(${TARGET_NAME} PRIVATE "/DEF:${CMAKE_CURRENT_SOURCE_DIR}/${DEF_FILE}")
    endif()
    set_target_properties(${TARGET_NAME} PROPERTIES
        PREFIX ""
        SUFFIX "${EXTENSION}"
        OUTPUT_NAME "${OUTPUT_BASENAME}"
    )
    add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/dist"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:${TARGET_NAME}>" "${CMAKE_BINARY_DIR}/dist/${OUTPUT_BASENAME}${EXTENSION}"
        COMMENT "Copying ${OUTPUT_BASENAME}${EXTENSION} to dist/"
    )
endfunction()

# 1. version.dll proxy target
setup_mod_target(RTX40MFG_version "version" "version.def" ".dll")

# 2. winmm.dll proxy target
setup_mod_target(RTX40MFG_winmm "winmm" "winmm.def" ".dll")

# 3. dxgi.dll proxy target
setup_mod_target(RTX40MFG_dxgi "dxgi" "dxgi.def" ".dll")

# 4. dinput8.dll proxy target
setup_mod_target(RTX40MFG_dinput8 "dinput8" "dinput8.def" ".dll")

# 5. Standard ASI target (Ultimate ASI Loader / CET / RenoDX)
setup_mod_target(DLSSGTransfusionAuto "DLSSG-Transfusion" "" ".asi")

# Also copy DLSSG-Transfusion.asi to DLSSG-Transfusion.dll and backwards-compatible aliases in dist
add_custom_command(TARGET DLSSGTransfusionAuto POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:DLSSGTransfusionAuto>" "${CMAKE_BINARY_DIR}/dist/DLSSG-Transfusion.dll"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:DLSSGTransfusionAuto>" "${CMAKE_BINARY_DIR}/dist/RTX40MFG.asi"
    COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:DLSSGTransfusionAuto>" "${CMAKE_BINARY_DIR}/dist/RTX40MFG.dll"
    COMMENT "Copying DLSSG-Transfusion.dll and aliases to dist/"
)
