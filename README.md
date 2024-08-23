# Lum
**Lum** is a voxel engine I'm developing because none of the existing ones meet my needs. Currently, it's not available as a standalone project but is bundled with a demo instead

##### Screenshot
![alt text](screenshot_v0.4.png)

## Installation 
- ### Prerequisites
  - **MinGW64 C++ Compiler**: If you're on Windows, follow instructions at [MSYS2](https://www.msys2.org/)
    - other compilers support not tested
  - **Vcpkg**: follow instructions at [Vcpkg](https://vcpkg.io/en/getting-started)
  - **Vulkan SDK**: follow instructions at [LunarG](https://vulkan.lunarg.com/sdk/home)
    - Vulkan support is required. However, you might be able to use MoltenVK or other implementations
  - **GNU Make**: installed with MinGW64 as mingw32-make.exe (mingw32-make.exe is refered as make)
- ### Required Libraries
  - **[GLM](https://github.com/g-truc/glm)**: Install via MSYS2
  - **[GLFW](https://www.glfw.org/)**: Install via MSYS2
  - **[Volk](https://github.com/zeux/volk)**: Part of the Vulkan SDK
  - **[RmlUi](https://mikke89.github.io/RmlUiDoc/)**: Install via Vcpkg (for MinGW64 static triplet)
- ### Steps  
  - Install Prerequisites, make sure VULKAN_SDK and VCPKG_ROOT are set and Mingw is in the Path
  - Install RmlUi using Vcpkg:\
`$vcpkg install rmlui --triplet=x64-mingw-static --host-triplet=x64-mingw-static`
    - you may be able to use other triplets, but i didnt test it

  - Install GLM and GLFW using MSYS2: \
`$ pacman -S mingw-w64-x86_64-glm mingw-w64-x86_64-glfw`
    - if not using MSYS2, just make sure necessary libraries and headers are installed in any other way and visible to your compiler

  - Get the repository: \
`$ git clone https://github.com/platonvin/lum.git` for *unstable* version or [download code from releases](https://github.com/platonvin/lum/releases)     

  - Navigate to the project directory:\
`$ cd lum` 
  - Initialize:\
`$ make init`   
  - Build and run:\
`$ make release -j10`
    - during development, `$make clean` if something (-pipe) broke

Alternatively, you can [download](https://github.com/platonvin/lum/releases) pre-built version for windows

## Engine Overview
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

### Demo controls
- WASD for camera movement
- Arrows for robot movement
  - Enter for shooting particles
- 0 to remove block underneath
- 1-9 and F1-F5 to place matching blocks (btw world is saved to a file)
- "<" and ">" to rotate camera
- "Page Up" and "Page Down" to zoom in/out
- Esc to close demo
