# Lum::Renderer 
`Lum::Renderer` is main Lum module - voxel rendering API. Supports both C++ and C99 interfaces
## Building the Renderer 

### C++ API 
[api](api/renderer.hpp)
The C++ API is available in `api/renderer.hpp`. You can compile it (library) using the following commands: 
- static library:
`make build_unity_lib_static_cpp`/ `make build_unity_lib_dynamic_cpp` for static / dynamic or just build everything: `make`

### C API 
[api](api/crenderer.h)
The C API can be found in `api/crenderer.h`. Compile with: 
`make build_unity_lib_static_c99` / `make build_unity_lib_dynamic_c99` for static / dynamic or just build everything: `make`

Both APIs are wrappers around the internal API, which is less stable. For examples of how to use the APIs, please refer to the [examples directory](https://chatgpt.com/examples)