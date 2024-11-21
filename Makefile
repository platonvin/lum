# Variables
BUILD_DIR := build

# Default target
all: build

# Create and build the project
build:
	@echo "Creating build directory and invoking CMake..."
	@cmake -S . -B $(BUILD_DIR)
	@cmake --build $(BUILD_DIR)

# Clean the build directory
clean:
	@echo "Cleaning build directory..."
	@cmake --build $(BUILD_DIR) --target clean || echo "Nothing to clean"

# Rebuild the project
rebuild: clean build

# Phony targets to avoid conflicts with files named 'build', 'clean', etc.
.PHONY: all build clean rebuild
