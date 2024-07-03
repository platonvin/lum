.ONESHELL:

#libs are -lglfw3 -lglew32s -lopengl32 -lgdi32 -lccd -lenet64 -lws2_32 -lwinmm
# G++ = C:\msys64\mingw64\bin\g++ exe
RML_UI = C:/prog/vcpkg/installed/x64-mingw-static
VCPKG = C:/prog/vcpkg/installed/x64-mingw-static

I = -I./src -I${VULKAN_SDK}/Include -I./common -I$(RML_UI)/Include
L = -L${VULKAN_SDK}/Lib -L$(VCPKG)/lib
F = -pipe -fno-exceptions -O1
D = -DNDEBUG
SA = -Os --target-env=vulkan1.1
A = $(I) $(F) $(args)

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
	# obj/visible_world.o\

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

headers:= \
	src/renderer/render.hpp\
	src/renderer/ui.hpp\
	common/ogt_vox.hpp\
	common/ogt_voxel_meshify.hpp\
	# src/renderer/visible_world.hpp\

_shaders:= \
	shaders/compiled/vert.spv\
	shaders/compiled/frag.spv\
	shaders/compiled/rayGenVert.spv\
	shaders/compiled/rayGenFrag.spv\
	shaders/compiled/rayGenParticlesVert.spv\
	shaders/compiled/rayGenParticlesGeom.spv\
	shaders/compiled/blockify.spv\
	shaders/compiled/copy.spv\
	shaders/compiled/map.spv\
	shaders/compiled/dfx.spv\
	shaders/compiled/dfy.spv\
	shaders/compiled/dfz.spv\
	shaders/compiled/comp.spv\
	shaders/compiled/denoise.spv\
	shaders/compiled/accumulate.spv\
	shaders/compiled/upscale.spv\

# flags = 
all: client
	@echo compiled
measure: client

obj/ogt_vox.o: common/ogt_vox.cpp common/ogt_vox.hpp
	g++ common/ogt_vox.cpp -O2 -c -o obj/ogt_vox.o $(F) $(I) $(args)
obj/ogt_voxel_meshify.o: common/ogt_voxel_meshify.cpp common/ogt_voxel_meshify.hpp
	g++ common/ogt_voxel_meshify.cpp -O2 -c -o obj/ogt_voxel_meshify.o $(F) $(I) $(args)
obj/engine.o: src/engine.cpp src/engine.hpp src/renderer/render.hpp src/renderer/ui.hpp
	g++ src/engine.cpp -c -o obj/engine.o $(F) $(I) $(args)
obj/render.o: src/renderer/render.cpp src/renderer/render.hpp
	g++ src/renderer/render.cpp -c -o obj/render.o $(F) $(I) $(args)
obj/setup.o: src/renderer/setup.cpp src/renderer/render.hpp
	g++ src/renderer/setup.cpp -c -o obj/setup.o $(F) $(I) $(args)
obj/load_stuff.o: src/renderer/load_stuff.cpp src/renderer/render.hpp
	g++ src/renderer/load_stuff.cpp -c -o obj/load_stuff.o $(F) $(I) $(args)
obj/render_ui_interface.o: src/renderer/render_ui_interface.cpp src/renderer/render.hpp
	g++ src/renderer/render_ui_interface.cpp -c -o obj/render_ui_interface.o $(F) $(I) $(args)
obj/ui.o: src/renderer/ui.cpp src/renderer/render.hpp src/renderer/ui.hpp
	g++ src/renderer/ui.cpp -c -o obj/ui.o $(F) $(I) $(args)
obj/main.o: src/main.cpp $(headers)
	g++ src/main.cpp -c -o obj/main.o $(F) -pipe -fno-exceptions -O3 $(I) $(args)

client: $(objs) $(_shaders)
	g++ $(objs) -o client.exe $(F) $(I) $(L) -lglfw3 -lvolk -lRmlDebugger -lRmlCore -lfreetype -lpng -lbrotlienc -lbrotlidec -lbrotlicommon -lpng16 -lz -lbz2 $(args)
