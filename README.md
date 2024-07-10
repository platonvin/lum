# Lum
**Lum** is a demo of a voxel engine featuring raytraced lighting with high perfomance. Targets tiled minecraft-dungeons-style blocky graphics with isometric view (but can be used differently)

## Features
- Raytraced (actually, path marched) Light
    - **Precise voxel raytracer**: Pure compute shaders without using the *VK_KHR_ray_tracing* extension
    - **Variance-guided temporal accumulation**: Considers normal and material information for improved quality
    - **`A-trous edge-preserving spatial denoiser**: Takes normal, material and depth information into account
    - **Edge-preserving upscaler**: Uses high-resolution G-buffer to guide itself, like FSR but no guessing
- **Particle system**: A simple particle system to enhance visual effects (who needs an engine without particle system?)
- **HTML-CSS style Ui**: Utilizes RmlUi for a flexible and modern user interface (**not** immediate mode)
- **Tiled world**: designed specifically for block-based scenes. (maybe not really a feature, but target audience instead)
- **Auto mesh generation**: generates and optimizes vertices from .vox files using opengametools (https://github.com/jpaver/opengametools) and meshoptimizer (https://github.com/zeux/meshoptimizer)

## Installation 
- ### Prerequisites
  - **MinGW64 C++ Compiler**: if on windows, follow instructions at https://www.msys2.org/
  - **Vcpkg**: follow instructions at https://vcpkg.io/en/getting-started
  - **Vulkan SDK**: follow instructions at https://vulkan.lunarg.com/sdk/home
  - **GNU Make**: installed with MinGW64 as mingw32-make.exe (mingw32-make.exe is refered as make)
- ### Required Libraries
  - **GLM**: Install via MSYS2
  - **GLFW**: Install via MSYS2
  - **Volk**: Part of the Vulkan SDK
  - **RmlUi**: Install via Vcpkg (for MinGW64 static triplet)
- ### Steps  
  - Install Prerequisites, make sure VULKAN_SDK and VCPKG_ROOT are set and Mingw is in the Path
  - Install RmlUi using Vcpkg:\
`$vcpkg install rmlui --triplet-x64-mingw-static --host-triplet=x64-mingw-static`
    - you may be able to use other host triplets, but i didnt test it

  - Install GLM and GLFW using MSYS2: \
`$ pacman -S mingw-w64-x86_64-glm mingw-w64-x86_64-glfw`
    - if not using MSYS2, just make sure necessary libraries and headers are installed in any other way and visible to compiler

  - Clone the repository: \
`$ git clone https://github.com/platonvin/lum.git`    

  - Navigate to the project directory:\
`$ cd lum` 
  - Initialize:\
`$ make init`   
  - Build:\
`$ make release -j6`
    - `$make clean` every time you switch from release to debug and vice versa
  - Run:\
`$ ./client.exe`   

## Engine Overview
### Stages
  1. **Ray generation**
       - Generates highres G-buffer via rasterization
  2. **G-buffer downscaling**
       - When upscaling is used, downscales images for raytracer, accumulator, and denoiser 
  3. **Block allocation**
       - Scene is tiled, and air blocks are all "0", so to translate mesh in scene temporary blocks are allocated and "pointers" to them are written to scene
  4. **Mesh translation**
       - Translates all meshes to "world space" from their "model space"
  5. **Raytracer pass**
       - Core raytracer calculations
  6. **Optional pre-accumulator denoiser pass**
       - Denoises raytraced image
  7. **Accumulator**
       - Mixes pixels with matching ones from previous frames using motion vectors
  8. **Optional post-accumulator denoiser pass**
       - Denoises accumulated image
  9. **Upscaler**
       - Uses G-buffer guided texture filtering to reconsturct highres image
  10. **Optional post-upscaler denoiser pass**
       - Denoises upscaled image
  11. **Ui and presentation**
       - Renders Ui to copy of final image and presents it

### Demo controls
- denoiser and upscaler setting in ui
- WASD for camera movement
- Arrows for robot movement
- Enter for shooting particles
- 0 to remove block underneath
- 1-9 and F1-F5 to place matching blocks (btw world is saved to a file)
- "<" and ">" to rotate camera
- Esc to close demo