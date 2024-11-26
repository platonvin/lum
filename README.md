[![Build Linux GCC](https://github.com/platonvin/lum/actions/workflows/cmake-linux-gcc.yml/badge.svg)](https://github.com/platonvin/lum/actions/workflows/cmake-linux-gcc.yml)
[![Build Linux Clang](https://github.com/platonvin/lum/actions/workflows/cmake-linux-clang.yml/badge.svg)](https://github.com/platonvin/lum/actions/workflows/cmake-linux-clang.yml) [![Build with Windows MinGW (MSYS2)](https://github.com/platonvin/lum/actions/workflows/cmake-windows-mingw.yml/badge.svg)](https://github.com/platonvin/lum/actions/workflows/cmake-windows-mingw.yml)

# Lum
**Lum** is a voxel\* renderer\*\* built with Vulkan. Currently, it's only available in a form of C99/C++ API, but might (on request) be ported (in form of bindings) into Unity / Unreal / languages that support C interop. Currently, it is also in process of porting to Rust (in pure Rust form).

\* Note: In Lum, "voxel" refers to a small, grid-aligned cube with a single material. Lum expects you to group voxels into blocks and blocks into world, but also supports non-grid-aligned models at a minor performance cost.
\*\* Note: Lum also has ECS ([check it out!](src/engine/README.md)), input, ~~Ui,~~ meshloader (and will have voxel physics engine), but it's stil mostly GPU code - so it's called renderer

If you have ideas or suggestions for the API, feel free to open an [issue](https://github.com/platonvin/lum/issues)

##### Some demo footage
https://github.com/user-attachments/assets/ce7883c4-a706-406f-875c-fbf23d68020d

#### Some (demo) benchmarks:
 * Intel Iris XE (integrated gpu!) - <7 mspf in FullHD
 * NVIDIA 1660s - ~1.6 mspf in FullHD 

## Feature-list
[md file with Lum:Renderer features](FEATURES.md)

## Installation 

### Prerequisites 
 
- **C++23 Compiler**  
  - Recommended: [MSYS2 MinGW](https://www.msys2.org/)  for Windows, GNU C++ for Linux.

  - See the badges above for compilers that are verified to work.
 
  - **Note:**  MSVC is not currently supported. If you have experience with MSVC and are willing to contribute, help is welcome!
 
- **CMake 3.22 or newer**  
  - Install via your package manager, e.g., `sudo apt-get install cmake` (on Debian-based systems), or [build from source](https://github.com/Kitware/CMake), or [download from official site](https://cmake.org/download/)
 
- **Vulkan support**

- on Linux, GLFW will ask you to install multiple different packages. You can do it in advance :
  - for Debian / Ubuntu / Mint:\
  `sudo apt install libxinerama-dev libxcursor-dev xorg-dev libglu1-mesa-dev pkg-config build-essential libxkbcommon-dev libwayland-dev`
  - for Fedore / Red Hat:\
  `sudo dnf install wayland-devel libxkbcommon-devel libXcursor-devel libXi-devel libXinerama-devel libXrandr-devel`
  - for FreeBSD:\
  `pkg install wayland libxkbcommon evdev-proto xorgproto`  

<!--todo: why? libxkbcommon-dev libwayland-client0.1-0 libwayland-cursor0 libwayland-egl1.0-0 wayland-protocols libwayland-dev libxinerama-dev libxcursor-dev xorg-dev libglu1-mesa-dev -->

### Build Instructions 
 
1. Clone the repository with it's submodules:


```bash
git clone https://github.com/platonvin/lum.git
cd lum
git submodule update --init --recursive
```
 
2. Build using **CMake** :

```bash
mkdir build
cd build
cmake .. -DCMAKE_BUILD_TYPE=Release
cmake --build .
```
To improve performance for release builds, you can enable **LTO**  (Link Time Optimization) :

```bash
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_INTERPROCEDURAL_OPTIMIZATION=ON
cmake --build .
```

Note: this will only build lum library, not demo. For demo, go to [examples](examples/README.md)

### Integration 
To use Lum as a subproject in your **CMake**  project:
See [examples/CMakeLists.txt](https://chatgpt.com/c/examples/CMakeLists.txt) for integration details.\
If you're using a different build system, include `include/lum.hpp` (or `include/clum.h`) and link against the `lum`, `lumal`, `glfw3`, `freetype`, `brotlidec` and some os-specific libraries*. Note that Lum also builds them (so they can be found in `lum/lib` (or `lum/examples/lib`, if building examples))\
Note: for C++ api (`lum.hpp`) you will also need glm to be in path (so `#include <glm/glm.hpp>` is possible). You can use "pocket" lum's glm installation at `lum/external/lum-al/external/glm`

* for windows, also link with `gdi32`
* for linux, also link with `dl`

## Usage:
 - [Lum::Renderer](src/renderer/README.md)
 - [Lum::ECSystem](src/engine/README.md)
 - [Lum::InpuHandler](src/input/README.md)

### Demo controls
- WASD for camera movement
- Arrows for robot movement
- Enter for shooting particles
- 0 to remove block underneath
- 1-9 and F1-F5 to place matching blocks (btw world is saved to a file)
- "<" and ">" to rotate camera
- "Page Up" and "Page Down" to zoom in/out
- Esc to close demo
