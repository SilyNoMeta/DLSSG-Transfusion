cmake_minimum_required(VERSION 3.24)
project(AdaptivePolicyTests LANGUAGES CXX)

add_executable(AdaptivePolicyTests main.cpp)
target_compile_features(AdaptivePolicyTests PRIVATE cxx_std_20)

enable_testing()
add_test(NAME adaptive_policy COMMAND AdaptivePolicyTests)
