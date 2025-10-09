# tsn-granular

tsn-granular is the timbre space navigation (TSN)-enabled version on slicer_granular. It contains all of the capabilities of slicer_granular, with the main addition that it analyzes the uploaded sample, splitting it into smaller segments via onset detection, and organizes these segments into an interactive multidimensional timbre space. 
Here is a video demonstration of the synth in action:
[![YouTube](http://i.ytimg.com/vi/31erhH7Crb4/hqdefault.jpg)](https://www.youtube.com/watch?v=31erhH7Crb4)

## Building

### Prerequisites
- CMake 3.15 or higher
- C++20 compatible compiler
- Eigen3
- libsamplerate
- Python 3.8 or higher (for building Essentia)
- Git

**Note:** JUCE is included as a submodule and will be automatically fetched when you run `git submodule update --init --recursive`.

**Note:** Build instructions below are provided for macOS and Linux. Windows builds should be possible but have not been tested. Windows users will need to install the prerequisites through vcpkg, chocolatey, or manual installation.

#### macOS
```bash
brew install eigen libsamplerate cmake
```
Note: macOS builds require macOS 13.0 or higher as the deployment target.

#### Linux
```bash
sudo apt-get install libeigen3-dev libsamplerate0-dev cmake python3
```

## Build Instructions
### Clone the repository

```bash
git clone https://github.com/nvssynthesis/tsn-granular.git
cd tsn-granular
```
### Initialize submodules recursively
```bash
git submodule update --init --recursive
```
### Create build directory
```bash
mkdir build
cd build
```

### Configure and build
```bash
cmake ..
cmake --build . --config Release
```
The plugin will be automatically copied to your system's plugin directory

Note: Essentia is automatically cloned and built by CMake during the first build. This may take several minutes.
## Build Options

### Parallel Builds

Speed up compilation by setting the number of parallel jobs:

```bash
CMAKE_BUILD_PARALLEL_LEVEL=8 cmake --build . --config Release
```

### Using System Libraries

By default, some dependencies (fmt, Xoshiro) are fetched automatically. To use system-installed versions instead:

```bash
cmake -DUSE_SYSTEM_LIBRARIES=ON ..
cmake --build . --config Release
```

### Plugin Formats
The build produces:
- **AU** (Audio Unit) - macOS only
- **VST3** - macOS and Linux
- **Standalone** application

All formats are automatically installed to your system's standard plugin directories after building.