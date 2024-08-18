.ONESHELL:

#libs are -lglfw3 -lglew32s -lopengl32 -lgdi32 -lccd -lenet64 -lws2_32 -lwinmm
# G++ = C:\msys64\mingw64\bin\g++ exe

I = -I./src -I${VULKAN_SDK}/Include -I./common -I${VCPKG_ROOT}/installed/x64-mingw-static/Include
L = -L${VULKAN_SDK}/Lib -L${VCPKG_ROOT}/installed/x64-mingw-static/lib
#-ftrivial-auto-var-init=zero sets "local" vars to 0 by default

# all of them united
always_enabled_flags = -pipe -fno-exceptions -Wuninitialized -ftrivial-auto-var-init=zero -Wl,--stack,1000000
debug_specific_flags   = -O0 
release_specific_flags = -O2 -DNDEBUG 

release_flags = $(release_specific_flags) $(always_enabled_flags) $(I) $(args) -c -o
  debug_flags = $(debug_specific_flags)   $(always_enabled_flags) $(I) $(args) -c -o

SHADER_FLAGS = --target-env=vulkan1.1 -g -O 
# SHADER_OPT_FLAGS = --merge-return --inline-entry-points-exhaustive --eliminate-dead-functions --scalar-replacement --eliminate-local-single-block --eliminate-local-single-store --simplify-instructions --vector-dce --eliminate-dead-inserts --eliminate-dead-code-aggressive --eliminate-dead-branches --merge-blocks --eliminate-local-multi-store --simplify-instructions --vector-dce --eliminate-dead-inserts --redundancy-elimination --eliminate-dead-code-aggressive --strip-debug
SHADER_OPT_FLAGS = --target-env=vulkan1.1

deb_objs := \
	obj/deb/main.o\
	obj/deb/engine.o\
	obj/deb/render.o\
	obj/deb/setup.o\
	obj/deb/load_stuff.o\
	obj/deb/render_ui_interface.o\
	obj/deb/ui.o\
	obj/ogt_vox.o\
	obj/ogt_voxel_meshify.o\
	obj/meshopt.o\

rel_objs := \
	obj/rel/main.o\
	obj/rel/engine.o\
	obj/rel/render.o\
	obj/rel/setup.o\
	obj/rel/load_stuff.o\
	obj/rel/render_ui_interface.o\
	obj/rel/ui.o\
	obj/ogt_vox.o\
	obj/ogt_voxel_meshify.o\
	obj/meshopt.o\

srcs := \
	src/main.cpp\
	src/engine.cpp\
	src/renderer/render.cpp\
	src/renderer/setup.cpp\
	src/renderer/load_stuff.cpp\
	src/renderer/render_ui_interface.cpp\
	src/renderer/ui.cpp\
	common/ogt_vox.cpp\
	common/ogt_voxel_meshify.cpp\
	common/meshopt.cpp\

# flags = 
all: Flags=$(release_specific_flags)
all: clean build
	@echo compiled
measure: build

obj/ogt_vox.o: common/ogt_vox.cpp common/ogt_vox.hpp
	g++ common/ogt_vox.cpp -Ofast -c -o obj/ogt_vox.o -pipe -fno-exceptions -Wuninitialized -ftrivial-auto-var-init=zero $(I) $(args)
obj/ogt_voxel_meshify.o: common/ogt_voxel_meshify.cpp common/ogt_voxel_meshify.hpp common/meshopt.hpp
	g++ common/ogt_voxel_meshify.cpp -Ofast -c -o obj/ogt_voxel_meshify.o -pipe -fno-exceptions -Wuninitialized -ftrivial-auto-var-init=zero $(I) $(args)
obj/meshopt.o: common/meshopt.cpp common/meshopt.hpp
	g++ common/meshopt.cpp -Ofast -c -o obj/meshopt.o -pipe -fno-exceptions -Wuninitialized -ftrivial-auto-var-init=zero $(I) $(args)

