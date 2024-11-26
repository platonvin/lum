cmake_minimum_required(VERSION 3.22)
project(lum VERSION "0.1.4" LANGUAGES CXX C)

# ---- Notes for Users ----
# - Custom flags for Release and Develop modes ensure optimal builds for Lum and its dependencies.
# - Interprocedural optimization (LTO) is enabled in Release mode for performance.
# - Develop mode uses lighter optimization for Lum itself while applying stronger optimization to dependencies.

# ATTENTION:
# I am bad in CMake. If you have better ideas on how to do all these - please help

# ---- CMake Settings ----
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_COLOR_DIAGNOSTICS ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# ---- Options ----
# option(BUILD_EXAMPLES "Build example programs (also test validness of linking to libs)" ON)
# option(LUM_CREATE_DEMO_PACKAGE "Create an archive of binaries, libraries, and shaders" OFF) # not fully implemented
option(LUM_ENABLE_UNITY_BUILD "Enable Unity (Jumbo) Build" OFF) # not fully implemented
option(LUM_ENABLE_EXTRA_WARNINGS "Enable extra compiler warnings (e.g., -Wall, -Wextra)" OFF)
option(LUM_CUSTOM_FLAGS "Apply custom flags (non-default in Release) when building Lum" OFF)
# Set LUM_CUSTOM_FLAGS to ON if the project is top-level (standalone build)
if(PROJECT_IS_TOP_LEVEL AND NOT LUM_CUSTOM_FLAGS)
    set(LUM_CUSTOM_FLAGS ON)
endif()

# ---- Default Build Type ----
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Choose build type: Debug, Develop, or Release" FORCE)
endif()

# ---- Interprocedural Optimization (LTO) ----
# set(CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)

# ---- Compiler-Specific Flags ----
if(LUM_CUSTOM_FLAGS)
    # Define SIMD and common flags
    if(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        set(SIMD_FLAGS "/arch:AVX") # Adjust?
    else()
        set(SIMD_FLAGS "-mmmx -msse -msse2 -msse3 -mssse3 -msse4.1 -msse4.2 -mcx16 -mavx -mpclmul")
    endif()

    set(COMMON_FLAGS "")
    if(LUM_ENABLE_EXTRA_WARNINGS)
        set(COMMON_FLAGS "-Wall -Wextra -Wpedantic -Wno-unused-variable")
    endif()

    # Set build type specific flags
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU" OR CMAKE_CXX_COMPILER_ID MATCHES "Clang")
        set(RELEASE_FLAGS "-O3 -ffast-math ${SIMD_FLAGS} ${COMMON_FLAGS} -Wl,--gc-sections -s")
        # set(DEVELOP_FLAGS "-O2 ${SIMD_FLAGS} ${COMMON_FLAGS}")
        set(LUM_DEVELOP_FLAGS "-O1 ${COMMON_FLAGS}") # Apply lighter optimization for Lum in Develop mode
    elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
        set(RELEASE_FLAGS "/X /FS /GL /O2 /Oi /Gy ${SIMD_FLAGS} ${COMMON_FLAGS} /Qstrip_debug")
        # set(DEVELOP_FLAGS "/O2 ${SIMD_FLAGS} ${COMMON_FLAGS}")
        set(LUM_DEVELOP_FLAGS "/X /FS /Od ${COMMON_FLAGS}") # Equivalent to -O1
    endif()

    # Apply flags globally
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${RELEASE_FLAGS}")
    if(CMAKE_BUILD_TYPE STREQUAL "Develop")
        # For Lum, lighter optimization; dependencies use normal develop flags
        set_target_properties(${PROJECT_NAME} PROPERTIES COMPILE_FLAGS "${LUM_DEVELOP_FLAGS}")
        # set(CMAKE_CXX_FLAGS "${DEVELOP_FLAGS}")
    endif()
endif()

# ---- Output Directories ----
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/bin)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_SOURCE_DIR}/lib)

# ---- Dependencies ----
# Submodules. Basically downloads subprojects from github
find_package(Git)
if(Git_FOUND)
  message("Git found: ${GIT_EXECUTABLE}")
