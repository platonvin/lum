.ONESHELL:

#setting up include and lib directories for dependencies
I = -Isrc -Icommon -Ilum-al/src
L = -Llum-al/lib

#Linux is so good that static doesn't work
STATIC_OR_DYNAMIC = 
#libs that i do not include in internal-unity-build
#Windows is so good that lum_vcpkg/vcpkg.exe is not executable
#if anyone knows good way to solve "\" "/" ".exe" ".\" "./" problem - please tell me
EXTERNAL_LIBS = -lglfw3 -lvolk -lRmlDebugger -lRmlCore -lfreetype -lpng -lbrotlienc -lbrotlidec -lbrotlicommon -lpng16 -lz -lbz2
ifeq ($(OS),Windows_NT)
	EXTERNAL_LIBS += -lgdi32       
	STATIC_OR_DYNAMIC += -static
	WHICH_WHERE = where
	RUN_POSTFIX = .exe
	RUN_PREFIX = .\\
	SLASH = \\
else
	EXTERNAL_LIBS += -lpthread -ldl
	RUN_PREFIX = ./
	WHICH_WHERE = which
	RUN_POSTFIX = 
	SLASH = /
endif
REQUIRED_LIBS += $(EXTERNAL_LIBS)
REQUIRED_LIBS += -llumal
VCPKG_EXECUTABLE := $(RUN_PREFIX)lum_vcpkg$(SLASH)vcpkg$(RUN_POSTFIX) --vcpkg-root=lum_vcpkg
	
always_enabled_flags = -fno-exceptions -Wuninitialized -std=c++20
# debug build
debug_flags   = $(always_enabled_flags) -O0 -g 
common_instructions := -mmmx -msse -msse2 -msse3 -mssse3 -msse4.1 -msse4.2 -mcx16 -mavx -mpclmul
# common_instructions := -march=native oops TODO: sep
# incremental development build
dev_flags = $(always_enabled_flags) -O2 -DVKNDEBUG $(common_instructions) 
# unity build
rel_flags = $(always_enabled_flags) -Ofast $(common_instructions) -s -Wl,--gc-sections 

SHADER_FLAGS = --target-env=vulkan1.1 -g -O

#all source files. Separate for convinience and unity build 
srcs := \
	src/main.cpp\
	src/engine.cpp\
	src/renderer/render.cpp\
	src/renderer/ao_lut.cpp\
	src/renderer/setup.cpp\
	src/renderer/load_stuff.cpp\
	src/renderer/render_ui_interface.cpp\
	src/renderer/ui.cpp\
	common/ogt_vox.cpp\
	common/ogt_voxel_meshify.cpp\
	common/meshopt.cpp\

#all src files included
UNITY_FILE := src/unity.cpp

#only debug objects
deb_objs := $(patsubst src/%.cpp,obj/deb/%.o,$(filter src/%.cpp,$(srcs)))

#only dev objects
dev_objs := $(patsubst src/%.cpp,obj/rel/%.o,$(filter src/%.cpp,$(srcs)))

com_objs := $(patsubst common/%.cpp,obj/%.o,$(filter common/%.cpp,$(srcs)))

#default target
all: init rel
#for testing.
# all: 
# 	@echo $(deb_objs)
# 	@echo $(dev_objs)
# 	@echo $(com_objs)

