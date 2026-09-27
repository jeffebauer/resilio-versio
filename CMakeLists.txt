cmake_minimum_required(VERSION 3.22)
project(resilio_versio VERSION 0.0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "")

option(RV_BUILD_PLUGIN "Build the JUCE AU/VST3 Plugin (needs libs/JUCE)" ON)

# Core: platform-independent DSP + ParamSpec. No libDaisy, no JUCE.
add_library(rv_core INTERFACE)
target_include_directories(rv_core INTERFACE core)

add_library(rv_host_common INTERFACE)
target_include_directories(rv_host_common INTERFACE host/common)

add_executable(rv_render host/render/main.cpp)
target_link_libraries(rv_render PRIVATE rv_core rv_host_common)

enable_testing()
add_executable(rv_tests host/tests/test_main.cpp)
target_link_libraries(rv_tests PRIVATE rv_core rv_host_common)
add_test(NAME rv_tests COMMAND rv_tests)

if(RV_BUILD_PLUGIN)
    if(EXISTS ${CMAKE_SOURCE_DIR}/libs/JUCE/CMakeLists.txt)
        add_subdirectory(libs/JUCE EXCLUDE_FROM_ALL)
        add_subdirectory(plugin)
    else()
        message(WARNING "libs/JUCE not found: skipping Plugin. Run: git submodule update --init libs/JUCE")
    endif()
endif()
