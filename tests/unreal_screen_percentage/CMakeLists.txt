cmake_minimum_required(VERSION 3.24)
project(UnrealScreenPercentage LANGUAGES CXX)

# Windows test: a mock of Unreal's r.ScreenPercentage declaration, in both code
# shapes, checked against source/native/unreal_screen_percentage.h. Build it
# twice: default (inlined constructor) and with -DOUT_OF_LINE=1.
add_executable(UnrealScreenPercentage main.cpp)
target_compile_features(UnrealScreenPercentage PRIVATE cxx_std_20)
option(OUT_OF_LINE "Constructor called with the static object" OFF)
if(OUT_OF_LINE)
    target_compile_definitions(UnrealScreenPercentage PRIVATE OUT_OF_LINE)
endif()
