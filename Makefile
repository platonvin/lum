.ONESHELL:

#libs are -lglfw3 -lglew32s -lopengl32 -lgdi32 -lccd -lenet64 -lws2_32 -lwinmm
# G++ = C:\msys64\mingw64\bin\g++ exe

I = -I./src -I${VULKAN_SDK}/Include -I./common -I${VCPKG_ROOT}/installed/x64-mingw-static/Include
L = -L${VULKAN_SDK}/Lib -L${VCPKG_ROOT}/installed/x64-mingw-static/lib
#-ftrivial-auto-var-init=zero sets "local" vars to 0 by default
debug_Flags = -pipe -fno-exceptions -O1 -Wuninitialized -ftrivial-auto-var-init=zero
release_Flags = -pipe -fno-exceptions -ftrivial-auto-var-init=zero -Os -DNDEBUG
SA = -O --target-env=vulkan1.1

objs := \
	obj/main.o\
	obj/engine.o\
	obj/render.o\
	obj/setup.o\
	obj/load_stuff.o\
	obj/render_ui_interface.o\
	obj/ui.o\
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
	shaders/compiled/glossyVert.spv\
	shaders/compiled/glossyFrag.spv\
	shaders/compiled/overlayVert.spv\
	shaders/compiled/overlayFrag.spv\

# flags = 
all: Flags=$(release_Flags)
all: clean build
	@echo compiled
measure: build

obj/ogt_vox.o: common/ogt_vox.cpp common/ogt_vox.hpp
	g++ common/ogt_vox.cpp -O2 -c -o obj/ogt_vox.o $(Flags) $(I) $(args)
obj/ogt_voxel_meshify.o: common/ogt_voxel_meshify.cpp common/ogt_voxel_meshify.hpp common/meshopt.hpp
	g++ common/ogt_voxel_meshify.cpp -O2 -c -o obj/ogt_voxel_meshify.o $(Flags) $(I) $(args)
obj/meshopt.o: common/meshopt.cpp common/meshopt.hpp
	g++ common/meshopt.cpp -O2 -c -o obj/meshopt.o $(Flags) $(I) $(args)
obj/engine.o: src/engine.cpp src/engine.hpp src/renderer/render.cpp src/renderer/render.hpp src/renderer/ui.hpp
	g++ src/engine.cpp -c -o obj/engine.o $(Flags) $(I) $(args)
obj/render.o: src/renderer/render.cpp src/renderer/render.hpp
	g++ src/renderer/render.cpp -c -o obj/render.o $(Flags) $(I) $(args)
obj/setup.o: src/renderer/setup.cpp src/renderer/render.hpp
	g++ src/renderer/setup.cpp -c -o obj/setup.o $(Flags) $(I) $(args)
obj/load_stuff.o: src/renderer/load_stuff.cpp src/renderer/render.hpp
	g++ src/renderer/load_stuff.cpp -c -o obj/load_stuff.o $(Flags) $(I) $(args)
obj/render_ui_interface.o: src/renderer/render_ui_interface.cpp src/renderer/render.hpp
	g++ src/renderer/render_ui_interface.cpp -c -o obj/render_ui_interface.o $(Flags) $(I) $(args)
obj/ui.o: src/renderer/ui.cpp src/renderer/ui.hpp src/renderer/render.hpp
	g++ src/renderer/ui.cpp -c -o obj/ui.o $(Flags) $(I) $(args)
obj/main.o: src/main.cpp src/engine.hpp src/renderer/render.hpp
	g++ src/main.cpp -c -o obj/main.o $(Flags) -pipe -fno-exceptions $(I) $(args)


build: $(objs) $(_shaders)
	g++ $(objs) -o client.exe $(Flags) $(I) $(L) -l:libglfw3.a -lgdi32 -l:volk.lib -lRmlDebugger -lRmlCore -lfreetype -lpng -lbrotlienc -lbrotlidec -lbrotlicommon -lpng16 -lz -lbz2 -static $(args)
# client_opt:
# 	g++ $(srcs) $(I) $(L) $(D) -l:libglfw3.a -lgdi32 -l:volk.lib -lRmlCore -lRmlDebugger -lRmlCore -lfreetype -lpng -lbrotlienc -lbrotlidec -lbrotlicommon -lpng16 -lz -lbz2 -static -Os -pipe -fno-exceptions -fdata-sections -ffunction-sections -o client.exe -s -fno-stack-protector -fomit-frame-pointer -fmerge-all-constants -momit-leaf-frame-pointer -mfancy-math-387 -fno-math-errno -Wl,--gc-sections $(args)

shaders/compiled/map.spv: shaders/map.comp 
	glslc shaders/map.comp -o shaders/compiled/map.spv $(SA)
shaders/compiled/radiance.spv: shaders/radiance.comp
	glslc shaders/radiance.comp -o shaders/compiled/radiance.spv $(SA)
shaders/compiled/blurVert.spv: shaders/blur.vert 
	glslc shaders/blur.vert -o shaders/compiled/blurVert.spv $(SA)
shaders/compiled/blurFrag.spv: shaders/blur.frag 
	glslc shaders/blur.frag -o shaders/compiled/blurFrag.spv $(SA)
shaders/compiled/rayGenVert.spv: shaders/rayGen.vert 
	glslc shaders/rayGen.vert -o shaders/compiled/rayGenVert.spv $(SA)
shaders/compiled/rayGenFrag.spv: shaders/rayGen.frag 
	glslc shaders/rayGen.frag -o shaders/compiled/rayGenFrag.spv $(SA)
