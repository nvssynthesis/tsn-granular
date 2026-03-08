cmake_minimum_required(VERSION 3.15)

project(tsn-granular-root VERSION 0.6.0)

option(USE_SYSTEM_LIBRARIES "Use system-installed libraries instead of fetching" OFF)
option(BUILD_PROFILING_TOOLS "Build profiling executables" OFF)

if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "13.0" CACHE STRING "Minimum macOS deployment version")
endif()

add_subdirectory(plugin)

if(BUILD_PROFILING_TOOLS)
    add_subdirectory(profiling)
endif()

message(STATUS "TSN Granular Build Configuration:")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  Use system libraries: ${USE_SYSTEM_LIBRARIES}")
message(STATUS "  Build profiling tools: ${BUILD_PROFILING_TOOLS}")