obj/deb/engine.o: src/engine.cpp src/engine.hpp src/renderer/render.cpp src/renderer/render.hpp src/renderer/ui.hpp common/defines.hpp
	g++ src/engine.cpp $(Flags) obj/deb/engine.o
obj/deb/render.o: src/renderer/render.cpp src/renderer/render.hpp common/defines.hpp
	g++ src/renderer/render.cpp $(Flags) obj/deb/render.o 
obj/deb/setup.o: src/renderer/setup.cpp src/renderer/render.hpp common/defines.hpp
	g++ src/renderer/setup.cpp $(Flags) obj/deb/setup.o
obj/deb/load_stuff.o: src/renderer/load_stuff.cpp src/renderer/render.hpp common/defines.hpp
	g++ src/renderer/load_stuff.cpp $(Flags) obj/deb/load_stuff.o
obj/deb/render_ui_interface.o: src/renderer/render_ui_interface.cpp src/renderer/render.hpp common/defines.hpp
	g++ src/renderer/render_ui_interface.cpp $(Flags) obj/deb/render_ui_interface.o
obj/deb/ui.o: src/renderer/ui.cpp src/renderer/ui.hpp src/renderer/render.hpp
	g++ src/renderer/ui.cpp $(Flags) obj/deb/ui.o
.PHONY: obj/deb/main.o
obj/deb/main.o:
	g++ src/main.cpp $(Flags) obj/deb/main.o

obj/rel/engine.o: src/engine.cpp src/engine.hpp src/renderer/render.cpp src/renderer/render.hpp src/renderer/ui.hpp common/defines.hpp
	g++ src/engine.cpp $(Flags) obj/rel/engine.o
obj/rel/render.o: src/renderer/render.cpp src/renderer/render.hpp common/defines.hpp
	g++ src/renderer/render.cpp $(Flags) obj/rel/render.o
obj/rel/setup.o: src/renderer/setup.cpp src/renderer/render.hpp common/defines.hpp
	g++ src/renderer/setup.cpp $(Flags) obj/rel/setup.o
obj/rel/load_stuff.o: src/renderer/load_stuff.cpp src/renderer/render.hpp common/defines.hpp
	g++ src/renderer/load_stuff.cpp $(Flags) obj/rel/load_stuff.o
obj/rel/render_ui_interface.o: src/renderer/render_ui_interface.cpp src/renderer/render.hpp common/defines.hpp
	g++ src/renderer/render_ui_interface.cpp $(Flags) obj/rel/render_ui_interface.o
obj/rel/ui.o: src/renderer/ui.cpp src/renderer/ui.hpp src/renderer/render.hpp
	g++ src/renderer/ui.cpp $(Flags) obj/rel/ui.o
# .PHONY: obj/rel/main.o
obj/rel/main.o: src/main.cpp src/engine.hpp src/renderer/render.hpp src/renderer/ui.hpp common/defines.hpp
	g++ src/main.cpp $(Flags) obj/rel/main.o


build_deb: $(deb_objs)
	g++ $(deb_objs) -o client.exe $(always_enabled_flags) $(I) $(L) -l:libglfw3.a -lgdi32 -l:volk.lib -lRmlDebugger -lRmlCore -lfreetype -lpng -lbrotlienc -lbrotlidec -lbrotlicommon -lpng16 -lz -lbz2 -static
build_rel: $(rel_objs)
	g++ $(rel_objs) -o client.exe $(always_enabled_flags) $(I) $(L) -l:libglfw3.a -lgdi32 -l:volk.lib -lRmlDebugger -lRmlCore -lfreetype -lpng -lbrotlienc -lbrotlidec -lbrotlicommon -lpng16 -lz -lbz2 -static

# client_opt:
# 	g++ $(srcs) $(I) $(L) $(D) -l:libglfw3.a -lgdi32 -l:volk.lib -lRmlCore -lRmlDebugger -lRmlCore -lfreetype -lpng -lbrotlienc -lbrotlidec -lbrotlicommon -lpng16 -lz -lbz2 -static -Os -pipe -fno-exceptions -fdata-sections -ffunction-sections -o client.exe -s -fno-stack-protector -fomit-frame-pointer -fmerge-all-constants -momit-leaf-frame-pointer -mfancy-math-387 -fno-math-errno -Wl,--gc-sections $(args)

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

