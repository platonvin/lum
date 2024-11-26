[![Build](https://github.com/platonvin/lum/actions/workflows/c-cpp.yml/badge.svg)](https://github.com/platonvin/lum/actions/workflows/c-cpp.yml)

# Lum
**Lum** is a voxel\* renderer\*\* i'm developing. Currently, it's only available in a form of C99/C++ API, but might (on request) be ported into Unity / Unreal

\* As any domain specific renderer, it has very limited usecase. By voxel i mean "small single-material cube, grid-aligned with its neighbours". Lum also wants you to unite voxels into blocks, and blocks into world. Non-world-grid aligned models are still alowed, but they have to be voxel too, and they also have some extra perfomance cost (~1 microsecond per model)\
\*\* Lum also has ECS ([check it out!](src/engine/README.md)), input, Ui, meshloader (and will have voxel physics engine), but it's stil mostly GPU code

If you have any API suggestions, open an issue

##### Some demo footage
https://github.com/user-attachments/assets/ce7883c4-a706-406f-875c-fbf23d68020d

#### Some (demo) benchmarks:
 * Intel Iris XE (integrated gpu!) - <7 mspf in FullHD
 * NVIDIA 1660s - ~1.6 mspf in FullHD 

## Feature-list
[md file with Lum:Renderer features](FEATURES.md)

## Installation
- ### Prerequisites
  - **C++23 Compiler**: [MSYS2 MinGW](https://www.msys2.org/) recommended for Windows. For Linux prefer GNU C++
  - \[optional\] **Vcpkg**: follow instructions at [Vcpkg](https://vcpkg.io/en/getting-started). If no vcpkg found in PATH, it will be installed automatically
  - \[optional, used for build\] **Make**: for Linux, typically installed by default. For Windows, install manually (shipped with MinGW. Maybe shipped with VS)
  - **Vulkan support**

- ### Steps
 - make sure you have C++23 compiler and Make (and optionally Vcpkg). If you want to use non-default triplet (compiler) for Vcpkg, set VCPKG_DEFAULT_TRIPLET environment variable to desired triplet
 - get the repository: \
`$ git clone https://github.com/platonvin/lum.git` for *unstable* version or [download code from releases](https://github.com/platonvin/lum/releases)     
 - navigate to the project directory: `$ cd lum` 
 - build:  `$ make`
    - on Linux, GLFW will ask you to install multiple different packages, but you can do it in advance:\
     `sudo apt install libxinerama-dev libxcursor-dev xorg-dev libglu1-mesa-dev pkg-config build-essential`

 - There is `src/unity/unity_lib.cpp` file, that includes every used source file for the C++ API. You can use it for your own unity (single translation unit) build - just include `lum/unity/unity_lib.cpp` in your own. Same for C99 API - `src/unity/unity_c_lib.cpp` (it has to be compiled by C++23 compiler tho)

You can also [download](https://github.com/platonvin/lum/releases) pre-built demo's and libraries for Windows\
After building, you will have both static and dynamic libraries for C99 and C++23 Lum::Renderer API

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