client_opt:
	g++ $(srcs) $(I) $(L) $(D) -ftree-parallelize-loops=4 -fopenmp -lglfw3 -lvolk -lRmlCore -lRmlDebugger -lRmlCore -lfreetype -lpng -lbrotlienc -lbrotlidec -lbrotlicommon -lpng16 -lz -lbz2 -Os -pipe -fno-exceptions -fdata-sections -ffunction-sections -o client.exe -s -fno-stack-protector -fomit-frame-pointer -fmerge-all-constants -momit-leaf-frame-pointer -mfancy-math-387 -fno-math-errno -Wl,--gc-sections $(args)

shaders/compiled/vert.spv: shaders/vert.vert
	glslc shaders/vert.vert -o shaders/compiled/vert.spv $(SA)
shaders/compiled/frag.spv: shaders/frag.frag
	glslc shaders/frag.frag -o shaders/compiled/frag.spv $(SA)
shaders/compiled/rayGenVert.spv: shaders/rayGen.vert
	glslc shaders/rayGen.vert -o shaders/compiled/rayGenVert.spv $(SA)
shaders/compiled/rayGenFrag.spv: shaders/rayGen.frag
	glslc shaders/rayGen.frag -o shaders/compiled/rayGenFrag.spv $(SA)
shaders/compiled/rayGenParticlesVert.spv: shaders/rayGenParticles.vert
	glslc shaders/rayGenParticles.vert -o shaders/compiled/rayGenParticlesVert.spv $(SA)
shaders/compiled/rayGenParticlesGeom.spv: shaders/rayGenParticles.geom
	glslc shaders/rayGenParticles.geom -o shaders/compiled/rayGenParticlesGeom.spv $(SA)

shaders/compiled/blockify.spv: shaders/blockify.comp
	glslc shaders/blockify.comp -o shaders/compiled/blockify.spv $(SA)
shaders/compiled/copy.spv: shaders/copy.comp
	glslc shaders/copy.comp -o shaders/compiled/copy.spv $(SA)
shaders/compiled/map.spv: shaders/map.comp
	glslc shaders/map.comp -o shaders/compiled/map.spv $(SA)
shaders/compiled/dfx.spv: shaders/dfx.comp
	glslc shaders/dfx.comp -o shaders/compiled/dfx.spv $(SA)
shaders/compiled/dfy.spv: shaders/dfy.comp
	glslc shaders/dfy.comp -o shaders/compiled/dfy.spv $(SA)
shaders/compiled/dfz.spv: shaders/dfz.comp
	glslc shaders/dfz.comp -o shaders/compiled/dfz.spv $(SA)
# shaders/compiled/comp.spv: shaders/comp.comp
# 	glslc shaders/comp.comp -o shaders/compiled/comp.spv $(SA)
shaders/compiled/comp.spv: shaders/compopt.comp
	glslc shaders/compopt.comp -o shaders/compiled/comp.spv $(SA)
shaders/compiled/denoise.spv: shaders/denoise.comp
	glslc shaders/denoise.comp -o shaders/compiled/denoise.spv $(SA)
shaders/compiled/upscale.spv: shaders/upscale.comp
	glslc shaders/upscale.comp -o shaders/compiled/upscale.spv $(SA)
shaders/compiled/accumulate.spv: shaders/accumulate.comp
	glslc shaders/accumulate.comp -o shaders/compiled/accumulate.spv $(SA)

init:
	mkdir obj
	mkdir shaders\compiled
run: client
	client.exe
# debug: client
# 	F += -DNDEBUG
# 	client.exe
fun:
	@echo fun was never an option
opt: client_opt
# client.exe
clean: #for fixing buld bugs :)
	-del "obj\*.o" 
	-del "shaders\compiled\*.spv" 
test: test.cpp
	g++ test.cpp -o test
	test
#FUCK CMAKE