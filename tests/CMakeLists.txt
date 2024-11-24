cmake_minimum_required(VERSION 3.21)
project(LumTests LANGUAGES CXX)

# CMake settings
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_COLOR_DIAGNOSTICS ON)
set(CMAKE_INTERPROCEDURAL_OPTIMIZATION OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(BUILD_SHARED_LIBS ON)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ")
# Explicitly set the source and binary directories for lum
add_subdirectory(${CMAKE_SOURCE_DIR}/.. ${CMAKE_BINARY_DIR}/lum)

# Add the test executable
add_executable(test_main main.cpp)

# Link the test executable with lum_static (or lum_shared)
# target_link_libraries(test_main PUBLIC lum_static) # recommended

target_link_libraries(test_main PUBLIC lum) 

# Tell cmake to but test_main.exe into test/bin 
set_target_properties(test_main PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin
)

# is not needed because PUBLIC in include directories upstream makes them visible for your project (and this test)
# target_include_directories 