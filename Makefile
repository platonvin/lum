.ONESHELL:

#setting up include and lib directories for dependencies
I = -Isrc -Icommon -Ilum-al/src
L = -Llum-al/lib

#linux is so good that static doesn't work
STATIC_OR_DYNAMIC = 
REQUIRED_LIBS = -llumal -lglfw3 -lvolk -lRmlDebugger -lRmlCore -lfreetype -lpng -lbrotlienc -lbrotlidec -lbrotlicommon -lpng16 -lz -lbz2
ifeq ($(OS),Windows_NT)
	REQUIRED_LIBS += -lgdi32       
	STATIC_OR_DYNAMIC += -static
else
	REQUIRED_LIBS += -lpthread -ldl
endif
	
always_enabled_flags = -fno-exceptions -Wuninitialized -std=c++20
debug_flags   = -O0 -g $(always_enabled_flags)
release_flags = -Ofast -DVKNDEBUG -mmmx -msse4.2 -mavx -mpclmul -fdata-sections -ffunction-sections -s -mfancy-math-387 -fno-math-errno -Wl,--gc-sections $(always_enabled_flags)
crazy_flags   = -flto -fopenmp -floop-parallelize-all -ftree-parallelize-loops=8 -D_GLIBCXX_PARALLEL -funroll-loops -w $(release_flags)

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

#only debug objects
deb_objs := $(patsubst src/%.cpp,obj/deb/%.o,$(filter src/%.cpp,$(srcs)))

#only release objects
rel_objs := $(patsubst src/%.cpp,obj/rel/%.o,$(filter src/%.cpp,$(srcs)))

com_objs := $(patsubst common/%.cpp,obj/%.o,$(filter common/%.cpp,$(srcs)))

#default target
all: init release
#for testing.
all: 
	@echo $(deb_objs)
	@echo $(rel_objs)
	@echo $(com_objs)

