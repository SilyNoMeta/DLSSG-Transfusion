cmake_minimum_required(VERSION 3.24)
project(VulkanNvxTests LANGUAGES CXX ASM_MASM)
set(NATIVE "${CMAKE_CURRENT_SOURCE_DIR}/../../source/native")
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_executable(VulkanNvxTests main.cpp "${NATIVE}/vulkan_nvx_thunks.asm")
target_compile_features(VulkanNvxTests PRIVATE cxx_std_20)
target_compile_definitions(VulkanNvxTests PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)

enable_testing()
add_test(NAME vulkan_nvx_transport COMMAND VulkanNvxTests)
