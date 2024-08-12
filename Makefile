.ONESHELL:

#libs are -lglfw3 -lglew32s -lopengl32 -lgdi32 -lccd -lenet64 -lws2_32 -lwinmm
# G++ = C:\msys64\mingw64\bin\g++ exe

I = -I./src -I${VULKAN_SDK}/Include -I./common -I${VCPKG_ROOT}/installed/x64-mingw-static/Include
L = -L${VULKAN_SDK}/Lib -L${VCPKG_ROOT}/installed/x64-mingw-static/lib
#-ftrivial-auto-var-init=zero sets "local" vars to 0 by default

# all of them united
always_enabled_flags = -pipe -fno-exceptions -Wuninitialized -ftrivial-auto-var-init=zero
debug_specific_flags   = -O0 
release_specific_flags = -O2 -DNDEBUG 

release_flags = $(release_specific_flags) $(always_enabled_flags) $(I) $(args) -c -o
  debug_flags = $(debug_specific_flags)   $(always_enabled_flags) $(I) $(args) -c -o

SHADER_FLAGS = --target-env=vulkan1.1 -g

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

# headers:= \
# 	src/renderer/render.hpp\
# 	src/renderer/ui.hpp\
# 	common/ogt_vox.hpp\
# 	common/meshopt.hpp\
# 	common/ogt_voxel_meshify.hpp\
# 	common/engine.hpp\

_shaders:= \
	shaders/compiled/map.spv\
	shaders/compiled/radiance.spv\
	shaders/compiled/blurVert.spv\
	shaders/compiled/blurFrag.spv\
	shaders/compiled/rayGenVert.spv\
	shaders/compiled/rayGenFrag.spv\
	shaders/compiled/rayGenParticlesVert.spv\
	shaders/compiled/rayGenParticlesGeom.spv\
	shaders/compiled/diffuseVert.spv\
	shaders/compiled/diffuseFrag.spv\
	shaders/compiled/fillStencilGlossyVert.spv\
	shaders/compiled/fillStencilGlossyFrag.spv\
	shaders/compiled/fillStencilSmokeVert.spv\
	shaders/compiled/fillStencilSmokeFrag.spv\
	shaders/compiled/smokeVert.spv\
	shaders/compiled/smokeFrag.spv\
	shaders/compiled/glossyVert.spv\
	shaders/compiled/glossyFrag.spv\
	shaders/compiled/overlayVert.spv\
	shaders/compiled/overlayFrag.spv\
	shaders/compiled/grassVert.spv\
	shaders/compiled/grassFrag.spv\
	shaders/compiled/waterVert.spv\
	shaders/compiled/updateGrass.spv\
	shaders/compiled/perlin2.spv\
	shaders/compiled/perlin3.spv\
	shaders/compiled/dfx.spv\
	shaders/compiled/dfy.spv\
	shaders/compiled/dfz.spv\
	shaders/compiled/bitmask.spv\

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
obj/rel/main.o: src/main.cpp src/renderer/render.hpp src/renderer/ui.hpp common/defines.hpp
	g++ src/main.cpp $(Flags) obj/rel/main.o


build_deb: $(deb_objs) $(_shaders)
	g++ $(deb_objs) -o client.exe $(always_enabled_flags) $(I) $(L) -l:libglfw3.a -lgdi32 -l:volk.lib -lRmlDebugger -lRmlCore -lfreetype -lpng -lbrotlienc -lbrotlidec -lbrotlicommon -lpng16 -lz -lbz2 -static
build_rel: $(rel_objs) $(_shaders)
	g++ $(rel_objs) -o client.exe $(always_enabled_flags) $(I) $(L) -l:libglfw3.a -lgdi32 -l:volk.lib -lRmlDebugger -lRmlCore -lfreetype -lpng -lbrotlienc -lbrotlidec -lbrotlicommon -lpng16 -lz -lbz2 -static

# client_opt:
# 	g++ $(srcs) $(I) $(L) $(D) -l:libglfw3.a -lgdi32 -l:volk.lib -lRmlCore -lRmlDebugger -lRmlCore -lfreetype -lpng -lbrotlienc -lbrotlidec -lbrotlicommon -lpng16 -lz -lbz2 -static -Os -pipe -fno-exceptions -fdata-sections -ffunction-sections -o client.exe -s -fno-stack-protector -fomit-frame-pointer -fmerge-all-constants -momit-leaf-frame-pointer -mfancy-math-387 -fno-math-errno -Wl,--gc-sections $(args)

shaders/compiled/map.spv: shaders/map.comp 
	glslc shaders/map.comp -o shaders/compiled/map.spv $(SHADER_FLAGS)
shaders/compiled/radiance.spv: shaders/radiance.comp
	glslc shaders/radiance.comp -o shaders/compiled/radiance.spv $(SHADER_FLAGS)
shaders/compiled/blurVert.spv: shaders/blur.vert 
	glslc shaders/blur.vert -o shaders/compiled/blurVert.spv $(SHADER_FLAGS)
shaders/compiled/blurFrag.spv: shaders/blur.frag 
	glslc shaders/blur.frag -o shaders/compiled/blurFrag.spv $(SHADER_FLAGS)
shaders/compiled/rayGenVert.spv: shaders/rayGen.vert 
	glslc shaders/rayGen.vert -o shaders/compiled/rayGenVert.spv $(SHADER_FLAGS)
shaders/compiled/rayGenFrag.spv: shaders/rayGen.frag 
	glslc shaders/rayGen.frag -o shaders/compiled/rayGenFrag.spv $(SHADER_FLAGS)
