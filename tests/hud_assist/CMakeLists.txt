cmake_minimum_required(VERSION 3.24)
project(HudAssistTests LANGUAGES CXX)

set(STREAMLINE_ROOT "" CACHE PATH "Path to the NVIDIA Streamline SDK")
if(NOT EXISTS "${STREAMLINE_ROOT}/include/sl.h")
    message(FATAL_ERROR "Set STREAMLINE_ROOT to the NVIDIA Streamline SDK")
endif()
set(NATIVE "${CMAKE_CURRENT_SOURCE_DIR}/../../source/native")

# GPU test on a real D3D12 device: replays the frame structure recorded in
# Crimson Desert through the capture and synthesis hooks.
add_executable(HudAssistTests main.cpp)
target_compile_features(HudAssistTests PRIVATE cxx_std_20)
target_compile_definitions(HudAssistTests PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
target_include_directories(HudAssistTests PRIVATE "${NATIVE}" "${STREAMLINE_ROOT}/include")
target_link_libraries(HudAssistTests PRIVATE d3d12 "${NATIVE}/detours/detours.lib")
