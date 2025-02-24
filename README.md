# tsn-granular

tsn-granular is the timbre space navigation (TSN)-enabled version on slicer_granular. It contains all of the capabilities of slicer_granular, with the main addition that it analyzes the uploaded sample, splitting it into smaller segments via onset detection, and organizes these segments into an interactive multidimensional timbre space. 
Here is a video demonstration of the synth in action:
[![YouTube](http://i.ytimg.com/vi/31erhH7Crb4/hqdefault.jpg)](https://www.youtube.com/watch?v=31erhH7Crb4)

## Building

```bash
# Clone the repository
$ git clone https://github.com/nvssynthesis/tsn-granular.git
$ cd tsn-granular

# initialize submodules recursively
$ git submodule update --init --recursive

# build aubio
$ cd aubio
$ make

# build essentia
# Essentia has a platform-dependent build process. Please follow the official instructions:
# https://essentia.upf.edu/installing.html
# While this project doesn't make use of most of essentia's own dependencies, it has only been tested on MacOS using all optional dependencies Essentia lists, except Gaia

# Open the .jucer project and generate the IDE project
# Open `tsn-granular.jucer` in Projucer.
# Configure the desired exporter (Xcode, Visual Studio, etc.).
# Click "Save Project and Open in IDE."

# Build the plugin from the generated project
# Once the project is open in your IDE, build it using the appropriate command:
# - Xcode: Select the desired scheme and build.
# - Visual Studio: Build the solution.
```