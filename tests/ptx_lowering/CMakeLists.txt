cmake_minimum_required(VERSION 3.24)
project(PtxLowering LANGUAGES CXX)

add_executable(PtxLowering main.cpp)
target_compile_features(PtxLowering PRIVATE cxx_std_20)
