# thin makefile to invoke CMake
# for people who do not want to (e.g)
# cmake -G "Ninja" .. -DCMAKE_BUILD_TYPE=Release
# cmake --build . --verbose


# Variables
BUILD_DIR := build

# Default target
all: build

# configure
build:
	@echo "Creating build directory and invoking CMake..."
	@cmake -S . -B $(BUILD_DIR)

build_gcc:
	@echo "Creating build directory and invoking CMake..."
	@cmake -DCMAKE_CXX_COMPILER=g++ -DCMAKE_C_COMPILER=gcc -S . -B $(BUILD_DIR)
	@cmake --build $(BUILD_DIR)

build_clang:
	@echo "Creating build directory and invoking CMake..."
	@cmake -DCMAKE_CXX_COMPILER=clang++ -DCMAKE_C_COMPILER=clang -S . -B $(BUILD_DIR)
	@cmake --build $(BUILD_DIR)
	
build_v:
	@echo "Creating build directory and invoking CMake..."
	@cmake -S . -B $(BUILD_DIR)
	@cmake --build $(BUILD_DIR) --verbose

o_build:
	@cmake --build $(BUILD_DIR)

# Clean the build directory
clean:
	@echo "Cleaning build directory..."
	@cmake --build $(BUILD_DIR) --target clean || echo "Nothing to clean"

pack:
ifeq ($(OS),Windows_NT)
	-mkdir "package"
	-mkdir "package\shaders\compiled"
	-mkdir "package\assets"
	-mkdir "package\lib"
	-mkdir "package\bin"
	-xcopy /s /i /y ".\bin\*.exe" ".\package\bin\*.exe*"
	-xcopy /s /i /y ".\lib\*.dll" ".\package\bin\*.dll*"
	-xcopy /s /i /y ".\lib\*.dll" ".\package\lib\*.dll*"
	-xcopy /s /i /y ".\lib\*.a" ".\package\lib\*.a*"
	-xcopy /s /i /y "shaders\compiled" "package\shaders\compiled"
	-xcopy /s /i /y "assets" "package\assets"
# powershell Compress-Archive -Update ./package package.zip
else
	-mkdir -p package
	-mkdir -p package/shaders/compiled
	-mkdir -p package/assets
	cp bin/client_rel package/client
	cp -a /shaders/compiled /package/shaders/compiled
	cp -a /assets           /package/assets
	zip -r package.zip package
endif

# Rebuild the project
rebuild: clean build
# Phony targets to avoid conflicts with files named 'build', 'clean', etc.
.PHONY: all build clean rebuild
