cmake_minimum_required(VERSION 3.15)

add_compile_options(-w)

project(tsn-granular VERSION 0.4.0)

message(STATUS "Slicer-granular links: ${SLICER_LINK_LIBS}")

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Options for external dependencies
option(USE_SYSTEM_LIBRARIES "Use system-installed libraries instead of fetching" OFF)

# Add slicer_granular as subdirectory (this will handle its own dependencies including JUCE)
#add_compile_definitions(TSN=1) # needs to be defined to alter slicer-granular's compilation
add_subdirectory(slicer_granular)
get_target_property(SLICER_LINK_LIBS slicer-granular INTERFACE_LINK_LIBRARIES)

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

# Set up build directory for Essentia
set(ESSENTIA_BUILD_DIR "${CMAKE_CURRENT_BINARY_DIR}/essentia_build")
set(ESSENTIA_INSTALL_DIR "${CMAKE_CURRENT_BINARY_DIR}/essentia_install")

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
    message(STATUS "Using Eigen from: ${EIGEN_INCLUDE_DIR}")

    # Set environment variables for the waf build, including Eigen path
    set(ENV{SDKROOT} ${MACOS_SDK_PATH})
    set(ENV{CPPFLAGS} "-isysroot ${MACOS_SDK_PATH} -I${EIGEN_INCLUDE_DIR}")
    set(ENV{CFLAGS} "-isysroot ${MACOS_SDK_PATH}")
    set(ENV{CXXFLAGS} "-isysroot ${MACOS_SDK_PATH} -I${EIGEN_INCLUDE_DIR}")
    set(ENV{LDFLAGS} "-isysroot ${MACOS_SDK_PATH}")
else()
    # For Linux, just add Eigen to the compiler flags
    message(STATUS "Using Eigen from: ${EIGEN_INCLUDE_DIR}")
    set(ENV{CPPFLAGS} "-I${EIGEN_INCLUDE_DIR}")
    set(ENV{CXXFLAGS} "-I${EIGEN_INCLUDE_DIR}")
endif()

# Configure with minimal features to reduce build time and dependencies
# Add --no-msse for M1 Macs to avoid archiver issues
set(WAF_CONFIGURE_ARGS "waf" "configure" "--build-static" "--prefix=${ESSENTIA_INSTALL_DIR}" "--lightweight=" "--no-msse")

execute_process(
        COMMAND ${PYTHON3_EXECUTABLE} ${WAF_CONFIGURE_ARGS}
        WORKING_DIRECTORY ${ESSENTIA_DIR}
        RESULT_VARIABLE WAF_CONFIGURE_RESULT
        OUTPUT_VARIABLE WAF_CONFIGURE_OUTPUT
        ERROR_VARIABLE WAF_CONFIGURE_ERROR
)

if(NOT WAF_CONFIGURE_RESULT EQUAL 0)
    message(STATUS "WAF Configure Output: ${WAF_CONFIGURE_OUTPUT}")
    message(STATUS "WAF Configure Error: ${WAF_CONFIGURE_ERROR}")
    message(FATAL_ERROR "Essentia waf configure failed")
endif()

message(STATUS "Building Essentia...")

# Set parallel jobs - use a default if CMAKE_BUILD_PARALLEL_LEVEL is not set
if(NOT CMAKE_BUILD_PARALLEL_LEVEL)
    set(CMAKE_BUILD_PARALLEL_LEVEL 4)
endif()

# Build Essentia
execute_process(
        COMMAND ${PYTHON3_EXECUTABLE} waf -j ${CMAKE_BUILD_PARALLEL_LEVEL}
        WORKING_DIRECTORY ${ESSENTIA_DIR}
        RESULT_VARIABLE WAF_BUILD_RESULT
        OUTPUT_VARIABLE WAF_BUILD_OUTPUT
        ERROR_VARIABLE WAF_BUILD_ERROR
)

if(NOT WAF_BUILD_RESULT EQUAL 0)
    message(STATUS "WAF Build Output: ${WAF_BUILD_OUTPUT}")
    message(STATUS "WAF Build Error: ${WAF_BUILD_ERROR}")
    message(FATAL_ERROR "Essentia waf build failed")
endif()

message(STATUS "Installing Essentia...")

# Install Essentia
execute_process(
        COMMAND ${PYTHON3_EXECUTABLE} waf install
        WORKING_DIRECTORY ${ESSENTIA_DIR}
        RESULT_VARIABLE WAF_INSTALL_RESULT
        OUTPUT_VARIABLE WAF_INSTALL_OUTPUT
        ERROR_VARIABLE WAF_INSTALL_ERROR
)

# Check if install actually succeeded even if waf returned an error
# Look for key files that should be installed
set(EXPECTED_HEADER "${ESSENTIA_INSTALL_DIR}/include/essentia/essentia.h")
set(EXPECTED_VERSION "${ESSENTIA_INSTALL_DIR}/include/essentia/version.h")

if(EXISTS "${EXPECTED_HEADER}" OR EXISTS "${EXPECTED_VERSION}")
    message(STATUS "Essentia installation appears successful (found expected files)")
    set(INSTALL_SUCCESS TRUE)
elseif(WAF_INSTALL_RESULT EQUAL 0)
    message(STATUS "Essentia installed successfully")
    set(INSTALL_SUCCESS TRUE)
else()
    message(STATUS "WAF Install Output: ${WAF_INSTALL_OUTPUT}")
    message(STATUS "WAF Install Error: ${WAF_INSTALL_ERROR}")
    set(INSTALL_SUCCESS FALSE)
