cmake_minimum_required(VERSION 3.15)

project(tsn-granular VERSION 0.6.5)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

include(FetchContent)

# Options for external dependencies
option(USE_SYSTEM_LIBRARIES "Use system-installed libraries instead of fetching" OFF)

# JUCE setup options
set(JUCE_DIR "" CACHE PATH "Path to JUCE framework (leave empty to use bundled JUCE in slicer_granular/JUCE subdirectory)")
option(JUCE_FETCH_IF_MISSING "Automatically fetch JUCE if JUCE_DIR is not set and JUCE/ subdirectory doesn't exist" ON)
set(JUCE_VERSION "8.0.10" CACHE STRING "JUCE version to fetch if JUCE_FETCH_IF_MISSING is ON")

find_package(Eigen3 REQUIRED NO_MODULE)

if(TARGET Eigen3::Eigen)
    get_target_property(EIGEN_INCLUDE_DIR Eigen3::Eigen INTERFACE_INCLUDE_DIRECTORIES)
    message(STATUS "Found Eigen3: ${EIGEN_INCLUDE_DIR}")
endif()

# Handle JUCE setup with fallback logic
if(JUCE_DIR)
    message(STATUS "Using user-specified JUCE from: ${JUCE_DIR}")
    add_subdirectory(${JUCE_DIR} juce_build)
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/slicer_granular/JUCE/CMakeLists.txt")
    message(STATUS "Using bundled JUCE from slicer_granular/JUCE/ subdirectory")
    add_subdirectory(slicer_granular/JUCE)
elseif(JUCE_FETCH_IF_MISSING)
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

add_subdirectory(../tsn-analyzer/juce_utils tsn-analyzer-juce-utils)
add_subdirectory(../tsn-analyzer/Source/lib tsn-analyzer-lib)

# Create the plugin target
juce_add_plugin(tsn-granular
        COMPANY_NAME "CorrodeAudio"
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

# Collect TSN-specific source files (excluding Analysis submodule)
file(GLOB_RECURSE TSN_SOURCE_FILES
        "Source/*.cpp"
        "Source/*.h"
)
# Remove the Analysis submodule files from the glob
list(FILTER TSN_SOURCE_FILES EXCLUDE REGEX "Source/Analysis/.*")

# Add delaunator source directly
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

# Xoshiro dependency
set(XOSHIRO_DIR "${CMAKE_SOURCE_DIR}/xoshiro-cpp")

if(USE_SYSTEM_LIBRARIES AND DEFINED XOSHIRO_ROOT)
    set(XOSHIRO_INCLUDE_DIR ${XOSHIRO_ROOT})
    message(STATUS "Using system Xoshiro from: ${XOSHIRO_INCLUDE_DIR}")
else()
    if(NOT EXISTS "${XOSHIRO_DIR}/XoshiroCpp.hpp")
        message(STATUS "Xoshiro not found or incomplete, cloning from GitHub...")

        if(EXISTS "${XOSHIRO_DIR}")
            message(STATUS "Removing incomplete Xoshiro directory...")
            file(REMOVE_RECURSE "${XOSHIRO_DIR}")
        endif()

        execute_process(
                COMMAND git clone --depth 1 https://github.com/Reputeless/Xoshiro-cpp.git "${XOSHIRO_DIR}"
                WORKING_DIRECTORY ${CMAKE_SOURCE_DIR}
                RESULT_VARIABLE GIT_CLONE_RESULT
                OUTPUT_VARIABLE GIT_CLONE_OUTPUT
                ERROR_VARIABLE GIT_CLONE_ERROR
        )

        if(NOT GIT_CLONE_RESULT EQUAL 0)
            message(STATUS "Git clone output: ${GIT_CLONE_OUTPUT}")
            message(STATUS "Git clone error: ${GIT_CLONE_ERROR}")
            message(FATAL_ERROR "Failed to clone Xoshiro repository")
        endif()

        message(STATUS "Xoshiro cloned successfully")
    else()
        message(STATUS "Found existing Xoshiro directory: ${XOSHIRO_DIR}")
    endif()

    set(XOSHIRO_INCLUDE_DIR ${XOSHIRO_DIR})
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
#        ${ESSENTIA_INCLUDE_DIRS}
        delaunator-cpp/include
        PRIVATE
        Source
        slicer_granular/Source
        slicer_granular/nvs_libraries
        ${EIGEN_INCLUDE_DIR}
)

# Link libraries
target_link_libraries(tsn-granular
        PRIVATE
        fmt::fmt
        tsn_analyzer  # brings in Essentia, Eigen, and JUCE modules automatically
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

get_target_property(TSN_ANALYZER_INCLUDES tsn_analyzer INTERFACE_INCLUDE_DIRECTORIES)
target_include_directories(tsn-granular SYSTEM PRIVATE ${TSN_ANALYZER_INCLUDES})

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
if(APPLE)
    message(STATUS "  macOS deployment target: 13.0")
endif()