shaders/compiled/rayGenParticlesVert.spv: shaders/rayGenParticles.vert 
	glslc shaders/rayGenParticles.vert -o shaders/compiled/rayGenParticlesVert.spv $(SHADER_FLAGS)
shaders/compiled/rayGenParticlesGeom.spv: shaders/rayGenParticles.geom 
	glslc shaders/rayGenParticles.geom -o shaders/compiled/rayGenParticlesGeom.spv $(SHADER_FLAGS)
shaders/compiled/diffuseVert.spv: shaders/diffuse.vert 
	glslc shaders/diffuse.vert -o shaders/compiled/diffuseVert.spv $(SHADER_FLAGS)
shaders/compiled/diffuseFrag.spv: shaders/diffuse.frag 
	glslc shaders/diffuse.frag -o shaders/compiled/diffuseFrag.spv $(SHADER_FLAGS)
shaders/compiled/glossyVert.spv: shaders/glossy.vert 
	glslc shaders/glossy.vert -o shaders/compiled/glossyVert.spv $(SHADER_FLAGS)
shaders/compiled/glossyFrag.spv: shaders/glossy.frag 
	glslc shaders/glossy.frag -o shaders/compiled/glossyFrag.spv $(SHADER_FLAGS)
shaders/compiled/overlayVert.spv: shaders/overlay.vert 
	glslc shaders/overlay.vert -o shaders/compiled/overlayVert.spv $(SHADER_FLAGS)
shaders/compiled/overlayFrag.spv: shaders/overlay.frag  
	glslc shaders/overlay.frag -o shaders/compiled/overlayFrag.spv $(SHADER_FLAGS)
shaders/compiled/grassVert.spv: shaders/grass.vert
	glslc shaders/grass.vert -o shaders/compiled/grassVert.spv $(SHADER_FLAGS)
shaders/compiled/grassFrag.spv: shaders/grass.frag
	glslc shaders/grass.frag -o shaders/compiled/grassFrag.spv $(SHADER_FLAGS)
shaders/compiled/waterVert.spv: shaders/water.vert
	glslc shaders/water.vert -o shaders/compiled/waterVert.spv $(SHADER_FLAGS)
shaders/compiled/updateGrass.spv: shaders/updateGrass.comp
	glslc shaders/updateGrass.comp -o shaders/compiled/updateGrass.spv $(SHADER_FLAGS)
shaders/compiled/perlin2.spv: shaders/perlin2.comp
	glslc shaders/perlin2.comp -o shaders/compiled/perlin2.spv $(SHADER_FLAGS)
shaders/compiled/perlin3.spv: shaders/perlin3.comp
	glslc shaders/perlin3.comp -o shaders/compiled/perlin3.spv $(SHADER_FLAGS)
shaders/compiled/dfx.spv: shaders/dfx.comp
	glslc shaders/dfx.comp -o shaders/compiled/dfx.spv $(SHADER_FLAGS)
shaders/compiled/dfy.spv: shaders/dfy.comp
	glslc shaders/dfy.comp -o shaders/compiled/dfy.spv $(SHADER_FLAGS)
shaders/compiled/dfz.spv: shaders/dfz.comp
	glslc shaders/dfz.comp -o shaders/compiled/dfz.spv $(SHADER_FLAGS)
shaders/compiled/bitmask.spv: shaders/bitmask.comp
	glslc shaders/bitmask.comp -o shaders/compiled/bitmask.spv $(SHADER_FLAGS)
shaders/compiled/fillStencilGlossyVert.spv: shaders/fillStencilGlossy.vert
	glslc shaders/fillStencilGlossy.vert -o shaders/compiled/fillStencilGlossyVert.spv $(SHADER_FLAGS)
shaders/compiled/fillStencilGlossyFrag.spv: shaders/fillStencilGlossy.frag
	glslc shaders/fillStencilGlossy.frag -o shaders/compiled/fillStencilGlossyFrag.spv $(SHADER_FLAGS)
shaders/compiled/fillStencilSmokeVert.spv: shaders/fillStencilSmoke.vert
	glslc shaders/fillStencilSmoke.vert -o shaders/compiled/fillStencilSmokeVert.spv $(SHADER_FLAGS)
shaders/compiled/fillStencilSmokeFrag.spv: shaders/fillStencilSmoke.frag
	glslc shaders/fillStencilSmoke.frag -o shaders/compiled/fillStencilSmokeFrag.spv $(SHADER_FLAGS)
shaders/compiled/smokeVert.spv: shaders/smoke.vert
	glslc shaders/smoke.vert -o shaders/compiled/smokeVert.spv $(SHADER_FLAGS)
shaders/compiled/smokeFrag.spv: shaders/smoke.frag
	glslc shaders/smoke.frag -o shaders/compiled/smokeFrag.spv $(SHADER_FLAGS)

shaders: $(_shaders)

debug: Flags=$(debug_flags) 
# debug: objs=$(deb_objs)
debug: build_deb
	client.exe
release: Flags=$(release_flags)
# release: objs=$(rel_objs)
release: build_rel
	client.exe

fun:
	@echo fun was never an option
# opt: client_opt
# client.exe
# g++ test.cpp -o test
# test
obj_folder = obj/rel
test: $(obj_dir)/aa.o 

# das=$(addprefix $(obj_dir), "main.o")
# das = main.o
# @echo $(das)
# @echo das?



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
	
init:
	mkdir obj
	mkdir obj\deb
	mkdir obj\rel
	mkdir shaders\compiled