cmake_minimum_required(VERSION 3.15)

project(tsn-granular-root VERSION 0.5.0)

# Global options
option(USE_SYSTEM_LIBRARIES "Use system-installed libraries instead of fetching" OFF)

# Set up global compile options
if(APPLE)
    set(CMAKE_OSX_DEPLOYMENT_TARGET "13.0" CACHE STRING "Minimum macOS deployment version")
endif()

# Add the plugin subdirectory
add_subdirectory(plugin)

# Print build configuration
message(STATUS "TSN Granular Build Configuration:")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  Use system libraries: ${USE_SYSTEM_LIBRARIES}")