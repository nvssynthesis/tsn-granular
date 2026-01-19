# profiling/CMakeLists.txt
cmake_minimum_required(VERSION 3.15)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Find Eigen3 (should already be found by parent)
find_package(Eigen3 REQUIRED NO_MODULE)

# Get fmt (should already be available from parent)
if(NOT TARGET fmt::fmt)
    message(FATAL_ERROR "fmt library not found. Build the main plugin first.")
endif()

# Get Essentia (should already be built by parent)
if(NOT TARGET essentia_built)
    message(FATAL_ERROR "Essentia not found. Build the main plugin first.")
endif()

# fetch catch2 for testing
include(FetchContent)
FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.5.2  # or whatever version you want
        GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(Catch2)

# add catch2's cmake modules to the path for test discovery
list(APPEND CMAKE_MODULE_PATH ${catch2_SOURCE_DIR}/extras)

# Delaunator sources
set(DELAUNATOR_SOURCES
        ${CMAKE_SOURCE_DIR}/plugin/delaunator-cpp/src/delaunator.cpp
)

# Profiling executable for triangulation functions
add_executable(tsn-profile-triangulation
        profile_triangulation.cpp
        ${DELAUNATOR_SOURCES}
)

# Copy necessary source files that contain the functions
target_sources(tsn-profile-triangulation PRIVATE
        ${CMAKE_SOURCE_DIR}/plugin/Source/TimbreSpace/TimbreSpaceTriangulation.cpp
        ${CMAKE_SOURCE_DIR}/plugin/Source/TimbreSpace/TimbrePointTypes.cpp
)

target_include_directories(tsn-profile-triangulation PRIVATE
        ${CMAKE_SOURCE_DIR}/plugin/Source
        ${CMAKE_SOURCE_DIR}/plugin/delaunator-cpp/include
)

target_link_libraries(tsn-profile-triangulation PRIVATE
        Eigen3::Eigen
)

target_compile_definitions(tsn-profile-triangulation PRIVATE
        _LIBCPP_ENABLE_CXX20_REMOVED_TYPE_TRAITS=1
)

# Enable optimization even in debug builds for profiling
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(tsn-profile-triangulation PRIVATE -O0 -g)
else()
    target_compile_options(tsn-profile-triangulation PRIVATE -O3 -g)
endif()

# Optional: macOS-specific profiling flags
if(APPLE)
    target_compile_options(tsn-profile-triangulation PRIVATE
            -fno-omit-frame-pointer
    )
endif()

#======================================================================================
# test executable with catch2
add_executable(test-triangle-walk test_triangle_walk.cpp ${DELAUNATOR_SOURCES})

target_sources(test-triangle-walk PRIVATE
        ${CMAKE_SOURCE_DIR}/plugin/Source/TimbreSpace/TimbreSpaceTriangulation.cpp
        ${CMAKE_SOURCE_DIR}/plugin/Source/TimbreSpace/TimbrePointTypes.cpp
)

target_include_directories(test-triangle-walk PRIVATE
        ${CMAKE_SOURCE_DIR}/plugin/delaunator-cpp/include
        ${CMAKE_SOURCE_DIR}/plugin/Source
        ${CMAKE_SOURCE_DIR}/build/_deps/fmt-src/include
)
target_compile_definitions(test-triangle-walk
        PRIVATE
        FMT_HEADER_ONLY=1
)
target_link_libraries(test-triangle-walk PRIVATE
        fmt::fmt
        Eigen3::Eigen
        Catch2::Catch2WithMain
)

# enable ctest integration
include(CTest)
include(Catch)
catch_discover_tests(test-triangle-walk)