#rule for re-evaluation after vcpkg_installed created. 
#Eval needed because vcpkg_installed does not exist when they initially evaluated
.PHONY: vcpkg_installed_eval
vcpkg_installed_eval: vcpkg_installed
	$(eval OTHER_DIRS := $(filter-out vcpkg_installed/vcpkg, $(wildcard vcpkg_installed/*)) )
	$(eval INCLUDE_LIST := $(addsuffix /include, $(OTHER_DIRS)) )
	$(eval INCLUDE_LIST += $(addsuffix /include/vma, $(OTHER_DIRS)) )
	$(eval INCLUDE_LIST += $(addsuffix /include/volk, $(OTHER_DIRS)) )
	$(eval LIB_LIST := $(addsuffix /lib, $(OTHER_DIRS)) )
	$(eval I += $(addprefix -I, $(INCLUDE_LIST)) )
	$(eval L += $(addprefix -L, $(LIB_LIST)) )
	$(eval GLSLC_DIR := $(firstword $(foreach dir, $(OTHER_DIRS), $(wildcard $(dir)/tools/shaderc))) )
	$(eval GLSLC := $(strip $(GLSLC_DIR))/glslc )

#If someone knows a way to simplify this, please tell me 
setup: init vcpkg_installed_eval lum-al/lib/liblumal.a

obj/%.o: common/%.cpp | setup
	c++ $(special_otp_flags) $(always_enabled_flags) $(I) $(args) -MMD -MP -c $< -o $@
DEPS = $(com_objs:.o=.d)
-include $(DEPS)

# obj/rel/%.o: setup
obj/rel/%.o: src/%.cpp | setup
	c++ $(release_specific_flags) $(always_enabled_flags) $(I) $(args) -MMD -MP -c $< -o $@
DEPS = $(rel_objs:.o=.d)
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

$(SHADER_OUT_DIR)/%.spv: setup
$(SHADER_OUT_DIR)/%.spv: $(SHADER_SRC_DIR)/% $(SHADERS_EXTRA_DEPEND)
	$(GLSLC) -o $@ $< $(SHADER_FLAGS)

shaders: init $(SHADERS_EXTRA_DEPEND) $(_TARGETS)


debug: init shaders $(com_objs) $(deb_objs) build_deb 
ifeq ($(OS),Windows_NT)
	.\client
else
	./client
endif
	
release: setup shaders lum-al/lib/liblumal.a $(com_objs) $(rel_objs) build_rel 
ifeq ($(OS),Windows_NT)
	.\client
else
	./client
endif

#not separate on purpose. make cleanr before usage
release_p: args = -D_PRINTLINE
release_p: release

#not separate on purpose. make cleanr before usage
release_vfs: args = -DVSYNC_FULLSCREEN
release_vfs: release

lum-al/lib/liblumal.a: vcpkg_installed
	git submodule init
	git submodule update
	cd lum-al
	make library
	cd ..

#mostly for testing
only_build: init shaders lum-al/lib/liblumal.a $(com_objs) $(rel_objs) build_rel

only_build_unity:

#crazy fast
crazy: init shaders
	c++ $(srcs) -o crazy_client $(crazy_flags) $(I) $(L) $(REQUIRED_LIBS) $(STATIC_OR_DYNAMIC)
crazy_native: init shaders
	c++ $(srcs) -o crazy_client $(crazy_flags) -march=native $(I) $(L) $(REQUIRED_LIBS) $(STATIC_OR_DYNAMIC)

#i could not make it work without this. Maybe posssible with eval
build_deb: setup $(deb_objs) $(com_objs)
	c++ -o client $(deb_objs) $(com_objs) $(debug_flags) $(I) $(L) $(REQUIRED_LIBS) $(STATIC_OR_DYNAMIC)
build_rel: setup $(com_objs) $(rel_objs) 
	c++ -o client $(com_objs) $(rel_objs) $(release_flags) $(I) $(L) $(REQUIRED_LIBS) $(STATIC_OR_DYNAMIC)

fun:
	@echo -e '\033[0;36m' fun was never an option '\033[0m'
test:
	c++ -pg test.cpp -o test -Wl,--stack,1000000
	test

pack:
ifeq ($(OS),Windows_NT)
	mkdir "package"
	mkdir "package/shaders/compiled"
	mkdir "package/assets"
	copy "client" "package/client"
	copy "shaders/compiled" "package/shaders/compiled"
	copy "assets" "package/assets"
	powershell Compress-Archive -Update package package.zip
else
	mkdir -p package
	mkdir -p package/shaders/compiled
	mkdir -p package/assets
	cp client package/client
	cp -a /shaders/compiled /package/shaders/compiled
	cp -a /assets           /package/assets
	zip -r package.zip package
endif

# yeah windows wants quotes and backslashes so linux obviously wants no quotes and forward slashes. They have to be incompatible.
cleans: init
ifeq ($(OS),Windows_NT)
	-del "shaders\compiled\*.spv" 
else
	-rm -R shaders/compiled/*.spv
endif

cleand: init
ifeq ($(OS),Windows_NT)
	-del "obj\deb\*.o" 
else
	-rm -R obj/deb/*.o
endif

cleanr: init
ifeq ($(OS),Windows_NT)
	-del "obj\rel\*.o"  
else
	-rm -R obj/rel/*.o
endif

clean: init
ifeq ($(OS),Windows_NT)
	-del "obj\*.o"
	-del "obj\deb\*.o"
	-del "obj\rel\*.o"
	-del "shaders\compiled\*.spv"
	-del "lum-al\lib\*.a"
else
	-rm -R obj/*.o
	-rm -R obj/deb/*.o 
	-rm -R obj/rel/*.o 
	-rm -R shaders/compiled/*.spv 
	-rm -R lum-al/lib/*.a
endif

init: folders vcpkg_installed vcpkg_installed_eval lum-al/lib/liblumal.a
folders: obj/ obj/deb/ obj/rel/ shaders/compiled/ obj/deb/renderer/ obj/rel/renderer/
%/: #pattern rule to make all folders
ifeq ($(OS),Windows_NT)
	mkdir "$@"
else
	mkdir -p $@
endif

vcpkg_installed:
	@echo installing vcpkg dependencies. Please do not interrupt
	vcpkg install

#use when big changes happen to lum 
update: init clean
	@echo updating vcpkg dependencies. Please do not interrupt
	vcpkg install
	git submodule init
	git submodule update
	cd lum-al
	vcpkg install