shaders/compiled/rayGenParticlesVert.spv: shaders/rayGenParticles.vert 
	glslc shaders/rayGenParticles.vert -o shaders/compiled/rayGenParticlesVert.spv $(SA)
shaders/compiled/rayGenParticlesGeom.spv: shaders/rayGenParticles.geom 
	glslc shaders/rayGenParticles.geom -o shaders/compiled/rayGenParticlesGeom.spv $(SA)
shaders/compiled/diffuseVert.spv: shaders/diffuse.vert 
	glslc shaders/diffuse.vert -o shaders/compiled/diffuseVert.spv $(SA)
shaders/compiled/diffuseFrag.spv: shaders/diffuse.frag 
	glslc shaders/diffuse.frag -o shaders/compiled/diffuseFrag.spv $(SA)
shaders/compiled/glossyVert.spv: shaders/glossy.vert 
	glslc shaders/glossy.vert -o shaders/compiled/glossyVert.spv $(SA)
shaders/compiled/glossyFrag.spv: shaders/glossy.frag 
	glslc shaders/glossy.frag -o shaders/compiled/glossyFrag.spv $(SA)
shaders/compiled/overlayVert.spv: shaders/overlay.vert 
	glslc shaders/overlay.vert -o shaders/compiled/overlayVert.spv $(SA)
shaders/compiled/overlayFrag.spv: shaders/overlay.frag  
	glslc shaders/overlay.frag -o shaders/compiled/overlayFrag.spv $(SA)


# shaders/compiled/*.spv: shaders/*.spv 
# 	glslc shaders/*.vert -o shaders/compiled/*Vert.spv $(SA)

# shaders/compiled/vert.spv: shaders/vert.vert
# 	glslc shaders/vert.vert -o shaders/compiled/vert.spv $(SA)
# shaders/compiled/frag.spv: shaders/frag.frag
# 	glslc shaders/frag.frag -o shaders/compiled/frag.spv $(SA)
# shaders/compiled/rayGenVert.spv: shaders/rayGen.vert
# 	glslc shaders/rayGen.vert -o shaders/compiled/rayGenVert.spv $(SA)
# shaders/compiled/rayGenFrag.spv: shaders/rayGen.frag
# 	glslc shaders/rayGen.frag -o shaders/compiled/rayGenFrag.spv $(SA)
# shaders/compiled/rayGenParticlesVert.spv: shaders/rayGenParticles.vert
# 	glslc shaders/rayGenParticles.vert -o shaders/compiled/rayGenParticlesVert.spv $(SA)
# shaders/compiled/rayGenParticlesGeom.spv: shaders/rayGenParticles.geom
# 	glslc shaders/rayGenParticles.geom -o shaders/compiled/rayGenParticlesGeom.spv $(SA)

# shaders/compiled/blockify.spv: shaders/blockify.comp
# 	glslc shaders/blockify.comp -o shaders/compiled/blockify.spv $(SA)
# shaders/compiled/copy.spv: shaders/copy.comp
# 	glslc shaders/copy.comp -o shaders/compiled/copy.spv $(SA)
# shaders/compiled/map.spv: shaders/map.comp
# 	glslc shaders/map.comp -o shaders/compiled/map.spv $(SA)
# shaders/compiled/mipmap.spv: shaders/mipmap.comp
# 	glslc shaders/mipmap.comp -o shaders/compiled/mipmap.spv $(SA)
# shaders/compiled/dfx.spv: shaders/dfx.comp
# 	glslc shaders/dfx.comp -o shaders/compiled/dfx.spv $(SA)
# shaders/compiled/dfy.spv: shaders/dfy.comp
# 	glslc shaders/dfy.comp -o shaders/compiled/dfy.spv $(SA)
# shaders/compiled/dfz.spv: shaders/dfz.comp
# 	glslc shaders/dfz.comp -o shaders/compiled/dfz.spv $(SA)
	
# shaders/compiled/raytrace.spv: shaders/conetrace.comp
# 	glslc shaders/conetrace.comp -o shaders/compiled/raytrace.spv $(SA)
# # shaders/compiled/raytrace.spv: shaders/raytrace.comp
# # 	glslc shaders/raytrace.comp -o shaders/compiled/raytrace.spv $(SA)
# shaders/compiled/radiance.spv: shaders/updateRadianceCache.comp
# 	glslc shaders/updateRadianceCache.comp -o shaders/compiled/radiance.spv $(SA)
# shaders/compiled/diffuse.spv: shaders/diffuse.comp
# 	glslc shaders/diffuse.comp -o shaders/compiled/diffuse.spv $(SA)
# shaders/compiled/glossy.spv: shaders/glossy.comp
# 	glslc shaders/glossy.comp -o shaders/compiled/glossy.spv $(SA)
# shaders/compiled/denoise.spv: shaders/denoise.comp
# 	glslc shaders/denoise.comp -o shaders/compiled/denoise.spv $(SA)
# shaders/compiled/upscale.spv: shaders/upscale.comp
# 	glslc shaders/upscale.comp -o shaders/compiled/upscale.spv $(SA)
# shaders/compiled/accumulate.spv: shaders/accumulate.comp
# 	glslc shaders/accumulate.comp -o shaders/compiled/accumulate.spv $(SA)

init:
	mkdir obj
	mkdir shaders\compiled

debug: Flags=$(debug_Flags)
# clean once before usage
debug: build
	client.exe
release: Flags=$(release_Flags)
release: build
	client.exe
# debug: client
# 	F += -DNDEBUG
# 	client.exe
fun:
	@echo fun was never an option
opt: client_opt
# client.exe
clean:
	del "obj\*.o" 
	del "shaders\compiled\*.spv" 
test: test.cpp
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