endif()

if(NOT INSTALL_SUCCESS)
    message(FATAL_ERROR "Essentia waf install failed")
endif()

message(STATUS "Essentia built and installed successfully")

# Set up Essentia variables for linking
# Try install directory first, then fall back to build directory
if(EXISTS "${ESSENTIA_INSTALL_DIR}/include")
    set(ESSENTIA_INCLUDE_DIRS
            ${ESSENTIA_INSTALL_DIR}/include/essentia
            ${ESSENTIA_INSTALL_DIR}/include
            ${ESSENTIA_DIR}/src
    )
    set(ESSENTIA_LIBRARY_DIRS ${ESSENTIA_INSTALL_DIR}/lib)
    message(STATUS "Using installed Essentia from: ${ESSENTIA_INSTALL_DIR}")
else()
    # Fall back to build directory
    set(ESSENTIA_INCLUDE_DIRS
            ${ESSENTIA_DIR}/src
            ${ESSENTIA_DIR}/build/src
    )
    set(ESSENTIA_LIBRARY_DIRS
            ${ESSENTIA_DIR}/build/src
            ${ESSENTIA_DIR}/build
    )
    message(STATUS "Using built Essentia from: ${ESSENTIA_DIR}/build")
endif()

# Debug: Check what actually got installed/built
message(STATUS "Checking Essentia files...")
message(STATUS "Include directories: ${ESSENTIA_INCLUDE_DIRS}")
message(STATUS "Library directories: ${ESSENTIA_LIBRARY_DIRS}")

foreach(LIB_DIR ${ESSENTIA_LIBRARY_DIRS})
    if(EXISTS "${LIB_DIR}")
        file(GLOB LIB_CONTENTS "${LIB_DIR}/*essentia*")
        if(LIB_CONTENTS)
            message(STATUS "Found in ${LIB_DIR}: ${LIB_CONTENTS}")
        endif()
    endif()
endforeach()

# Find the built Essentia library - try multiple locations and names
find_library(ESSENTIA_LIBRARY
        NAMES essentia libessentia
        PATHS ${ESSENTIA_LIBRARY_DIRS}
        NO_DEFAULT_PATH
)

# If not found, try a broader search
if(NOT ESSENTIA_LIBRARY)
    file(GLOB_RECURSE ESSENTIA_LIBS
            "${ESSENTIA_INSTALL_DIR}/*essentia*.a"
            "${ESSENTIA_DIR}/build/*essentia*.a"
            "${ESSENTIA_DIR}/build/*essentia*.dylib"
            "${ESSENTIA_DIR}/build/*essentia*.so"
    )

    if(ESSENTIA_LIBS)
        # Use the first library found
        list(GET ESSENTIA_LIBS 0 ESSENTIA_LIBRARY)
        message(STATUS "Found Essentia library via search: ${ESSENTIA_LIBRARY}")
    endif()
endif()

if(NOT ESSENTIA_LIBRARY)
    message(FATAL_ERROR "Could not find built Essentia library. Searched in: ${ESSENTIA_LIBRARY_DIRS}")
endif()

message(STATUS "Found Essentia library: ${ESSENTIA_LIBRARY}")

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

    if(FFTW3_LIB AND FFTW3F_LIB)
        list(APPEND ESSENTIA_SYSTEM_LIBRARIES ${FFTW3_LIB} ${FFTW3F_LIB})
        message(STATUS "Found FFTW3 libraries: ${FFTW3_LIB}, ${FFTW3F_LIB}")
    else()
        message(STATUS "FFTW3 not found, using built-in FFT")
    endif()
elseif(UNIX)
    # Minimal Linux dependencies
    set(ESSENTIA_SYSTEM_LIBRARIES pthread m)

    # Try to find FFTW3 on Linux
    find_library(FFTW3_LIB fftw3)
    find_library(FFTW3F_LIB fftw3f)

    if(FFTW3_LIB AND FFTW3F_LIB)
        list(APPEND ESSENTIA_SYSTEM_LIBRARIES ${FFTW3_LIB} ${FFTW3F_LIB})
    endif()
endif()

# NOW create the imported target for Essentia with all properties set at once
add_library(essentia_built STATIC IMPORTED)
set_target_properties(essentia_built PROPERTIES
        IMPORTED_LOCATION ${ESSENTIA_LIBRARY}
        INTERFACE_INCLUDE_DIRECTORIES "${ESSENTIA_INCLUDE_DIRS};${EIGEN_INCLUDE_DIR}"
        INTERFACE_LINK_LIBRARIES "${ESSENTIA_SYSTEM_LIBRARIES}"
)


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
        PLUGIN_CODE Slgr
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

# Include directories
target_include_directories(tsn-granular
        PRIVATE
        Source
#        slicer_granular/Source
#        slicer_granular/nvs_libraries
        ${ESSENTIA_INCLUDE_DIRS}
        ${EIGEN_INCLUDE_DIR}  # Changed from ${eigen_SOURCE_DIR}
        delaunator-cpp/include
        PUBLIC
        ${ESSENTIA_INCLUDE_DIRS}
        ${EIGEN_INCLUDE_DIR}  # Changed from ${eigen_SOURCE_DIR}
)

# Link libraries
target_link_libraries(tsn-granular
        PRIVATE
        slicer-granular
        fmt::fmt
        essentia_built
        Eigen3::Eigen  # Add this line
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