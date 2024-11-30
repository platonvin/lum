cmake_minimum_required(VERSION 3.22)
project(LumTests LANGUAGES C CXX)

# CMake settings
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_COLOR_DIAGNOSTICS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(BUILD_SHARED_LIBS OFF) # disable shared (dynamic) libraries, so build static

# Explicitly set the source and binary directories for lum
# Note: it look weird because examples/ folder is inside lum/ folder
# if Lum is your git submodule / just folder, that you can just do
# add_subdirectory(path/to/lum), e.g. add_subdirectory(external/lum)
add_subdirectory(${CMAKE_SOURCE_DIR}/.. ${CMAKE_BINARY_DIR}/lum)


if (CMAKE_BUILD_TYPE STREQUAL "Develop")
    # lto is too much for development
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION OFF)
    # decreases compile speed, but this is absolutely neccessary for lum
    set(CMAKE_CXX_FLAGS "-O1")
    set(LUM_CUSTOM_FLAGS OFF) 
else()
    # Enable -flto for higher perfomance. Note that it increases compile times
    set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
endif()

# Add the test executable named test_demo_cpp
add_executable(test_demo_cpp demo.cpp)
add_executable(test_demo_c99 
    cdemo.c
    cdemo_api_impl.cpp # this has to be compiled with c++ compiler
)

# Link the test executable with lum (or lum)
# This will automatically propagate include directiory with lum headers
# so target_include_directories is not needed to add lum to include paths
# Note: if you are not using CMake, you still need to add them manually
target_link_libraries(test_demo_cpp PRIVATE lum)
target_link_libraries(test_demo_c99 PRIVATE lum)

# Tell cmake to but test_demo_cpp.exe into test/bin 
set_target_properties(test_demo_cpp test_demo_c99 PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin
)
