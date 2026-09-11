cmake_minimum_required(VERSION 3.24)
project(StartupHookHarness LANGUAGES CXX ASM_MASM)
set(STREAMLINE_ROOT "" CACHE PATH "Path to the NVIDIA Streamline SDK")
set(NATIVE "${CMAKE_CURRENT_SOURCE_DIR}/../../source/native")
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_library(StartupInterposer SHARED interposer.cpp)
target_compile_features(StartupInterposer PRIVATE cxx_std_20)
target_compile_definitions(StartupInterposer PRIVATE SL_INTERPOSER)
target_include_directories(StartupInterposer PRIVATE "${STREAMLINE_ROOT}/include")
set_target_properties(StartupInterposer PROPERTIES OUTPUT_NAME "sl.interposer")

add_executable(StartupHookHarness main.cpp "${NATIVE}/midpoint_fix.cpp"
    "${NATIVE}/crash_diagnostics.cpp"
    "${NATIVE}/dlssg_provider_policy.cpp" "${NATIVE}/proxy.cpp"
    "${NATIVE}/proxy_thunks.asm")
target_compile_features(StartupHookHarness PRIVATE cxx_std_20)
target_compile_definitions(StartupHookHarness PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
target_include_directories(StartupHookHarness PRIVATE "${STREAMLINE_ROOT}/include")
target_link_libraries(StartupHookHarness PRIVATE StartupInterposer bcrypt version user32
    "${NATIVE}/detours/detours.lib")

enable_testing()
add_executable(QualityPtxHarness quality_ptx.cpp)
target_compile_features(QualityPtxHarness PRIVATE cxx_std_20)
add_executable(QualityFatbinHarness quality_fatbin.cpp)
target_compile_features(QualityFatbinHarness PRIVATE cxx_std_20)
target_compile_definitions(QualityFatbinHarness PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
add_test(NAME quality_provider_fatbin COMMAND QualityFatbinHarness
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../DLSS DLL 310.9.0 + Streamline 2.14/nvngx_dlssg.dll"
    "${CMAKE_CURRENT_BINARY_DIR}/quality-patched.fatbin")
add_test(NAME quality_ptx_profile COMMAND QualityPtxHarness
    "${CMAKE_CURRENT_SOURCE_DIR}/../../../ptx_dumps/01_Kernel_BlendCandidatesFused_sm120.ptx"
    "${CMAKE_CURRENT_BINARY_DIR}/quality-patched.ptx")
add_executable(DiagnosticsHarness diagnostics.cpp "${NATIVE}/crash_diagnostics.cpp")
target_compile_features(DiagnosticsHarness PRIVATE cxx_std_20)
target_compile_definitions(DiagnosticsHarness PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
add_test(NAME exception_diagnostics COMMAND DiagnosticsHarness)
add_test(NAME detours_before_imports COMMAND StartupHookHarness detours-first)
add_test(NAME imports_before_detours COMMAND StartupHookHarness imports-first)
add_test(NAME concurrent_installers COMMAND StartupHookHarness concurrent)
add_test(NAME busy_transaction_retry COMMAND StartupHookHarness busy-transaction)
add_test(NAME small_stack_notifications COMMAND StartupHookHarness small-stack)
set_tests_properties(exception_diagnostics detours_before_imports imports_before_detours
    concurrent_installers busy_transaction_retry small_stack_notifications PROPERTIES TIMEOUT 15)
