.ONESHELL:

#setting up include and lib directories for dependencies
I = -Isrc -Icommon -Ilum-al/src
L = -Llum-al/lib

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


deb_objs := \
	obj/deb/main.o\
	obj/deb/engine.o\
	obj/deb/render.o\
	obj/deb/setup.o\
	obj/deb/load_stuff.o\
	obj/deb/render_ui_interface.o\
	obj/deb/ui.o\
	obj/deb/ao_lut.o\

rel_objs := \
	obj/rel/main.o\
	obj/rel/engine.o\
	obj/rel/render.o\
	obj/rel/setup.o\
	obj/rel/load_stuff.o\
	obj/rel/render_ui_interface.o\
	obj/rel/ui.o\
	obj/rel/ao_lut.o\

com_objs := \
	obj/ogt_vox.o\
	obj/ogt_voxel_meshify.o\
	obj/meshopt.o\

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

#default target
all: init vcpkg_installed_eval lum-al/lib/liblumal.a release

#rule for re-evaluation after vcpkg_installed created
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
obj/%.o: common/%.cpp init vcpkg_installed_eval lum-al/lib/liblumal.a
	c++ $(special_otp_flags) $(always_enabled_flags) $(I) $(args) -MMD -MP -c $< -o $@
DEPS = $(com_objs:.o=.d)
-include $(DEPS)

obj/rel/%.o: src/%.cpp init vcpkg_installed_eval lum-al/lib/liblumal.a
	c++ $(release_specific_flags) $(always_enabled_flags) $(I) $(args) -MMD -MP -c $< -o $@
obj/rel/%.o: src/renderer/%.cpp init vcpkg_installed_eval lum-al/lib/liblumal.a
	c++ $(release_specific_flags) $(always_enabled_flags) $(I) $(args) -MMD -MP -c $< -o $@
DEPS = $(rel_objs:.o=.d)
-include $(DEPS)

obj/deb/%.o: src/%.cpp init vcpkg_installed_eval lum-al/lib/liblumal.a
	c++ $(debug_specific_flags) $(always_enabled_flags) $(I) $(args) -MMD -MP -c $< -o $@
obj/deb/%.o: src/renderer/%.cpp init vcpkg_installed_eval lum-al/lib/liblumal.a
	c++ $(debug_specific_flags) $(always_enabled_flags) $(I) $(args) -MMD -MP -c $< -o $@
DEPS = $(deb_objs:.o=.d)
-include $(DEPS)

SHADER_SRC_DIR = shaders
SHADER_OUT_DIR = shaders/compiled

COMP_EXT = .comp
VERT_EXT = .vert
FRAG_EXT = .frag
GEOM_EXT = .geom

COMP_SHADERS = $(wildcard $(SHADER_SRC_DIR)/*$(COMP_EXT))
VERT_SHADERS = $(wildcard $(SHADER_SRC_DIR)/*$(VERT_EXT))
FRAG_SHADERS = $(wildcard $(SHADER_SRC_DIR)/*$(FRAG_EXT))
GEOM_SHADERS = $(wildcard $(SHADER_SRC_DIR)/*$(GEOM_EXT))
SHADERS = $(wildcard $(SHADER_SRC_DIR)/*$(GEOM_EXT))

COMP_TARGETS = $(patsubst $(SHADER_SRC_DIR)/%$(COMP_EXT), $(SHADER_OUT_DIR)/%.spv, $(COMP_SHADERS))
VERT_TARGETS = $(patsubst $(SHADER_SRC_DIR)/%$(VERT_EXT), $(SHADER_OUT_DIR)/%Vert.spv, $(VERT_SHADERS))
FRAG_TARGETS = $(patsubst $(SHADER_SRC_DIR)/%$(FRAG_EXT), $(SHADER_OUT_DIR)/%Frag.spv, $(FRAG_SHADERS))
GEOM_TARGETS = $(patsubst $(SHADER_SRC_DIR)/%$(GEOM_EXT), $(SHADER_OUT_DIR)/%Geom.spv, $(GEOM_SHADERS))

ALL_SHADER_TARGETS = $(COMP_TARGETS) $(VERT_TARGETS) $(FRAG_TARGETS) $(GEOM_TARGETS)

$(SHADER_OUT_DIR)/%.spv: $(SHADER_SRC_DIR)/%$(COMP_EXT)
	$(GLSLC) -o $@ $< $(SHADER_FLAGS)
$(SHADER_OUT_DIR)/%Vert.spv: $(SHADER_SRC_DIR)/%$(VERT_EXT)
	$(GLSLC) -o $@ $< $(SHADER_FLAGS)
$(SHADER_OUT_DIR)/%Frag.spv: $(SHADER_SRC_DIR)/%$(FRAG_EXT)
	$(GLSLC) -o $@ $< $(SHADER_FLAGS)
$(SHADER_OUT_DIR)/%Geom.spv: $(SHADER_SRC_DIR)/%$(GEOM_EXT)
	$(GLSLC) -o $@ $< $(SHADER_FLAGS)

shaders: vcpkg_installed_eval $(ALL_SHADER_TARGETS)


debug: init vcpkg_installed_eval shaders $(com_objs) $(deb_objs) build_deb 
ifeq ($(OS),Windows_NT)
	.\client
else
	./client
endif
	
release: init vcpkg_installed_eval shaders lum-al/lib/liblumal.a $(com_objs) $(rel_objs) build_rel 
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
only_build: init vcpkg_installed_eval shaders lum-al/lib/liblumal.a $(com_objs) $(rel_objs) build_rel

#crazy fast
crazy: init vcpkg_installed_eval shaders
	c++ $(srcs) -o crazy_client $(crazy_flags) $(I) $(L) $(REQUIRED_LIBS) $(STATIC_OR_DYNAMIC)
crazy_native: init vcpkg_installed_eval shaders
	c++ $(srcs) -o crazy_client $(crazy_flags) -march=native $(I) $(L) $(REQUIRED_LIBS) $(STATIC_OR_DYNAMIC)

#i could not make it work without this
build_deb: init vcpkg_installed_eval lum-al/lib/liblumal.a $(deb_objs) $(com_objs)
	c++ -o client $(deb_objs) $(com_objs) $(debug_flags) $(I) $(L) $(REQUIRED_LIBS) $(STATIC_OR_DYNAMIC)
build_rel: init vcpkg_installed_eval lum-al/lib/liblumal.a $(com_objs) $(rel_objs) 
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

# mkdir obj
init: obj obj/deb obj/rel shaders/compiled
obj:
ifeq ($(OS),Windows_NT)
	mkdir "obj"
else
	mkdir -p obj
endif

obj/deb:
ifeq ($(OS),Windows_NT)
	mkdir "obj/deb"
else
	mkdir -p obj/deb
endif

obj/rel:
ifeq ($(OS),Windows_NT)
	mkdir "obj/rel"
else
	mkdir -p obj/rel
endif

shaders/compiled:
ifeq ($(OS),Windows_NT)
	mkdir "shaders/compiled"
else
	mkdir -p shaders/compiled
endif

vcpkg_installed:
	echo installing vcpkg dependencies. Please do not interrupt
	vcpkg install

#use when big changes happen to lum 
update: init clean
	echo updating vcpkg dependencies. Please do not interrupt
	vcpkg install
	git submodule init
	git submodule update
	cd lum-al
	vcpkg install