.ONESHELL:

#libs are -lglfw3 -lglew32s -lopengl32 -lgdi32 -lccd -lenet64 -lws2_32 -lwinmm
# G++ = C:\msys64\mingw64\bin\g++ exe

I = -I./src -I${VULKAN_SDK}/Include -I./common
L = -L${VULKAN_SDK}/Lib
F = -pipe -fno-exceptions -fno-rtti -O1
D = -DNDEBUG
SA = -O --target-env=vulkan1.1
A = $(I) $(F) $(args)

objs := \
	obj/main.o\
	obj/render.o\
	obj/setup.o\
	obj/load_stuff.o\
	obj/ogt_vox.o\
	obj/ogt_voxel_meshify.o\
	# obj/visible_world.o\

srcs := \
	src/main.cpp\
	src/renderer/render.cpp\
	src/renderer/setup.cpp\
	src/renderer/load_stuff.cpp\
	common/ogt_vox.cpp\
	common/ogt_voxel_meshify.cpp\
	# src/renderer/visible_world.cpp\

headers:= \
	src/renderer/render.hpp\
	common/ogt_vox.hpp\
	common/ogt_voxel_meshify.hpp\
	# src/renderer/visible_world.hpp\

_shaders:= \
	shaders/compiled/vert.spv\
	shaders/compiled/frag.spv\
	shaders/compiled/rayGenVert.spv\
	shaders/compiled/rayGenFrag.spv\
	shaders/compiled/blockify.spv\
	shaders/compiled/copy.spv\
	shaders/compiled/map.spv\
	shaders/compiled/dfx.spv\
	shaders/compiled/dfy.spv\
	shaders/compiled/dfz.spv\
	shaders/compiled/comp.spv\
	shaders/compiled/denoise.spv\
	shaders/compiled/upscale.spv\

# flags = 
all: client
	@echo compiled this ****
measure: client

obj/ogt_vox.o: common/ogt_vox.cpp common/ogt_vox.hpp
	g++ common/ogt_vox.cpp -O2 -c -o obj/ogt_vox.o $(F) $(I) $(args)
obj/ogt_voxel_meshify.o: common/ogt_voxel_meshify.cpp common/ogt_voxel_meshify.hpp
	g++ common/ogt_voxel_meshify.cpp -O2 -c -o obj/ogt_voxel_meshify.o $(F) $(I) $(args)
obj/render.o: src/renderer/render.cpp src/renderer/render.hpp
	g++ src/renderer/render.cpp -c -o obj/render.o $(F) $(I) $(args)
obj/setup.o: src/renderer/setup.cpp src/renderer/render.hpp
	g++ src/renderer/setup.cpp -c -o obj/setup.o $(F) $(I) $(args)
obj/load_stuff.o: src/renderer/load_stuff.cpp src/renderer/render.hpp
	g++ src/renderer/load_stuff.cpp -c -o obj/load_stuff.o $(F) $(I) $(args)
# obj/visible_world.o: src/renderer/visible_world.cpp src/renderer/visible_world.hpp src/renderer/primitives.hpp
# 	g++ src/renderer/visible_world.cpp -c -o obj/visible_world.o $(F) $(I) $(args)
# obj/window.o: src/renderer/window.cpp src/renderer/window.hpp
# 	g++ src/renderer/window.cpp -c -o obj/window.o $(F) $(I) $(args)
obj/main.o: src/main.cpp $(headers)
	g++ src/main.cpp -c -o obj/main.o $(F) $(I) $(args)
# obj/main.pch: src/main.h
# 	g++ src/main.cpp -c -o obj/main.o $(F) $(I) $(args)
client: $(objs) $(_shaders)
	g++ $(objs) -o client.exe $(F) $(I) $(L) -lglfw3 -lvolk $(args)
client_opt: $(src) $(headers) $(_shaders)
	g++ $(srcs) $(I) $(L) $(D) -lglfw3 -lvolk -Oz -pipe -fno-exceptions -fno-rtti -fdata-sections -ffunction-sections -o client.exe -s -fno-stack-protector -fomit-frame-pointer -fmerge-all-constants -momit-leaf-frame-pointer -mfancy-math-387 -fno-math-errno -Wl,--gc-sections $(args)

temp:
	g++ .\src\renderer\temp.cpp $(F) $(I) $(L)

shaders/compiled/vert.spv: shaders/vert.vert
	glslc shaders/vert.vert -o shaders/compiled/vert.spv $(SA)
shaders/compiled/frag.spv: shaders/frag.frag
	glslc shaders/frag.frag -o shaders/compiled/frag.spv $(SA)
shaders/compiled/rayGenVert.spv: shaders/rayGen.vert
	glslc shaders/rayGen.vert -o shaders/compiled/rayGenVert.spv $(SA)
shaders/compiled/rayGenFrag.spv: shaders/rayGen.frag
	glslc shaders/rayGen.frag -o shaders/compiled/rayGenFrag.spv $(SA)
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

init:
	mkdir obj
run: client
	client.exe
fun:
	@echo fun was never an option
opt: client_opt
# client.exe
clean:
	-del "obj\*.o" 
test: test.cpp
	g++ test.cpp -o test
	test
#FUCK CMAKE