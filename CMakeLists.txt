cmake_minimum_required(VERSION 3.22)
project(resilio_versio VERSION 0.0.1 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_OSX_DEPLOYMENT_TARGET "12.0" CACHE STRING "")

option(RV_BUILD_PLUGIN "Build the JUCE AU/VST3 Plugin (needs libs/JUCE)" ON)

# Core: platform-independent DSP + ParamSpec. No libDaisy, no JUCE.
file(GLOB RV_CORE_SOURCES CONFIGURE_DEPENDS core/dsp/*.cpp)
add_library(rv_core STATIC ${RV_CORE_SOURCES} core/params/ParamSpec.h)
target_include_directories(rv_core PUBLIC core)
set_target_properties(rv_core PROPERTIES LINKER_LANGUAGE CXX POSITION_INDEPENDENT_CODE ON)
if(NOT MSVC)
    target_compile_options(rv_core PRIVATE -Wall -Wextra)
endif()

# Desktop host helpers (WAV, JSON, FFT, metrics). Header-only or host/common/*.cpp.
file(GLOB RV_HOST_COMMON_SOURCES CONFIGURE_DEPENDS host/common/*.cpp)
add_library(rv_host_common STATIC ${RV_HOST_COMMON_SOURCES} host/common/Wav.h)
target_include_directories(rv_host_common PUBLIC host/common)
target_link_libraries(rv_host_common PUBLIC rv_core)
set_target_properties(rv_host_common PROPERTIES LINKER_LANGUAGE CXX)

file(GLOB RV_RENDER_SOURCES CONFIGURE_DEPENDS host/render/*.cpp)
add_executable(rv_render ${RV_RENDER_SOURCES})
target_link_libraries(rv_render PRIVATE rv_host_common)

# Every host/tests/test_*.cpp is its own test executable with its own main().
enable_testing()
file(GLOB RV_TEST_SOURCES CONFIGURE_DEPENDS host/tests/test_*.cpp)
foreach(src ${RV_TEST_SOURCES})
    get_filename_component(name ${src} NAME_WE)
    add_executable(rv_${name} ${src})
    target_link_libraries(rv_${name} PRIVATE rv_host_common)
    add_test(NAME ${name} COMMAND rv_${name} WORKING_DIRECTORY ${CMAKE_BINARY_DIR})
endforeach()

if(RV_BUILD_PLUGIN)
    if(EXISTS ${CMAKE_SOURCE_DIR}/libs/JUCE/CMakeLists.txt)
        add_subdirectory(libs/JUCE EXCLUDE_FROM_ALL)
        add_subdirectory(plugin)
    else()
        message(WARNING "libs/JUCE not found: skipping Plugin. Run: git submodule update --init libs/JUCE")
    endif()
endif()
