cmake_minimum_required(VERSION 3.24)
project(DLSSGTransfusionNative LANGUAGES C CXX ASM_MASM RC)

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
)

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
    target_compile_definitions(${TARGET_NAME} PRIVATE QUALITY_DIAGNOSTIC=${QUALITY_DIAGNOSTIC} SCATTER_EXPERIMENT=${SCATTER_EXPERIMENT} UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
    target_include_directories(${TARGET_NAME} PRIVATE "${STREAMLINE_ROOT}/include" "${DETOURS_DIR}")
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

# Clean dist output
