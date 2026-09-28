cmake_minimum_required(VERSION 3.24)
project(AddonConfig LANGUAGES CXX)

add_executable(AddonConfig main.cpp)
target_compile_features(AddonConfig PRIVATE cxx_std_20)