endif()
execute_process(
    COMMAND ${GIT_EXECUTABLE} submodule update --init --recursive
    WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
    RESULT_VARIABLE GIT_SUBMODULE_RESULT)
if(NOT GIT_SUBMODULE_RESULT EQUAL 0)
    message(WARNING "Failed to initialize or update git submodules. Update them manually with  $> git submodule update --init --recursive")
endif()

# freetype - font lib for rmlui
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/external/freetype)
    set(FT_DISABLE_ZLIB TRUE)
    set(FT_DISABLE_BZIP2 TRUE)
    set(FT_DISABLE_PNG TRUE)
    set(FT_DISABLE_HARFBUZZ TRUE)
    add_subdirectory(external/freetype)
else()
    message(FATAL_ERROR "FreeType directory not found.")
endif()

# rmlui - library for perfomant ui rendering
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/external/RmlUi)
    set(RMLUI_FONT_ENGINE none)
    set(RMLUI_STATIC_LIBRARIES ON)
    set(RMLUI_BUILD_SHARED_LIBRARIES OFF)
    set(RMLUI_CUSTOM_RTTI ON)
    add_subdirectory(external/RmlUi)
else()
    message(FATAL_ERROR "RmlUi directory not found.")
endif()

# lumal - lum's abstraction layer for vulkan
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/external/lum-al)
    add_subdirectory(external/lum-al)
else()
    message(FATAL_ERROR "lum-al directory not found.")
endif()

# Project sources
file(GLOB_RECURSE PROJECT_SOURCES
    src/renderer/*.cpp
    common/*.cpp
    common/*.c
)
# Exclude problematic files temporarily
list(REMOVE_ITEM PROJECT_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/renderer/src/ui.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/renderer/src/render_ui_interface.cpp
)

# unity build in unnecessary when lto is used. But might be useful
if(LUM_ENABLE_UNITY_BUILD)
    message(STATUS "Unity build enabled. Combining source files into a single translation unit.")
        # Unity build source
    set(UNITY_SOURCE ${CMAKE_BINARY_DIR}/unity_build.cpp)
        # Generate the unity build file
    file(WRITE ${UNITY_SOURCE} "// Unity Build Source File. Generated with CMake\n")
        # Add lumal sources (defined in lumal's CMakeLists)
    message(STATUS "Adding Lum-al sources to unity build: ${LUMAL_SOURCES}")
    foreach(SRC ${LUMAL_SOURCES})
        file(APPEND ${UNITY_SOURCE} "#include \"${SRC}\"\n")
    endforeach()
        # Add Lum sources
    message(STATUS "Adding Lum sources to unity build: ${LUMAL_SOURCES}")
    foreach(SRC ${PROJECT_SOURCES})
        file(APPEND ${UNITY_SOURCE} "#include \"${SRC}\"\n")
    endforeach()
        # Override sources with unity build file. Otherwise there is duplication
    set(PROJECT_SOURCES ${UNITY_SOURCE})
endif()

add_library(lum ${PROJECT_SOURCES})

# set_target_properties(lum PROPERTIES CMAKE_INTERPROCEDURAL_OPTIMIZATION OFF)
# if(CMAKE_BUILD_TYPE STREQUAL "Release")
#     set_target_properties(lum PROPERTIES CMAKE_INTERPROCEDURAL_OPTIMIZATION ON)
# endif()

# Set (internal?) include directories
target_include_directories(lum PUBLIC # TODO: private
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/renderer/api
    ${CMAKE_CURRENT_SOURCE_DIR}/common
    ${CMAKE_CURRENT_SOURCE_DIR}/external/RmlUi/Include
    ${CMAKE_CURRENT_SOURCE_DIR}/external
)

# Set API headers directory, so #include <lum.hpp> or <clum.h> is possible
target_include_directories(lum PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include # API headers directory
)

# ---- Link lum with lumal and freetype ----
target_link_libraries(lum PUBLIC lumal freetype)