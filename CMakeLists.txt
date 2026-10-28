cmake_minimum_required(VERSION 3.25)

file(STRINGS "${CMAKE_CURRENT_SOURCE_DIR}/include/dsa/config.hpp" LINES REGEX "#define[ \t]+DSA_VERSION_")

foreach(part IN ITEMS MAJOR MINOR PATCH)
    # ${part} is MAJOR, then MINOR, then PATCH
    string(REGEX MATCH "#define[ \t]+DSA_VERSION_${part}[ \t]+([0-9]+)" _ "${LINES}")
    set(DSA_VERSION_${part} "${CMAKE_MATCH_1}")
    if(NOT "${DSA_VERSION_${part}}" MATCHES "^[0-9]+$")
        message(FATAL_ERROR "Failed to extract DSA_VERSION_${part} value.")
    endif()
endforeach()

project(dsa 
        VERSION "${DSA_VERSION_MAJOR}.${DSA_VERSION_MINOR}.${DSA_VERSION_PATCH}"
        DESCRIPTION "DSA Library" 
        LANGUAGES CXX)

# library and alias
add_library(dsa INTERFACE)
add_library(dsa::dsa ALIAS dsa)

# include directories
target_include_directories(dsa INTERFACE 
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:include>)

target_compile_features(dsa INTERFACE cxx_std_20)

# Developer-only setup: skipped when dsa is pulled in via add_subdirectory /
# FetchContent, so consumers get no extra options, targets or flags.
if(PROJECT_IS_TOP_LEVEL)
    set(CMAKE_CXX_EXTENSIONS OFF)   # -std=c++20, not -std=gnu++20
    include(cmake/Warnings.cmake)
    include(cmake/Sanitizers.cmake)
endif()
