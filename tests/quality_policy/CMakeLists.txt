cmake_minimum_required(VERSION 3.24)
project(QualityPolicy LANGUAGES CXX)

add_executable(QualityPolicy main.cpp)
target_compile_features(QualityPolicy PRIVATE cxx_std_20)