#rule for re-evaluation after vcpkg_installed created. 
#Eval needed because vcpkg_installed does not exist when they initially evaluated
.PHONY: vcpkg_installed_eval
vcpkg_installed_eval: vcpkg_installed | check_vcpkg_itself
	$(eval OTHER_DIRS := $(filter-out vcpkg_installed/vcpkg, $(wildcard vcpkg_installed/*)) )
	$(eval INCLUDE_LIST := $(addsuffix /include, $(OTHER_DIRS)) )
	$(eval INCLUDE_LIST += $(addsuffix /include/vma, $(OTHER_DIRS)) )
	$(eval INCLUDE_LIST += $(addsuffix /include/volk, $(OTHER_DIRS)) )
	$(eval LIB_LIST := $(addsuffix /lib, $(OTHER_DIRS)) )
	$(eval I += $(addprefix -I, $(INCLUDE_LIST)) )
	$(eval L += $(addprefix -L, $(LIB_LIST)) )
	$(eval GLSLC_DIR := $(firstword $(foreach dir, $(OTHER_DIRS), $(wildcard $(dir)/tools/shaderc))) )
	$(eval GLSLC := $(strip $(GLSLC_DIR))/glslc )

setup: init vcpkg_installed_eval lum-al/lib/liblumal.a

#If someone knows a way to simplify this, please tell me 
obj/%.o: common/%.cpp | setup
	c++ $(dev_flags) $(I) $(args) -MMD -MP -c $< -o $@
DEPS = $(com_objs:.o=.d)
-include $(DEPS)

# obj/rel/%.o: setup
obj/rel/%.o: src/%.cpp | setup
	c++ $(dev_flags) $(I) $(args) -MMD -MP -c $< -o $@
DEPS = $(dev_objs:.o=.d)
-include $(DEPS)

# obj/deb/%.o: setup
obj/deb/%.o: src/%.cpp | setup
	c++ $(debug_specific_flags) $(always_enabled_flags) $(I) $(args) -MMD -MP -c $< -o $@
DEPS = $(deb_objs:.o=.d)
-include $(DEPS)

SHADER_SRC_DIR = shaders
SHADER_OUT_DIR = shaders/compiled
SHADERS_EXTRA_DEPEND = \

_SHADERS += $(wildcard $(SHADER_SRC_DIR)/*.vert)
_SHADERS += $(wildcard $(SHADER_SRC_DIR)/*.frag)
_SHADERS += $(wildcard $(SHADER_SRC_DIR)/*.comp)
_SHADERS += $(wildcard $(SHADER_SRC_DIR)/*.geom)
_TARGETS = $(patsubst $(SHADER_SRC_DIR)/%, $(SHADER_OUT_DIR)/%.spv, $(_SHADERS))

# spirv-opt optimized version. It is ~0.002% faster
# $(SHADER_OUT_DIR)/%.spv.temp: $(SHADER_SRC_DIR)/% $(SHADERS_EXTRA_DEPEND)
# 	$(GLSLC) -o $@ $< $(SHADER_FLAGS)
# $(SHADER_OUT_DIR)/%.spv: $(SHADER_OUT_DIR)/%.spv.temp
# 	spirv-opt $< -o $@ -O

$(SHADER_OUT_DIR)/%.spv: $(SHADER_SRC_DIR)/% $(SHADERS_EXTRA_DEPEND) | vcpkg_installed_eval
	$(GLSLC) -o $@ $< $(SHADER_FLAGS)

shaders: init $(SHADERS_EXTRA_DEPEND) $(_TARGETS)

debug: init shaders $(com_objs) $(deb_objs) build_deb 
	$(RUN_PREFIX)bin/client_deb$(RUN_POSTFIX)

dev: setup shaders $(com_objs) $(dev_objs) build_dev 
	$(RUN_PREFIX)bin/client_dev$(RUN_POSTFIX)

rel: setup shaders build_unity 
	$(RUN_PREFIX)bin/client_rel$(RUN_POSTFIX)

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
only_build: init shaders lum-al/lib/liblumal.a $(com_objs) $(dev_objs) build_dev

only_build_unity: init shaders build_unity

#i could not make it work without this. Maybe posssible with eval
build_deb: setup $(deb_objs) $(com_objs)
	c++ -o bin/client_deb $(deb_objs) $(com_objs) $(debug_flags) $(I) $(L) $(REQUIRED_LIBS) $(STATIC_OR_DYNAMIC)
build_dev: setup $(com_objs) $(dev_objs) 
	c++ -o bin/client_dev $(com_objs) $(dev_objs) $(dev_flags) $(I) $(L) $(REQUIRED_LIBS) $(STATIC_OR_DYNAMIC)
build_unity: setup
	c++ -o bin/client_rel $(UNITY_FILE) $(rel_flags) $(I) $(L) $(EXTERNAL_LIBS) $(STATIC_OR_DYNAMIC)

fun:
	@echo -e '\033[0;36m' fun was never an option '\033[0m'
test:
	c++ -pg test.cpp -o test -Wl,--stack,1000000
	test

#not sure if it works on non-Windows
pack:
ifeq ($(OS),Windows_NT)
	-mkdir "package"
	-mkdir "package\shaders\compiled"
	-mkdir "package\assets"
	-xcopy /s /i /y ".\bin\client_rel.exe" ".\package\client.exe*"
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
	-del "obj\deb\render\*.o"  
else
	-rm -R obj/deb/*.o
	-rm -R obj/deb/render/*.o
endif

cleanr: folders
ifeq ($(OS),Windows_NT)
	-del "obj\rel\*.o"  
	-del "obj\rel\render\*.o"  
else
	-rm -R obj/rel/*.o
	-rm -R obj/rel/render/*.o
endif

clean: folders
ifeq ($(OS),Windows_NT)
	-del "obj\*.o"
	-del "obj\deb\*.o"
	-del "obj\rel\*.o"
	-del "obj\deb\render\*.o"  
	-del "obj\rel\render\*.o"  
	-del "shaders\compiled\*.spv"
	-del "lum-al\lib\*.a"
else
	-rm -R obj/*.o
	-rm -R obj/deb/*.o 
	-rm -R obj/rel/*.o 
	-rm -R obj/deb/render/*.o
	-rm -R obj/rel/render/*.o
	-rm -R shaders/compiled/*.spv 
	-rm -R lum-al/lib/*.a
endif

init: folders vcpkg_installed vcpkg_installed_eval lum-al/lib/liblumal.a
folders: obj/ obj/deb/ obj/rel/ shaders/compiled/ obj/deb/renderer/ obj/rel/renderer/ bin/
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
	$(eval VCPKG_EXECUTABLE := vcpkg)
else
check_vcpkg_itself: | lum_vcpkg
	$(info NATIVE VCPKG NOT FOUND, INSTALLED POCKET VERSION)
endif


vcpkg_installed: | check_vcpkg_itself
	@echo installing vcpkg dependencies. Please do not interrupt
	$(VCPKG_EXECUTABLE) install

#use when big changes happen to lum 
update: init clean | check_vcpkg_itself
	@echo updating vcpkg dependencies. Please do not interrupt
	$(info VCPKG IS FOUND AS $(VCPKG_EXECUTABLE))
	$(VCPKG_EXECUTABLE) install
	git submodule init
	git submodule update
	cd lum-al
	$(VCPKG_EXECUTABLE) install
	make library
	cd ..