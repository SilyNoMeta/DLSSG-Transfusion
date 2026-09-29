cmake_minimum_required(VERSION 3.24)
project(DLSSGTransfusionNative LANGUAGES C CXX ASM_MASM RC)

# Release identity: DLL version resources and the first log lines.
set(TRANSFUSION_VERSION "1.4.5.2-rtx20-30-40")
set(TRANSFUSION_VERSION_NUMERIC "1,4,5,2")
set(TRANSFUSION_COMPANY "SilyNoMeta")
set(TRANSFUSION_DESCRIPTION "DLSSG-Transfusion for RTX 20, 30 and 40")

set(SCATTER_EXPERIMENT "0" CACHE STRING "Scatter comparison: 0 baseline, 1 stricter consistency, 2 smaller spread, 3 center depth bias, 4 fixed 1080p working resolution, 5 relaxed 25% agreement, 6 geometric warp, 7 pure warp, 8 shadow scale veto, 9 siren chroma 0.25 with shadow veto")
if(NOT SCATTER_EXPERIMENT MATCHES "^[0123456789]$")
    message(FATAL_ERROR "SCATTER_EXPERIMENT must be 0, 1, 2, 3, 4, 5, 6, 7, 8, or 9")
endif()
set(QUALITY_DIAGNOSTIC "0" CACHE STRING "RGB source isolation: 0 off, 1 source 0, 2 source 1")
if(NOT QUALITY_DIAGNOSTIC MATCHES "^[012]$")
    message(FATAL_ERROR "QUALITY_DIAGNOSTIC must be 0, 1, or 2")
endif()
if(NOT QUALITY_DIAGNOSTIC STREQUAL "0" AND NOT SCATTER_EXPERIMENT STREQUAL "9")
    message(FATAL_ERROR "Source diagnostics require SCATTER_EXPERIMENT=9")
endif()
option(QUALITY_CAPTURE "Build opt-in GPU candidate capture" OFF)
if(QUALITY_CAPTURE)
    if(NOT SCATTER_EXPERIMENT STREQUAL "9" OR NOT QUALITY_DIAGNOSTIC STREQUAL "0")
        message(FATAL_ERROR "Capture requires mode 9 and source diagnostics off")
    endif()
    if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../build-capture/capture_binary.generated.h")
        message(FATAL_ERROR "Run scripts/build_capture_probe.py first")
    endif()
    add_compile_definitions(QUALITY_CAPTURE=1 QUALITY_CAPTURE_DIRECTORY=L"${CMAKE_CURRENT_SOURCE_DIR}/../../capture-output")
    include_directories("${CMAKE_CURRENT_SOURCE_DIR}/../../build-capture")
endif()
set(STREAMLINE_ROOT "" CACHE PATH "Path to the NVIDIA Streamline SDK directory")
if(NOT EXISTS "${STREAMLINE_ROOT}/include/sl_dlss_g.h")
    message(FATAL_ERROR "Set STREAMLINE_ROOT to a Streamline SDK directory containing include/sl_dlss_g.h")
endif()
# NGX SDK headers (DLSS Super Resolution render-scale override). The Streamline
# SDK ships them under external/ngx-sdk/include; they are not redistributed here.
set(NGX_INCLUDE_DIR "${STREAMLINE_ROOT}/external/ngx-sdk/include" CACHE PATH "Directory containing nvsdk_ngx_params.h")
if(NOT EXISTS "${NGX_INCLUDE_DIR}/nvsdk_ngx_params.h")
    message(FATAL_ERROR "nvsdk_ngx_params.h not found in NGX_INCLUDE_DIR=${NGX_INCLUDE_DIR}; set NGX_INCLUDE_DIR to the NGX SDK include directory (Streamline SDK: external/ngx-sdk/include)")
endif()

# Statically link C/C++ runtime (/MT) so the mod has zero external MSVC runtime dependency
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

set(COMMON_SOURCES
    crash_diagnostics.cpp
    patcher.cpp
    midpoint_fix.cpp
    dlssg_provider_policy.cpp
    multiplier_overlay.cpp
    proxy.cpp
    proxy_thunks.asm
    vulkan_nvx_thunks.asm
    gpu_arch.cpp
    smooth_motion_sm86.cpp
)

