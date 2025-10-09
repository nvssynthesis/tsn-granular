cmake_minimum_required(VERSION 3.15)

project(tsn-granular VERSION 0.4.1)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)
include(ExternalProject)

# Options for external dependencies
option(USE_SYSTEM_LIBRARIES "Use system-installed libraries instead of fetching" OFF)

# JUCE setup options - these will be passed to slicer_granular
set(JUCE_DIR "" CACHE PATH "Path to JUCE framework (leave empty to use bundled JUCE in slicer_granular/JUCE subdirectory)")
option(JUCE_FETCH_IF_MISSING "Automatically fetch JUCE if JUCE_DIR is not set and JUCE/ subdirectory doesn't exist" ON)
set(JUCE_VERSION "8.0.10" CACHE STRING "JUCE version to fetch if JUCE_FETCH_IF_MISSING is ON")

find_package(Eigen3 REQUIRED NO_MODULE)

# Set the Eigen include directory for use in your build
if(TARGET Eigen3::Eigen)
    get_target_property(EIGEN_INCLUDE_DIR Eigen3::Eigen INTERFACE_INCLUDE_DIRECTORIES)
    message(STATUS "Found Eigen3: ${EIGEN_INCLUDE_DIR}")
else()
    # Fallback if the target approach doesn't work
    find_path(EIGEN_INCLUDE_DIR
            NAMES Eigen/Dense
            PATHS
            /opt/homebrew/include/eigen3
            /usr/local/include/eigen3
            /usr/include/eigen3
            NO_DEFAULT_PATH
    )
    if(EIGEN_INCLUDE_DIR)
        message(STATUS "Found Eigen3 headers: ${EIGEN_INCLUDE_DIR}")
    else()
        message(FATAL_ERROR "Could not find Eigen3 headers")
    endif()
endif()

# Check if Essentia directory exists and has content, if not clone it
set(ESSENTIA_DIR "${CMAKE_CURRENT_SOURCE_DIR}/essentia")
set(ESSENTIA_INSTALL_DIR "${CMAKE_CURRENT_BINARY_DIR}/essentia_install")

if(NOT EXISTS "${ESSENTIA_DIR}/wscript")
    message(STATUS "Essentia not found or incomplete, cloning from GitHub...")

    # Remove existing directory if it exists but is incomplete
    if(EXISTS "${ESSENTIA_DIR}")
        message(STATUS "Removing incomplete Essentia directory...")
        file(REMOVE_RECURSE "${ESSENTIA_DIR}")
    endif()

    execute_process(
            COMMAND git clone --depth 1 https://github.com/MTG/essentia.git essentia
            WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
            RESULT_VARIABLE GIT_CLONE_RESULT
            OUTPUT_VARIABLE GIT_CLONE_OUTPUT
            ERROR_VARIABLE GIT_CLONE_ERROR
    )

    if(NOT GIT_CLONE_RESULT EQUAL 0)
        message(STATUS "Git clone output: ${GIT_CLONE_OUTPUT}")
        message(STATUS "Git clone error: ${GIT_CLONE_ERROR}")
        message(FATAL_ERROR "Failed to clone Essentia repository")
    endif()

    message(STATUS "Essentia cloned successfully")
else()
    message(STATUS "Found existing Essentia directory: ${ESSENTIA_DIR}")
endif()

# Build Essentia using its waf build system
message(STATUS "Building Essentia with waf...")

# Find Python executable
find_program(PYTHON3_EXECUTABLE
        NAMES python3.11 python3.10 python3.9 python3.8 python3
        DOC "Python 3 interpreter for building Essentia"
)

if(NOT PYTHON3_EXECUTABLE)
    message(FATAL_ERROR "Could not find Python 3 executable")
endif()

message(STATUS "Using Python: ${PYTHON3_EXECUTABLE}")

# Check Python version
execute_process(
        COMMAND ${PYTHON3_EXECUTABLE} -c "import sys; print(f'{sys.version_info.major}.{sys.version_info.minor}')"
        OUTPUT_VARIABLE PYTHON_VERSION
        OUTPUT_STRIP_TRAILING_WHITESPACE
)

message(STATUS "Python version: ${PYTHON_VERSION}")

# Configure Essentia with minimal dependencies
message(STATUS "Configuring Essentia...")