COMP_TARGETS = $(patsubst $(SHADER_SRC_DIR)/%$(COMP_EXT), $(SHADER_OUT_DIR)/%.spv, $(COMP_SHADERS))
VERT_TARGETS = $(patsubst $(SHADER_SRC_DIR)/%$(VERT_EXT), $(SHADER_OUT_DIR)/%Vert.spv, $(VERT_SHADERS))
FRAG_TARGETS = $(patsubst $(SHADER_SRC_DIR)/%$(FRAG_EXT), $(SHADER_OUT_DIR)/%Frag.spv, $(FRAG_SHADERS))
GEOM_TARGETS = $(patsubst $(SHADER_SRC_DIR)/%$(GEOM_EXT), $(SHADER_OUT_DIR)/%Geom.spv, $(GEOM_SHADERS))

ALL_SHADER_TARGETS = $(COMP_TARGETS) $(VERT_TARGETS) $(FRAG_TARGETS) $(GEOM_TARGETS)

$(SHADER_OUT_DIR)/%.spv: $(SHADER_SRC_DIR)/%$(COMP_EXT)
	glslc -o $(SHADER_OUT_DIR)/$*_unopt.spv $< $(SHADER_FLAGS)
	spirv-opt -o $@ $(SHADER_OUT_DIR)/$*_unopt.spv $(SHADER_OPT_FLAGS)
	del "$(SHADER_OUT_DIR)\$*_unopt.spv"
$(SHADER_OUT_DIR)/%Vert.spv: $(SHADER_SRC_DIR)/%$(VERT_EXT)
	glslc -o $(SHADER_OUT_DIR)/$*Vert_unopt.spv $< $(SHADER_FLAGS)
	spirv-opt -o $@ $(SHADER_OUT_DIR)/$*Vert_unopt.spv $(SHADER_OPT_FLAGS)
	del "$(SHADER_OUT_DIR)\$*Vert_unopt.spv"
$(SHADER_OUT_DIR)/%Frag.spv: $(SHADER_SRC_DIR)/%$(FRAG_EXT)
	glslc -o $(SHADER_OUT_DIR)/$*Frag_unopt.spv $< $(SHADER_FLAGS)
	spirv-opt -o $@ $(SHADER_OUT_DIR)/$*Frag_unopt.spv $(SHADER_OPT_FLAGS)
	del "$(SHADER_OUT_DIR)\$*Frag_unopt.spv"
$(SHADER_OUT_DIR)/%Geom.spv: $(SHADER_SRC_DIR)/%$(GEOM_EXT)
	glslc -o $(SHADER_OUT_DIR)/$*Geom_unopt.spv $< $(SHADER_FLAGS)
	spirv-opt -o $@ $(SHADER_OUT_DIR)/$*Geom_unopt.spv $(SHADER_OPT_FLAGS)
	del "$(SHADER_OUT_DIR)\$*Geom_unopt.spv"

shaders: $(ALL_SHADER_TARGETS)

debug: Flags=$(debug_flags) 
debug: shaders build_deb
	client.exe
release: Flags=$(release_flags)
release: shaders build_rel
	client.exe

fun:
	@echo fun was never an option
test:
	g++ test.cpp -o test
	test

pack:
	mkdir "package"
	mkdir "package/shaders/compiled"
	mkdir "package/assets"
	copy "client.exe" "package/client.exe"
	copy "shaders/compiled" "package/shaders/compiled"
	copy "assets" "package/assets"
	powershell Compress-Archive -Update package package.zip
clean:
	del "obj\*.o" 
	del "obj\deb\*.o" 
	del "obj\rel\*.o" 
	del "shaders\compiled\*.spv" 
cleans:
	del "shaders\compiled\*.spv" 
init:
	mkdir obj
	mkdir obj\deb
	mkdir obj\rel
	mkdir shaders\compiled