add_executable(smooth_motion_fatbin_test "${CMAKE_CURRENT_SOURCE_DIR}/../../tests/smooth_motion_fatbin_test.cpp")
target_compile_features(smooth_motion_fatbin_test PRIVATE cxx_std_20)
target_include_directories(smooth_motion_fatbin_test PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

add_executable(smooth_motion_sm86_probe "${CMAKE_CURRENT_SOURCE_DIR}/../../tests/smooth_motion_sm86_probe.cpp"
    smooth_motion_sm86.cpp)
target_compile_features(smooth_motion_sm86_probe PRIVATE cxx_std_20)
target_link_libraries(smooth_motion_sm86_probe PRIVATE bcrypt)

add_executable(smooth_motion_vulkan_gate_probe
    "${CMAKE_CURRENT_SOURCE_DIR}/../../tests/smooth_motion_vulkan_gate_probe.cpp"
    smooth_motion_sm86.cpp)
target_compile_features(smooth_motion_vulkan_gate_probe PRIVATE cxx_std_20)
target_include_directories(smooth_motion_vulkan_gate_probe PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_link_libraries(smooth_motion_vulkan_gate_probe PRIVATE bcrypt)

# Read-only Vulkan profile diagnostic. Point this at the official NVIDIA
# nvapi SDK headers; no NVAPI import library is needed (runtime resolution).
set(NVAPI_INCLUDE_DIR "" CACHE PATH "Optional: directory containing NVIDIA nvapi.h")
if(NVAPI_INCLUDE_DIR)
    if(NOT EXISTS "${NVAPI_INCLUDE_DIR}/nvapi.h")
        message(FATAL_ERROR "NVAPI_INCLUDE_DIR must contain nvapi.h")
    endif()
    add_executable(smooth_motion_drs_probe "${CMAKE_CURRENT_SOURCE_DIR}/../../tests/smooth_motion_drs_probe.cpp")
    target_compile_features(smooth_motion_drs_probe PRIVATE cxx_std_20)
    target_include_directories(smooth_motion_drs_probe PRIVATE "${NVAPI_INCLUDE_DIR}")
endif()

# DL1/DL2 network kernels: not part of the public source tree
# (docs/PUBLIC-SOURCE.md). Without them the provider's network runs unchanged.
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/private/kernels.rc")
    list(APPEND COMMON_SOURCES private/kernels.rc)
else()
    message(STATUS "private/ not present: building without the optimized network and image kernels (docs/PUBLIC-SOURCE.md)")
endif()

set(DETOURS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/detours")

# The primary distribution binary follows OptiScaler's proven model: one PE
# contains the union of all named proxy exports and selects the backing system
# DLL from its filename at runtime. Exact-ordinal targets remain below as
# compatibility fallbacks for the uncommon ordinal-import case.
set(UNIVERSAL_DEF "${CMAKE_CURRENT_BINARY_DIR}/DLSSG-Transfusion-universal.def")
file(WRITE "${UNIVERSAL_DEF}" "LIBRARY DLSSG-Transfusion\nEXPORTS\n")
set(UNIVERSAL_EXPORTS)
foreach(PROXY_DEF version.def winmm.def dxgi.def dinput8.def)
    file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/${PROXY_DEF}" DEF_LINES)
    foreach(DEF_LINE IN LISTS DEF_LINES)
        string(STRIP "${DEF_LINE}" DEF_LINE)
        if(DEF_LINE STREQUAL "" OR DEF_LINE MATCHES "^(LIBRARY|EXPORTS)" OR DEF_LINE MATCHES "NONAME")
            continue()
        endif()
        string(REGEX REPLACE "[ \t]+@[0-9]+.*$" "" DEF_LINE "${DEF_LINE}")
        string(REGEX MATCH "^[^ =]+" EXPORT_NAME "${DEF_LINE}")
        list(FIND UNIVERSAL_EXPORTS "${EXPORT_NAME}" DUPLICATE_INDEX)
        if(NOT DUPLICATE_INDEX EQUAL -1)
            message(FATAL_ERROR "Duplicate universal proxy export: ${EXPORT_NAME}")
        endif()
        list(APPEND UNIVERSAL_EXPORTS "${EXPORT_NAME}")
        file(APPEND "${UNIVERSAL_DEF}" "    ${DEF_LINE}\n")
    endforeach()
endforeach()

function(setup_mod_target TARGET_NAME OUTPUT_BASENAME DEF_FILE EXTENSION)
    set(RC_INTERNAL_NAME "${OUTPUT_BASENAME}${EXTENSION}")
    set(RC_ORIGINAL_FILENAME "${OUTPUT_BASENAME}${EXTENSION}")
    set(RC_FILE "${CMAKE_CURRENT_BINARY_DIR}/${TARGET_NAME}.rc")
    configure_file("${CMAKE_CURRENT_SOURCE_DIR}/transfusion.rc.in" "${RC_FILE}" @ONLY)

    add_library(${TARGET_NAME} SHARED ${COMMON_SOURCES} "${RC_FILE}")
    target_compile_features(${TARGET_NAME} PRIVATE cxx_std_20)
    target_compile_options(${TARGET_NAME} PRIVATE "$<$<COMPILE_LANGUAGE:CXX>:/Zi>")
    target_link_options(${TARGET_NAME} PRIVATE /DEBUG:FULL /OPT:REF /OPT:ICF)
    target_compile_definitions(${TARGET_NAME} PRIVATE QUALITY_DIAGNOSTIC=${QUALITY_DIAGNOSTIC} SCATTER_EXPERIMENT=${SCATTER_EXPERIMENT} TRANSFUSION_VERSION="${TRANSFUSION_VERSION}" UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_include_directories(${TARGET_NAME} PRIVATE "${STREAMLINE_ROOT}/include" "${NGX_INCLUDE_DIR}" "${DETOURS_DIR}")
    target_link_libraries(${TARGET_NAME} PRIVATE bcrypt version user32 d3d12 dxgi d3dcompiler "${DETOURS_DIR}/detours.lib")
    if(DEF_FILE)
        if(IS_ABSOLUTE "${DEF_FILE}")
            target_link_options(${TARGET_NAME} PRIVATE "/DEF:${DEF_FILE}")
        else()
            target_link_options(${TARGET_NAME} PRIVATE "/DEF:${CMAKE_CURRENT_SOURCE_DIR}/${DEF_FILE}")
        endif()
    endif()
    set_target_properties(${TARGET_NAME} PROPERTIES
        PREFIX ""
        SUFFIX "${EXTENSION}"
        OUTPUT_NAME "${OUTPUT_BASENAME}"
        # The .dll and .asi share OUTPUT_BASENAME and both export the add-on API:
        # give each its own import library and .exp so parallel links never collide.
        ARCHIVE_OUTPUT_NAME "${TARGET_NAME}"
        PDB_NAME "${TARGET_NAME}"
        COMPILE_PDB_NAME "${TARGET_NAME}"
    )
    add_custom_command(TARGET ${TARGET_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/dist"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:${TARGET_NAME}>" "${CMAKE_BINARY_DIR}/dist/${OUTPUT_BASENAME}${EXTENSION}"
        COMMENT "Copying ${OUTPUT_BASENAME}${EXTENSION} to dist/"
    )
endfunction()

# Primary renameable distribution DLL (named exports).
setup_mod_target(Transfusion_universal "DLSSG-Transfusion" "${UNIVERSAL_DEF}" ".dll")

# Exact-export compatibility fallbacks.
# 1. version.dll proxy target
setup_mod_target(Transfusion_version "version" "version.def" ".dll")

# 2. winmm.dll proxy target
setup_mod_target(Transfusion_winmm "winmm" "winmm.def" ".dll")

# 3. dxgi.dll proxy target
setup_mod_target(Transfusion_dxgi "dxgi" "dxgi.def" ".dll")

# 4. dinput8.dll proxy target
setup_mod_target(Transfusion_dinput8 "dinput8" "dinput8.def" ".dll")

# 5. Standard ASI target (Ultimate ASI Loader / CET / RenoDX)
setup_mod_target(DLSSGTransfusionAuto "DLSSG-Transfusion" "" ".asi")

# 6. Optional ReShade add-on (settings panel). Built only when RESHADE_ROOT
# points to a ReShade source tree (v6.8.0) with its deps/imgui submodule.
set(RESHADE_ROOT "" CACHE PATH "Optional: ReShade source directory (include/reshade.hpp and deps/imgui) for the settings add-on")
if(RESHADE_ROOT)
    if(NOT EXISTS "${RESHADE_ROOT}/include/reshade.hpp" OR NOT EXISTS "${RESHADE_ROOT}/deps/imgui/imgui.h")
        message(FATAL_ERROR "RESHADE_ROOT must contain include/reshade.hpp and deps/imgui/imgui.h (git submodule update --init deps/imgui)")
    endif()
    add_library(Transfusion_addon SHARED "${CMAKE_CURRENT_SOURCE_DIR}/../addon/addon.cpp")
    target_compile_features(Transfusion_addon PRIVATE cxx_std_20)
    target_include_directories(Transfusion_addon PRIVATE "${RESHADE_ROOT}")
    target_link_libraries(Transfusion_addon PRIVATE psapi)
    set_target_properties(Transfusion_addon PROPERTIES PREFIX "" SUFFIX ".addon64" OUTPUT_NAME "DLSSG-Transfusion"
        ARCHIVE_OUTPUT_NAME "Transfusion_addon" PDB_NAME "Transfusion_addon" COMPILE_PDB_NAME "Transfusion_addon")
    add_custom_command(TARGET Transfusion_addon POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/dist"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:Transfusion_addon>" "${CMAKE_BINARY_DIR}/dist/DLSSG-Transfusion.addon64"
        COMMENT "Copying DLSSG-Transfusion.addon64 to dist/"
    )
endif()

# Clean dist output
