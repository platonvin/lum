# Variables
COMPILER ?= default         # Compiler: gcc, clang, msvc, or default (system default)
# BUILD_TYPE ?= Release       # Build type: Debug or Release
BUILD_DIR := build_$(COMPILER) # Compiler-specific build directory

# Detect OS
ifeq ($(OS),Windows_NT)
    RM = del /s /q
    RMDIR = rmdir /s /q
    MKDIR = if not exist "$(BUILD_DIR)" mkdir "$(BUILD_DIR)"
    SLASH = \ # Windows uses backslashes for paths
    CMAKE_GEN_FLAGS =
else
    RM = rm -rf
    RMDIR = rm -rf
    MKDIR = mkdir -p
    SLASH = / # Unix-like systems use forward slashes
    CMAKE_GEN_FLAGS = -G "Unix Makefiles"
endif

# Targets
.PHONY: all configure build clean clean_all

all: build

# Configure the build directory
configure:
	@echo "Configuring project for $(COMPILER)..."
	$(MKDIR)
ifeq ($(COMPILER),gcc)
	@cmake -S . -B $(BUILD_DIR) $(CMAKE_GEN_FLAGS) -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++
else ifeq ($(COMPILER),clang)
	@cmake -S . -B $(BUILD_DIR) $(CMAKE_GEN_FLAGS) -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++
else ifeq ($(COMPILER),msvc)
	@echo "Configuring project for MSVC using Visual Studio generator..."
	@cmake -S . -B $(BUILD_DIR) -G "Visual Studio 17 2022"
else
	@cmake -S . -B $(BUILD_DIR) $(CMAKE_GEN_FLAGS)
endif

# Build the project
build: configure
	@cmake --build $(BUILD_DIR)

# Clean the build directory
clean:
	@echo "Cleaning build directory $(BUILD_DIR)..."
ifeq ($(OS),Windows_NT)
	-$(RM) $(BUILD_DIR)$(SLASH)* && $(RMDIR) $(BUILD_DIR)
else
	-$(RM) $(BUILD_DIR)
endif

# Clean all build directories
clean_all:
	@echo "Cleaning all build directories..."
ifeq ($(OS),Windows_NT)
	-for /d %d in (build_*) do $(RMDIR) %d
else
	-$(RM) build_*
endif
