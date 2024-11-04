.ONESHELL:

# Directories for includes and libraries
INCLUDE_DIRS = -Isrc -Icommon -Ilum-al/src -Iinclude -I./
LINK_DIRS = -Llum-al/lib

# Linux is so good that static doesn't work
STATIC_OR_DYNAMIC = 
# libs that i do not include in internal-unity-build
# Windows is so good that lum_vcpkg/vcpkg.exe is not executable
# if anyone knows good way to solve "\" "/" ".exe" ".\" "./" problem - please tell me
EXTERNAL_LIBS = -lglfw3 -lvolk -lRmlDebugger -lRmlCore -lfreetype -lpng -lbrotlienc -lbrotlidec -lbrotlicommon -lpng16 -lz -lbz2
ifeq ($(OS),Windows_NT)
	EXTERNAL_LIBS += -lgdi32       
	STATIC_OR_DYNAMIC += -static
	WHICH_WHERE = where
	RUN_POSTFIX = .exe
	DYN_LIB_POSTFIX = .dll
	RUN_PREFIX = .\\
	COPY_COMMAND = copy
	FPIC = 
	SLASH = \\

else
	EXTERNAL_LIBS += -lpthread -ldl
	RUN_PREFIX = ./
	WHICH_WHERE = which
	RUN_POSTFIX = 
	DYN_LIB_POSTFIX = .so
	COPY_COMMAND = cp
	SLASH = /
	FPIC = -fPIC
endif

# lumal is Lum's Abstraction Layer for vulkan
REQUIRED_LIBS = $(EXTERNAL_LIBS) -llumal
# default version. If native found, it overrides this one
VCPKG_EXEC := $(RUN_PREFIX)lum_vcpkg$(SLASH)vcpkg$(RUN_POSTFIX) --vcpkg-root=lum_vcpkg
	
# {Largest Common Set} of {x86 instruction sets} for most {x86 CPU}'s made in last ~15 years 
COMMON_INSTRUCTIONS = -mmmx -msse -msse2 -msse3 -mssse3 -msse4.1 -msse4.2 -mcx16 -mavx -mpclmul

# Compiler flags
# -ftrivial-auto-var-init=zero is somehow not supported by other compilers?
COMMON_FLAGS = -Wuninitialized -std=c++23
DEB_FLAGS = $(COMMON_FLAGS) -O0 -g 
DEV_FLAGS = $(COMMON_FLAGS) -O2 -DVKNDEBUG $(COMMON_INSTRUCTIONS)
REL_FLAGS = $(COMMON_FLAGS) -Ofast $(COMMON_INSTRUCTIONS) -s -Wl,--gc-sections

# Shader compilation settings
SHADER_FLAGS = -V --target-env vulkan1.1 -g
SHADER_SRC_DIR = shaders
SHADER_OUT_DIR = shaders/compiled
SHADERS_EXTRA_DEPEND = \
	shaders/common/ext.glsl\
	shaders/common/ubo.glsl\

# Source files, mostly for incremental build while developing
LIB_CPP_SOURCES = \
    src/renderer/api/renderer.cpp \
    src/renderer/src/internal_render.cpp \
    src/renderer/src/ao_lut.cpp \
    src/renderer/src/setup.cpp \
    src/renderer/src/load_stuff.cpp \
    src/renderer/src/render_ui_interface.cpp \
    src/renderer/src/ui.cpp \
    common/ogt_vox.cpp \
    common/ogt_voxel_meshify.cpp \
    common/meshopt.cpp

LIB_C99_SOURCES = \
    src/renderer/api/crenderer.cpp $(LIB_CPP_SOURCES)

ALL_SOURCES = \
    src/examples/demo.cpp \
    src/examples/cdemo.c $(LIB_C99_SOURCES)

#single-translation-unit build source files
UNITY_FILE_LIB_CPP := src/unity/unity_lib.cpp
UNITY_FILE_LIB_C99 := src/unity/unity_c_lib.cpp
UNITY_FILE_DEMO_CPP := src/unity/unity_demo.cpp
UNITY_FILE_DEMO_C99 := src/unity/unity_c_demo.cpp