# Set up proper environment for macOS
if(APPLE)
    # Find the macOS SDK
    execute_process(
            COMMAND xcrun --show-sdk-path
            OUTPUT_VARIABLE MACOS_SDK_PATH
            OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    message(STATUS "macOS SDK path: ${MACOS_SDK_PATH}")

    set(ESSENTIA_ENV
            "SDKROOT=${MACOS_SDK_PATH}"
            "CPPFLAGS=-isysroot ${MACOS_SDK_PATH} -I${EIGEN_INCLUDE_DIR}"
            "CFLAGS=-isysroot ${MACOS_SDK_PATH}"
            "CXXFLAGS=-isysroot ${MACOS_SDK_PATH} -I${EIGEN_INCLUDE_DIR}"
            "LDFLAGS=-isysroot ${MACOS_SDK_PATH}"
    )
else()
    # For Linux, just add Eigen to the compiler flags
    set(ESSENTIA_ENV
            "CPPFLAGS=-I${EIGEN_INCLUDE_DIR}"
            "CXXFLAGS=-I${EIGEN_INCLUDE_DIR}"
    )
endif()
message(STATUS "Using Eigen from: ${EIGEN_INCLUDE_DIR}")

# Set parallel jobs - use a default if CMAKE_BUILD_PARALLEL_LEVEL is not set
if(NOT CMAKE_BUILD_PARALLEL_LEVEL)
    set(CMAKE_BUILD_PARALLEL_LEVEL 4)
endif()

# Use ExternalProject_Add to build Essentia properly
ExternalProject_Add(essentia_external
        SOURCE_DIR ${ESSENTIA_DIR}
        DOWNLOAD_COMMAND ""  # Already cloned above
        CONFIGURE_COMMAND ${CMAKE_COMMAND} -E env ${ESSENTIA_ENV}
        ${PYTHON3_EXECUTABLE} waf configure
        --build-static
        --prefix=${ESSENTIA_INSTALL_DIR}
        --lightweight=libsamplerate
        --fft=ACCELERATE
        --no-msse
        BUILD_COMMAND ${CMAKE_COMMAND} -E env ${ESSENTIA_ENV}
        ${PYTHON3_EXECUTABLE} waf -j ${CMAKE_BUILD_PARALLEL_LEVEL}
        INSTALL_COMMAND ${CMAKE_COMMAND} -E env ${ESSENTIA_ENV}
        ${PYTHON3_EXECUTABLE} waf install
        BUILD_IN_SOURCE 1
        BUILD_ALWAYS 0  # Only build when needed
        LOG_CONFIGURE ON
        LOG_BUILD ON
        LOG_INSTALL ON
)

# Set up Essentia variables for linking
set(ESSENTIA_INCLUDE_DIRS
        ${ESSENTIA_INSTALL_DIR}/include/essentia
        ${ESSENTIA_INSTALL_DIR}/include
        ${ESSENTIA_DIR}/src
)
set(ESSENTIA_LIBRARY_DIRS ${ESSENTIA_INSTALL_DIR}/lib)

message(STATUS "Essentia will be installed to: ${ESSENTIA_INSTALL_DIR}")

# Find the built Essentia library
set(ESSENTIA_LIBRARY ${ESSENTIA_INSTALL_DIR}/lib/libessentia.a)

if(APPLE)
    # Only essential system frameworks for basic functionality
    find_library(ACCELERATE_FRAMEWORK Accelerate)
    find_library(CORE_AUDIO_FRAMEWORK CoreAudio)
    find_library(AUDIO_TOOLBOX_FRAMEWORK AudioToolbox)

    set(ESSENTIA_SYSTEM_LIBRARIES
            ${ACCELERATE_FRAMEWORK}
            ${CORE_AUDIO_FRAMEWORK}
            ${AUDIO_TOOLBOX_FRAMEWORK}
    )

    # Try to find some basic libraries that might be available
    find_library(FFTW3_LIB fftw3 PATHS /usr/local/lib /opt/local/lib)
    find_library(FFTW3F_LIB fftw3f PATHS /usr/local/lib /opt/local/lib)

    # Add libsamplerate for Resample algorithm
    find_library(SAMPLERATE_LIB samplerate PATHS /usr/local/lib /opt/local/lib /opt/homebrew/lib)

    if(FFTW3_LIB AND FFTW3F_LIB)
        list(APPEND ESSENTIA_SYSTEM_LIBRARIES ${FFTW3_LIB} ${FFTW3F_LIB})
        message(STATUS "Found FFTW3 libraries: ${FFTW3_LIB}, ${FFTW3F_LIB}")
    else()
        message(STATUS "FFTW3 not found, using built-in FFT")
    endif()

    if(SAMPLERATE_LIB)
        list(APPEND ESSENTIA_SYSTEM_LIBRARIES ${SAMPLERATE_LIB})
        message(STATUS "Found libsamplerate: ${SAMPLERATE_LIB}")
    else()
        message(FATAL_ERROR "libsamplerate not found, but required for Resample algorithm")
    endif()
elseif(UNIX)
    # Minimal Linux dependencies
    set(ESSENTIA_SYSTEM_LIBRARIES pthread m)

    # Try to find FFTW3 on Linux
    find_library(FFTW3_LIB fftw3)
    find_library(FFTW3F_LIB fftw3f)
    find_library(SAMPLERATE_LIB samplerate)

    if(FFTW3_LIB AND FFTW3F_LIB)
        list(APPEND ESSENTIA_SYSTEM_LIBRARIES ${FFTW3_LIB} ${FFTW3F_LIB})
    endif()

    if(SAMPLERATE_LIB)
        list(APPEND ESSENTIA_SYSTEM_LIBRARIES ${SAMPLERATE_LIB})
    else()
        message(FATAL_ERROR "libsamplerate not found, but required for Resample algorithm")
    endif()
endif()

# NOW create the imported target for Essentia with all properties set at once
add_library(essentia_built STATIC IMPORTED)
set_target_properties(essentia_built PROPERTIES
        IMPORTED_LOCATION ${ESSENTIA_LIBRARY}
        INTERFACE_INCLUDE_DIRECTORIES "${ESSENTIA_INCLUDE_DIRS};${EIGEN_INCLUDE_DIR}"
        INTERFACE_LINK_LIBRARIES "${ESSENTIA_SYSTEM_LIBRARIES}"
)

# Make sure essentia_built depends on essentia_external
add_dependencies(essentia_built essentia_external)

# Handle JUCE setup with fallback logic
if(JUCE_DIR)
    # User specified their own JUCE path
    message(STATUS "Using user-specified JUCE from: ${JUCE_DIR}")
    add_subdirectory(${JUCE_DIR} juce_build)
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/slicer_granular/JUCE/CMakeLists.txt")
    # Use bundled JUCE in slicer_granular subdirectory (existing setup)
    message(STATUS "Using bundled JUCE from slicer_granular/JUCE/ subdirectory")
    add_subdirectory(slicer_granular/JUCE)
elseif(JUCE_FETCH_IF_MISSING)
    # Auto-fetch JUCE if allowed
    message(STATUS "JUCE not found. Fetching JUCE ${JUCE_VERSION}...")
    FetchContent_Declare(
            JUCE
            GIT_REPOSITORY https://github.com/juce-framework/JUCE.git
            GIT_TAG ${JUCE_VERSION}
            GIT_SHALLOW TRUE
            GIT_PROGRESS TRUE
    )
    FetchContent_MakeAvailable(JUCE)
else()
    message(FATAL_ERROR
            "JUCE not found!\n"
            "Please either:\n"
            "  1. Set JUCE_DIR to your JUCE installation path, or\n"
            "  2. Place JUCE in the slicer_granular/JUCE/ subdirectory, or\n"
            "  3. Enable JUCE_FETCH_IF_MISSING to auto-download JUCE"
    )
endif()

# Create the plugin target
juce_add_plugin(tsn-granular
        COMPANY_NAME "Corrode Audio"
        IS_SYNTH TRUE
        NEEDS_MIDI_INPUT TRUE
        NEEDS_MIDI_OUTPUT TRUE
        IS_MIDI_EFFECT FALSE
        EDITOR_WANTS_KEYBOARD_FOCUS FALSE
        COPY_PLUGIN_AFTER_BUILD TRUE
        PLUGIN_MANUFACTURER_CODE Crrd
        PLUGIN_CODE Tngr
        FORMATS AU VST3 Standalone
        PRODUCT_NAME "tsn-granular"
        COMPANY_WEBSITE "https://nvssynthesis.github.io/"
        AU_MAIN_TYPE kAudioUnitType_MusicDevice
        VST3_CATEGORIES Instrument Synth
        AU_SANDBOX_SAFE TRUE
)

# Collect TSN-specific source files
file(GLOB_RECURSE TSN_SOURCE_FILES
        "Source/*.cpp"
        "Source/*.h"
)

# Add delaunator source directly (bypassing its CMakeLists.txt)
set(DELAUNATOR_SOURCES
        "delaunator-cpp/src/delaunator.cpp"
)

# Add all source files to the target
target_sources(tsn-granular PRIVATE
        ${TSN_SOURCE_FILES}
        ${DELAUNATOR_SOURCES}
)

juce_generate_juce_header(tsn-granular)

set(SLICER_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/slicer_granular/Source)
file(GLOB_RECURSE SLICER_SOURCES
        "${SLICER_SOURCE_DIR}/*.cpp"
        "${SLICER_SOURCE_DIR}/*.h"
        "${CMAKE_CURRENT_SOURCE_DIR}/slicer_granular/nvs_libraries/nvs_libraries/src/*.cpp"
        "${CMAKE_CURRENT_SOURCE_DIR}/slicer_granular/nvs_libraries/nvs_libraries/include/*.h"
)
list(REMOVE_ITEM SLICER_SOURCES "${SLICER_SOURCE_DIR}/createPluginFilter.cpp")

target_sources(tsn-granular PRIVATE ${SLICER_SOURCES})

# Copy the dependency fetch logic from slicer_granular/CMakeLists.txt
# (Xoshiro, and any other FetchContent dependencies)
if(USE_SYSTEM_LIBRARIES AND DEFINED XOSHIRO_ROOT)
    set(XOSHIRO_INCLUDE_DIR ${XOSHIRO_ROOT})
else()
    FetchContent_Declare(
            Xoshiro
            GIT_REPOSITORY https://github.com/Reputeless/Xoshiro-cpp.git
            GIT_TAG master
            CONFIGURE_COMMAND ""
            BUILD_COMMAND ""
    )
    FetchContent_MakeAvailable(Xoshiro)
    set(XOSHIRO_INCLUDE_DIR ${xoshiro_SOURCE_DIR})
endif()

# fmt library
if(USE_SYSTEM_LIBRARIES)
    find_package(fmt REQUIRED)
else()
    FetchContent_Declare(
            fmt
            GIT_REPOSITORY https://github.com/fmtlib/fmt.git
            GIT_TAG 11.2.0
    )
    FetchContent_MakeAvailable(fmt)
endif()

# Include directories
target_include_directories(tsn-granular
        SYSTEM PRIVATE  # Suppresses warnings from these includes
        slicer_granular/nvs_libraries/nvs_libraries/external/sprout
        ${XOSHIRO_INCLUDE_DIR}
        ${ESSENTIA_INCLUDE_DIRS}
        delaunator-cpp/include
        PRIVATE
        Source
        slicer_granular/Source
        slicer_granular/nvs_libraries
        ${EIGEN_INCLUDE_DIR}
        PUBLIC
        ${ESSENTIA_INCLUDE_DIRS}
        ${EIGEN_INCLUDE_DIR}
)

# Link libraries
target_link_libraries(tsn-granular
        PRIVATE
        fmt::fmt
        essentia_built
        Eigen3::Eigen
        juce::juce_audio_basics
        juce::juce_audio_devices
        juce::juce_audio_formats
        juce::juce_audio_plugin_client
        juce::juce_audio_processors
        juce::juce_audio_utils
        juce::juce_core
        juce::juce_cryptography
        juce::juce_data_structures
        juce::juce_dsp
        juce::juce_events
        juce::juce_graphics
        juce::juce_gui_basics
        juce::juce_gui_extra
        PUBLIC
        juce::juce_recommended_config_flags
        juce::juce_recommended_lto_flags
        juce::juce_recommended_warning_flags
)

# Compiler definitions
target_compile_definitions(tsn-granular
        PRIVATE
        FMT_HEADER_ONLY=1
        TSN=1
        PUBLIC
        JUCE_STRICT_REFCOUNTEDPOINTER=1
        JUCE_VST3_CAN_REPLACE_VST2=0
        JUCE_WEB_BROWSER=0
        JUCE_USE_CURL=0
        JUCE_APPLICATION_NAME_STRING="$<TARGET_PROPERTY:tsn-granular,JUCE_PRODUCT_NAME>"
        JUCE_APPLICATION_VERSION_STRING="$<TARGET_PROPERTY:tsn-granular,JUCE_VERSION>"
        _LIBCPP_ENABLE_CXX20_REMOVED_TYPE_TRAITS=1
)

# Platform-specific settings
if(APPLE)
    set_target_properties(tsn-granular PROPERTIES
            MACOSX_DEPLOYMENT_TARGET "13.0"
    )

    target_compile_options(tsn-granular PRIVATE
            -Wall -Wextra -Wshadow -Wuninitialized
            -ftemplate-backtrace-limit=0
    )
endif()

# Build type specific definitions
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    target_compile_definitions(tsn-granular PRIVATE NDEBUG=1)
endif()

if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    target_compile_definitions(tsn-granular PRIVATE DEBUG=1 _DEBUG=1)
endif()

# Print configuration info
message(STATUS "TSN Granular Configuration:")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  Essentia directory: ${ESSENTIA_DIR}")
message(STATUS "  Essentia library: ${ESSENTIA_LIBRARY}")
if(APPLE)
    message(STATUS "  macOS deployment target: 13.0")
endif()