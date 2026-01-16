# profiling/CMakeLists.txt
cmake_minimum_required(VERSION 3.15)

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
        ${CMAKE_SOURCE_DIR}/plugin/Source/TimbreSpace/TimbreSpaceTriangulation.h
        ${CMAKE_SOURCE_DIR}/plugin/Source/TimbreSpace/TimbreSpacePointSelector.h
        ${CMAKE_SOURCE_DIR}/plugin/Source/TimbreSpace/TimbrePointTypes.h
)

target_include_directories(tsn-profile-triangulation PRIVATE
        ${CMAKE_SOURCE_DIR}/plugin/Source
        ${CMAKE_SOURCE_DIR}/plugin/delaunator-cpp/include
        ${CMAKE_SOURCE_DIR}/plugin/slicer_granular/nvs_libraries
        ${CMAKE_SOURCE_DIR}/plugin/slicer_granular/JUCE/modules
        ${CMAKE_BINARY_DIR}/plugin/tsn-granular_artefacts/JuceLibraryCode  # Add generated JUCE header location
        ${CMAKE_SOURCE_DIR}/essentia/src
        ${CMAKE_BINARY_DIR}/essentia_install/include
        ${CMAKE_BINARY_DIR}/essentia_install/include/essentia
)

target_link_libraries(tsn-profile-triangulation PRIVATE
        Eigen3::Eigen
        fmt::fmt
        essentia_built
        juce::juce_core
        juce::juce_data_structures
        juce::juce_events
)

target_compile_definitions(tsn-profile-triangulation PRIVATE
        JUCE_STRICT_REFCOUNTEDPOINTER=1
        _LIBCPP_ENABLE_CXX20_REMOVED_TYPE_TRAITS=1
        JUCE_STANDALONE_APPLICATION=1
        FMT_HEADER_ONLY=1
)

target_compile_features(tsn-profile-triangulation PRIVATE cxx_std_20)

# Enable optimization even in debug builds for profiling
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_options(tsn-profile-triangulation PRIVATE -O2 -g)
else()
    target_compile_options(tsn-profile-triangulation PRIVATE -O3 -g)
endif()

# Optional: macOS-specific profiling flags
if(APPLE)
    target_compile_options(tsn-profile-triangulation PRIVATE
            -fno-omit-frame-pointer
    )
endif()