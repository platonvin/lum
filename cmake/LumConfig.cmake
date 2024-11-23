# LumConfig.cmake - configuration for Lum
include(CMakeFindDependencyMacro)

# Find dependencies
find_dependency(Freetype REQUIRED)
find_dependency(RmlUi REQUIRED)

# Target configuration
set(LUM_TARGETS lum_static)

# Add an alias for users to use `lum::lum` instead of manually handling the target
add_library(lum::lum ALIAS lum_static)

# Include directories
set(LUM_INCLUDE_DIRS
    ${CMAKE_CURRENT_LIST_DIR}/../src/
    ${CMAKE_CURRENT_LIST_DIR}/../common
    ${CMAKE_CURRENT_LIST_DIR}/../external/lum-al/external
    ${CMAKE_CURRENT_LIST_DIR}/../external/lum-al/src
    ${CMAKE_CURRENT_LIST_DIR}/../external/RmlUi/Include
    ${CMAKE_CURRENT_LIST_DIR}/../external
)

# Set variables for CMake config
set(LUM_LIBRARIES lum_static)

# Export the include directories
set(LUM_INCLUDE_DIRS ${LUM_INCLUDE_DIRS} CACHE INTERNAL "Lum include directories")
set(LUM_LIBRARIES ${LUM_LIBRARIES} CACHE INTERNAL "Lum libraries")

# Export the target
export(TARGETS lum_static FILE LumTargets.cmake)