# Object files by build type
DEBUG_OBJECTS = $(patsubst src/%.cpp,obj/deb/%.o,$(filter src/%.cpp,$(ALL_SOURCES))) \
                $(patsubst src/%.c,obj/deb/%.o,$(filter src/%.c,$(ALL_SOURCES)))

DEV_OBJECTS = $(patsubst src/%.cpp,obj/rel/%.o,$(filter src/%.cpp,$(ALL_SOURCES))) \
              $(patsubst src/%.c,obj/rel/%.o,$(filter src/%.c,$(ALL_SOURCES)))

# No release objects - release is unity build

COMMON_OBJECTS = $(patsubst common/%.cpp,obj/%.o,$(filter common/%.cpp,$(ALL_SOURCES))) \
                 $(patsubst common/%.c,obj/%.o,$(filter common/%.c,$(ALL_SOURCES)))

# Default target. Basically "compile all libs and demos"
all: init only_build_unity

# Rule for re-evaluation after vcpkg_installed created. 
# Eval needed because vcpkg_installed does not exist when they initially evaluated
.PHONY: vcpkg_installed_eval
vcpkg_installed_eval: vcpkg_installed | check_vcpkg_itself
	$(eval OTHER_DIRS := $(filter-out vcpkg_installed/vcpkg, $(wildcard vcpkg_installed/*)) )
	$(eval INCLUDE_LIST := $(addsuffix /include, $(OTHER_DIRS)) )
	$(eval INCLUDE_LIST += $(addsuffix /include/vma, $(OTHER_DIRS)) )
	$(eval INCLUDE_LIST += $(addsuffix /include/volk, $(OTHER_DIRS)) )
	$(eval LIB_LIST := $(addsuffix /lib, $(OTHER_DIRS)) )
	$(eval INCLUDE_DIRS += $(addprefix -I, $(INCLUDE_LIST)) )
	$(eval LINK_DIRS += $(addprefix -L, $(LIB_LIST)) )
	$(eval GLSLC_DIR := $(firstword $(foreach dir, $(OTHER_DIRS), $(wildcard $(dir)/tools/glslang))) )
	$(eval GLSLC := $(strip $(GLSLC_DIR))/glslang ) 
	$(eval GLSLC := $(subst /,$(SLASH),$(GLSLC)) )

setup: init vcpkg_installed_eval lum-al/lib/liblumal.a

# Compilation pattern rules for different build types
# If someone knows a way to simplify this, please tell me 
obj/%.o: common/%.cpp | setup
	c++ $(DEV_FLAGS) $(INCLUDE_DIRS) $(args) -MMD -MP -c $< -o $@
-include $(COMMON_OBJECTS:.o=.d)

obj/rel/%.o: src/%.cpp | setup
	c++ $(DEV_FLAGS) $(INCLUDE_DIRS) $(args) -MMD -MP -c $< -o $@
-include $(DEV_OBJECTS:.o=.d)

obj/rel/%.o: src/%.c | setup
	cc -O2 -DVKNDEBUG $(COMMON_INSTRUCTIONS) $(INCLUDE_DIRS) $(args) -MMD -MP -c $< -o $@
-include $(DEV_OBJECTS:.o=.d)

obj/deb/%.o: src/%.cpp | setup
	c++ $(DEB_FLAGS) $(INCLUDE_DIRS) $(args) -MMD -MP -c $< -o $@
-include $(DEBUG_OBJECTS:.o=.d)

# Shader compilation
$(SHADER_OUT_DIR)/%.spv: $(SHADER_SRC_DIR)/% $(SHADERS_EXTRA_DEPEND) | vcpkg_installed_eval
	$(GLSLC) -o $@ $< $(SHADER_FLAGS)
# spirv-opt optimized version. It is reliably ~0.002% (2 / 100000) faster
# $(SHADER_OUT_DIR)/%.spv.temp: $(SHADER_SRC_DIR)/% $(SHADERS_EXTRA_DEPEND)
# 	$(GLSLC) -o $@ $< $(SHADER_FLAGS)
# $(SHADER_OUT_DIR)/%.spv: $(SHADER_OUT_DIR)/%.spv.temp
# 	spirv-opt $< -o $@ -O

ALL_SHADERS := $(wildcard $(SHADER_SRC_DIR)/*.vert) \
	 $(wildcard $(SHADER_SRC_DIR)/*.frag) \
	 $(wildcard $(SHADER_SRC_DIR)/*.comp) \
	 $(wildcard $(SHADER_SRC_DIR)/*.geom)
ALL_SHADER_TARGETS = $(patsubst $(SHADER_SRC_DIR)/%, $(SHADER_OUT_DIR)/%.spv, $(ALL_SHADERS))

# Build targets
compile_shaders: folders vcpkg_installed $(SHADERS_EXTRA_DEPEND) $(ALL_SHADER_TARGETS)

debug: setup compile_shaders $(COMMON_OBJECTS) $(DEBUG_OBJECTS) build_debug 
	$(RUN_PREFIX)bin/demo_deb$(RUN_POSTFIX)

# Develop also builds & runs cpp demo
dev: setup compile_shaders $(COMMON_OBJECTS) $(DEV_OBJECTS) build_dev 
	$(RUN_PREFIX)bin/demo_dev$(RUN_POSTFIX)

# Release also builds & runs cpp demo
rel: setup compile_shaders build_unity_lib_static_cpp build_unity_lib_static_c99 build_unity_demo_cpp
	$(RUN_PREFIX)bin/unity_demo_cpp$(RUN_POSTFIX)

#not separate on purpose. make cleanr before usage
dev_p: args = -D_PRINTLINE
dev_p: dev

#not separate on purpose. make cleanr before usage
dev_vfs: args = -DVSYNC_FULLSCREEN
dev_vfs: dev

lumal:
	cd lum-al
	make library
	cd ..

lum-al/lib/liblumal.a: vcpkg_installed | check_vcpkg_itself
	git submodule init
	git submodule update
	cd lum-al
	make library
	cd ..

#mostly for testing
only_build: init compile_shaders lum-al/lib/liblumal.a $(COMMON_OBJECTS) $(DEV_OBJECTS) build_dev

only_build_unity: init compile_shaders build_unity_lib_static_cpp build_unity_lib_static_c99 build_demo_stagic_cpp build_demo_stagic_c99 build_demo_dynamic_c99 build_demo_dynamic_cpp

#i could not make it work without this. Maybe posssible with eval
build_deb: setup $(DEBUG_OBJECTS) $(COMMON_OBJECTS)
	c++ -o bin/demo_deb $(DEBUG_OBJECTS) $(COMMON_OBJECTS) $(DEB_FLAGS) $(INCLUDE_DIRS) $(LINK_DIRS) $(REQUIRED_LIBS) $(STATIC_OR_DYNAMIC)
build_dev: setup $(COMMON_OBJECTS) $(DEV_OBJECTS) 
	c++ -o bin/demo_dev $(COMMON_OBJECTS) $(DEV_OBJECTS) $(DEV_FLAGS) $(INCLUDE_DIRS) $(LINK_DIRS) $(REQUIRED_LIBS) $(STATIC_OR_DYNAMIC)

# build libs objects and ar them
# C++ API lib
obj/rel/unity/unity_lib.o: src/unity/unity_lib.cpp $(LIB_CPP_SOURCES) | setup
	c++ $(FPIC) $(REL_FLAGS) $(INCLUDE_DIRS) $(args) -MMD -MP -c $< -o $@
DEPS = $(DEV_OBJECTS:.o=.d)
-include $(DEPS)
build_unity_lib_static_cpp: obj/rel/unity/unity_lib.o
	ar rvs lib/liblum.a obj/rel/unity/unity_lib.o
# C99 API lib
obj/rel/unity/unity_c_lib.o: src/unity/unity_c_lib.cpp $(LIB_C99_SOURCES) | setup
	c++ $(FPIC) $(REL_FLAGS) $(INCLUDE_DIRS) $(args) -MMD -MP -c $< -o $@
DEPS = $(DEV_OBJECTS:.o=.d)
-include $(DEPS)
build_unity_lib_static_c99: setup obj/rel/unity/unity_c_lib.o
	ar rvs lib/libclum.a obj/rel/unity/unity_c_lib.o

# Windows DLL i used for Unity Game Engine bindings
# for now this ugly script and Windows-only
build_unity_lib_dynamic_c99: obj/rel/unity/unity_c_lib.o
	c++ -shared -o lib/clum$(DYN_LIB_POSTFIX) obj/rel/unity/unity_c_lib.o $(REL_FLAGS) $(INCLUDE_DIRS) $(LINK_DIRS) \
		$(EXTERNAL_LIBS) -lstdc++ -Wl,--gc-sections
ifeq ($(OS),Windows_NT)
	$(COPY_COMMAND) lib\clum$(DYN_LIB_POSTFIX) bin\clum$(DYN_LIB_POSTFIX)
else
	$(COPY_COMMAND) lib/clum$(DYN_LIB_POSTFIX) bin/clum$(DYN_LIB_POSTFIX)
endif


build_unity_lib_dynamic_cpp: obj/rel/unity/unity_lib.o
	c++ -shared -o lib/lum$(DYN_LIB_POSTFIX) obj/rel/unity/unity_lib.o $(REL_FLAGS) $(INCLUDE_DIRS) $(LINK_DIRS) \
		$(EXTERNAL_LIBS) -lstdc++ -lm -Wl,--gc-sections
ifeq ($(OS),Windows_NT)
	$(COPY_COMMAND) lib\lum$(DYN_LIB_POSTFIX) bin\lum$(DYN_LIB_POSTFIX)
else
	$(COPY_COMMAND) lib/lum$(DYN_LIB_POSTFIX) bin/lum$(DYN_LIB_POSTFIX)
endif	
# clean_dll:
# 	rm -f lib/clum$(DYN_LIB_POSTFIX)
# 	rm -f lib/lum$(DYN_LIB_POSTFIX)

# example of full unity build for demos
build_unity_demo_cpp: setup compile_shaders
	c++ -o bin/unity_demo_cpp$(RUN_POSTFIX) $(UNITY_FILE_DEMO_CPP) $(REL_FLAGS) $(INCLUDE_DIRS) $(LINK_DIRS) $(EXTERNAL_LIBS) $(STATIC_OR_DYNAMIC)
# yep, you will need to compile your C with C++ compiler if you want full unity build
build_unity_demo_99: setup compile_shaders
	c++ -o bin/unity_demo_c99$(RUN_POSTFIX) $(UNITY_FILE_DEMO_C99) $(REL_FLAGS) $(INCLUDE_DIRS) $(LINK_DIRS) $(EXTERNAL_LIBS) $(STATIC_OR_DYNAMIC)

# example of unity library + separate (unity or not unity) app, linked with unity-built lib
# note that you dont have to link with lumal
build_demo_stagic_cpp: setup compile_shaders obj/rel/examples/demo.o build_unity_lib_static_cpp
	c++         -o bin/demo_static_cpp$(RUN_POSTFIX) obj/rel/examples/demo.o  -Llib  -llum $(REL_FLAGS) -Iinclude $(LINK_DIRS) $(EXTERNAL_LIBS) $(STATIC_OR_DYNAMIC)
# (!) library was (& has to be) built with C++ compiler, but your code can be compiled and linked by C compiler
# you still HAVE TO link against -lstdc++ (and put it last. Order matters)
build_demo_stagic_c99: setup compile_shaders obj/rel/examples/cdemo.o build_unity_lib_static_c99
	cc -std=c99 -o bin/demo_static_c99$(RUN_POSTFIX) obj/rel/examples/cdemo.o -Llib -lclum -Ofast $(common_instructions) -s -Wl,--gc-sections -Iinclude $(LINK_DIRS) $(EXTERNAL_LIBS) -lstdc++ -lm $(STATIC_OR_DYNAMIC)

build_demo_dynamic_c99: setup compile_shaders obj/rel/examples/cdemo.o build_unity_lib_dynamic_c99
	cc -std=c99 -o bin/demo_dynamic_c99$(RUN_POSTFIX) obj/rel/examples/cdemo.o lib/clum$(DYN_LIB_POSTFIX) -Ofast $(common_instructions) -s -Wl,--gc-sections -Iinclude -Llib
build_demo_dynamic_cpp: setup compile_shaders obj/rel/examples/demo.o build_unity_lib_dynamic_cpp
	c++ -o bin/demo_dynamic_cpp$(RUN_POSTFIX) obj/rel/examples/demo.o lib/lum$(DYN_LIB_POSTFIX) -Llib $(REL_FLAGS)


fun:
	@echo -e '\033[0;36m' fun was never an option '\033[0m'
test:
	c++ -pg test.cpp -o test -Wl,--stack,1000000
	test

# tested on Windows only
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

# yeah windows wants quotes and backslashes so linux obviously wants no quotes and forward slashes. They have to be incompatible.
cleans: folders
ifeq ($(OS),Windows_NT)
	-del "shaders\compiled\*.spv" 
else
	-rm -R shaders/compiled/*.spv
endif

cleand: folders
ifeq ($(OS),Windows_NT)
	-del "obj\deb\*.o" 
	-del "obj\deb\renderer\*.o"  
else
	-rm -R obj/deb/*.o
	-rm -R obj/deb/renderer/*.o
endif

cleanr: folders
ifeq ($(OS),Windows_NT)
	-del "obj\rel\*.o"  
	-del "obj\rel\renderer\*.o"  
	-del "obj\rel\renderer\*.o"  
else
	-rm -R obj/rel/*.o
	-rm -R obj/rel/renderer/*.o
endif

CLEAN_PATHS = \
	obj/*.o \
	obj/deb/*.o \
	obj/rel/*.o \
	obj/deb/renderer/*.o \
	obj/rel/renderer/*.o \
	obj/deb/examples/*.o \
	obj/rel/examples/*.o \
	obj/deb/unity/*.o \
	obj/rel/unity/*.o \
	shaders/compiled/*.spv \
	lum-al/lib/*.a \
	lib/*.a \
	bin/*$(RUN_POSTFIX)\
	bin/*$(DYN_LIB_POSTFIX)

clean: folders
ifeq ($(OS),Windows_NT)
	$(foreach path, $(CLEAN_PATHS), -del "$(subst /,\,$(path))" ;)
else
	$(foreach path, $(CLEAN_PATHS), -rm -R $(path);)
endif

init: folders vcpkg_installed vcpkg_installed_eval lum-al/lib/liblumal.a
folders: bin/ lib/ obj/ obj/deb/ obj/rel/ shaders/compiled/\
	 obj/deb/renderer/ obj/rel/renderer/\
	 obj/deb/unity/ obj/rel/unity/\
	 obj/deb/examples/ obj/rel/examples/\

%/: #pattern rule to make all folders
ifeq ($(OS),Windows_NT)
	mkdir "$@"
else
	mkdir -p $@
endif

# try to find native vcpkg 
VCPKG_FOUND := $(shell $(WHICH_WHERE) vcpkg)

#sorry microsoft no telemetry today
lum_vcpkg:
	@echo No vcpkg in PATH, installing vcpkg
	git clone https://github.com/microsoft/vcpkg.git lum_vcpkg
	cd lum_vcpkg
ifeq ($(OS),Windows_NT)
	-$(RUN_PREFIX)bootstrap-vcpkg.bat -disableMetrics 
else
	-$(RUN_PREFIX)bootstrap-vcpkg.sh -disableMetrics 
endif
	vcpkg$(RUN_POSTFIX) integrate install
	@echo bootstrapped

# if no vcpkg found, install it directly in Lum 
ifdef VCPKG_FOUND
check_vcpkg_itself:
	$(info NATIVE VCPKG IS FOUND AT $(VCPKG_FOUND))
	$(eval VCPKG_EXEC := vcpkg)
else
check_vcpkg_itself: | lum_vcpkg
	$(info NATIVE VCPKG NOT FOUND, INSTALLED POCKET VERSION)
endif


vcpkg_installed: | check_vcpkg_itself
	@echo installing vcpkg dependencies. Please do not interrupt
	$(VCPKG_EXEC) install

# $(VCPKG_EXEC) clean --all
purge: 
	@echo %VCPKG_BUILD_TYPE%


#use when big changes happen to lum 
update: init clean | check_vcpkg_itself
	@echo updating vcpkg dependencies. Please do not interrupt
	$(info VCPKG IS FOUND AS $(VCPKG_EXEC))
	$(VCPKG_EXEC) install
	git submodule init
	git submodule update
	cd lum-al
	$(VCPKG_EXEC) install
	make library
	cd ..