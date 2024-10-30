[![Build](https://github.com/platonvin/lum/actions/workflows/c-cpp.yml/badge.svg)](https://github.com/platonvin/lum/actions/workflows/c-cpp.yml)

# Lum
**Lum** is a voxel\* renderer\*\* I'm developing because none of the existing ones meet my needs\*\*\*. Currently, it's only available in a form of C++ API, but might (on request) be ported into Unity/Unreal/C

\* As any domain specific renderer, it has very limited usecase. By voxel i mean "small single-material cube, grid-aligned with its neighbours". Lum also wants you to unite voxels into blocks, and blocks into world. Non-world-grid aligned models are still alowed, but they have to be voxel too, and they also have some perfomance cost (~1 microsecond per model)
\*\* technically, it also has input system, Ui, meshloader (and will have voxel physics engine), but it is stil mostly GPU code\
\*\*\* my needs are high perfomance, dynamic GI, simplicity and very specific style (similar to Minecraft Dungeons). I did not find any perfomant engine i would enjoy and understand enough so i made Lum.\
\*\*\*\* If you have any API suggestions, open an issue

##### Some demo footage
https://github.com/user-attachments/assets/ce7883c4-a706-406f-875c-fbf23d68020d


## Feature-list

*for perfomance lists go to end

support level:
 - 0 - not implemented yet
 - 1 - partially implemented (e.g. hardcoded)
 - 2 - fully implemented

| Feature | Support Level | Description | 
| --- | --- | --- | 
| High performance  | 3 | Fully optimized :rocket: - works well even on integrated GPUs | 
| Library separation | 1.5 | simple API exists, but it might need improvements | 
| Raytraced voxel-space reflections (glossy) | 2 | Fully implemented | 
| Non-per-block diffuse light | 0.1 | Implemented in past, not fast enough yet (requires accumulation and denoising) | 
| Ambient Occlusion | 2 | Fully implemented (HBAO btw) | 
| Per-block raytraced lighting (diffuse) | 1.9 | implemented, needs 6-direction radiance | 
| Lightmaps | 1 | Partially implemented (Sun only) | 
| Deferred rendering | 2 | Fully implemented | 
| Particle system | 1 | Partially implemented (CPU side, no blocks) | 
| Foliage rendering | 1 | Partially implemented (hardcoded grass) | 
| Water rendering | 1.9 | Nearly complete (cascaded DFT, no control) | 
| Volumetrics | 1 | Partially implemented (hardcoded raster) | 
| Postprocess | 1 | Partially implemented (needs sharpening, proper values for correction) | 
| UI | 1 | Partially implemented (needs? some RmlUI abstraction) |
| Material palette | 1 | Needs better management | 
| Bloom | 0 | Not implemented (Does Lum need ~~bLum~~ bloom?) | 
| Transparency | 0 | Not implemented. Big challenge BTW | 
| Settings | 1 | Partially implemented (CPU-side only; needs block size, material palette size, etc.) | 
| OpenGL backend | 0 | this is going to be a long journey... |

## Installation
- ### Prerequisites
  - **C++ Compiler**: [MSYS2 MinGW](https://www.msys2.org/) recommended for Windows. For Linux prefer GNU C++
  - \[optional\] **Vcpkg**: follow instructions at [Vcpkg](https://vcpkg.io/en/getting-started). If no vcpkg found in PATH, it will be installed automatically
  - \[optional, used to build demo / liblum.a\] **Make**: for Linux, typically installed by default. For Windows, install manually (shipped with MinGW)
  - **Vulkan support**

- ### Steps  
  - make sure you have C++20 compiler and Make (and optionally Vcpkg). If you want to use non-default triplet (compiler) for Vcpkg, set VCPKG_DEFAULT_TRIPLET environment variable to desired triplet
  - get the repository: \
`$ git clone https://github.com/platonvin/lum.git` for *unstable* version or [download code from releases](https://github.com/platonvin/lum/releases)     
  - navigate to the project directory:\
 - `$ cd lum` 

 - `$ make`
    - on Linux, GLFW will ask you to install multiple different packages, but you can do it in advance:\
     `sudo apt install libxinerama-dev libxcursor-dev xorg-dev libglu1-mesa-dev pkg-config build-essential`

 - There is `unity.cpp` file, that includes every used source file. You can use it for your own unity (single translation unit) build - just include `lum/unity.cpp` in your own

You can also [download](https://github.com/platonvin/lum/releases) pre-built demo for Windows

## Engine frame overview
```mermaid
flowchart TD
%% Nodes
    Frame[\"Frame"/]
    Compute[["Compute"]]
        BAS("Bulding acceleration structure")
        Radiance("Update radiance")
    Rpass1(["lighmap pass"])
        1blocks("blocks")
        1models("models") 
    Rpass2(["gbuffer pass"])
        2blocks("blocks")
        2models("models") 
        2particles("particles")
        2grass("grass")
        2water("water")
    Rpass3(["shading pass"])
            3diffuse("diffuse (radiance + lightmaps)")
            3ambient("ambient occlusion")
            3glossy("glossy ray generation")
            3smoke("smoke ray generation")
            3glossy("glossy")
            3smoke("volumetrics")
            3tonemapping("tonemapping & color correction")
            3ui("ui")
    Graphics[["Graphics"]]
    Present[["Present"]] 

%% Edge connections between nodes
    Frame --> Compute
        Compute --> BAS --> Graphics
        Compute --> Radiance --> Graphics
    %% Frame --> Graphics
    Graphics --> Rpass1
        Rpass1 --> 1blocks --> Rpass2
        Rpass1 --> 1models --> Rpass2
    %% Graphics --> Rpass2
        Rpass2 --> 2blocks --> Rpass3
        Rpass2 --> 2models --> Rpass3
        Rpass2 --> 2particles --> Rpass3
        Rpass2 --> 2grass --> Rpass3
        Rpass2 --> 2water --> Rpass3
    %% Graphics --> Rpass3
        Rpass3 --> 3diffuse --> Present
        Rpass3 --> 3ambient --> Present
        Rpass3 --> 3glossy --> Present
        Rpass3 --> 3smoke --> Present
        Rpass3 --> 3tonemapping --> Present
        Rpass3 --> 3ui --> Present
    %% Compute --> Graphics
    %% 3diffuse --> 3ambient
    %% 3ambient --> 3glossy
    %% 3glossy --> 3smoke
    %% Rpass1 --> Rpass2
    %% Rpass2 --> Rpass3
    %% Rpass3 --> Present

%% Individual node styling
    style Frame color:#1E1A1D, fill:#AFAAB9, stroke:#1B2A41

    style Compute color:#1E1A1D, fill:#EDF6F9, stroke:#1B2A41
    style Graphics color:#1E1A1D, fill:#EDF6F9, stroke:#1B2A41

    style Present color:#1E1A1D, fill:#AFAAB9, stroke:#1B2A41

    style BAS color:#1E1A1D, fill:#D6CA98, stroke:#1B2A41
    style Radiance color:#1E1A1D, fill:#D6CA98, stroke:#1B2A41

    style Rpass1 color:#1E1A1D, fill:#2C6661, stroke:#1B2A41
    style 1blocks color:#1E1A1D, fill:#2C666E, stroke:#1B2A41
    style 1models color:#1E1A1D, fill:#2C666E, stroke:#1B2A41

    style Rpass2 color:#1E1A1D, fill:#7CA975, stroke:#1B2A41
    style 2blocks color:#1E1A1D, fill:#7CA982, stroke:#1B2A41
    style 2models color:#1E1A1D, fill:#7CA982, stroke:#1B2A41
    style 2particles color:#1E1A1D, fill:#7CA982, stroke:#1B2A41
    style 2grass color:#1E1A1D, fill:#7CA982, stroke:#1B2A41
    style 2water color:#1E1A1D, fill:#7CA982, stroke:#1B2A41

    style Rpass3 color:#1E1A1D, fill:#83C5B1, stroke:#1B2A41
    style 3diffuse color:#1E1A1D, fill:#83C5BE, stroke:#1B2A41
    style 3ambient color:#1E1A1D, fill:#83C5BE, stroke:#1B2A41
    style 3glossy color:#1E1A1D, fill:#83C5BE, stroke:#1B2A41
    style 3smoke color:#1E1A1D, fill:#83C5BE, stroke:#1B2A41
    style 3glossy color:#1E1A1D, fill:#83C5BE, stroke:#1B2A41
    style 3smoke color:#1E1A1D, fill:#83C5BE, stroke:#1B2A41
    style 3tonemapping color:#1E1A1D, fill:#83C5BE, stroke:#1B2A41
    style 3ui color:#1E1A1D, fill:#83C5BE, stroke:#1B2A41
```

### Performance TODO List 
| Feature | Support Level | Description | 
| --- | --- | --- | 
| Shader JIT | 0 | TODO, +settings (specialization constants?) | 
| World shift | 0 | TODO | 
| Stencil buffer generation | 1 | Discard in FS is used currently, TODO: simple raster | 
| No-stall swapchain recreation | 0 | AKA resize window | 
| LOD for "rough" rays | 0 | Not implemented | 
| Small vertex buffer optimization | 0 | Not implemented | 
| Depth sorting | 1.7 | Needs better algorithm | 
| Radiance | 1 | Needs more stable work distribution | 
| Better indexing | 0 | Not implemented (beta omega curve? Morton?) | 


### Some implemented optimizations 
| Optimization | Support Level  | Description | 
| --- | --- | --- | 
| Contour meshing | 2 | No per-voxel data in vertices. Saves VFA, PES+VPC, memory (cache, VRAM) | 
| GPU-driven grass | 2 | No per-blade data. Saves memory | 
| Water simulation | 2 | Cascaded DFT. Saves memory | 
| HBAO LUT | 2 | Completed. Reduces computations for small memory| 
| BVH hierarchy | 2 | Least possible scoreboard stall for huge saves | 
| Subpasses | 1.99 | Nearly complete. Might save a lot of memory, does on mobile and some others | 
| Screen-space volumetrics | 2 | Custom depth buffer via programmable blend min/max. Saves 3d volumetrics structure | 
| Inverse voxel mapping | 2 | Mapping from world from model instead of from model into world. Prevents override | 
| Smallest possible formats | 1.99 | Some formats are just not faster | 
| Quaternions | 2 | Fully implemented (no stretching tho. 50% memory saves) | 
| Smallest precision | 2 | 16/8 bit when possible. Saves a ton | 
| Shadow samplers | 2 | No gains on modern hardware |

### Demo controls
- WASD for camera movement
- Arrows for robot movement
- Enter for shooting particles
- 0 to remove block underneath
- 1-9 and F1-F5 to place matching blocks (btw world is saved to a file)
- "<" and ">" to rotate camera
- "Page Up" and "Page Down" to zoom in/out
- Esc to close demo
