.ONESHELL:

#libs are -lglfw3 -lglew32s -lopengl32 -lgdi32 -lccd -lenet64 -lws2_32 -lwinmm
# G++ = C:\msys64\mingw64\bin\g++.exe

I = -I./src -I${VULKAN_SDK}/Include -IC:\prog\sl-vector -I./common
L = -L${VULKAN_SDK}/Lib
F = -pipe -fno-exceptions -fno-rtti
D = -DNDEBUG
SA = 
A = $(I) $(F) $(args)

objs := \
	obj/main.o\
	obj/render.o\
	obj/window.o\
	obj/visible_world.o\

srcs := \
	src/main.cpp\
	src/renderer/render.cpp\
	src/renderer/window.cpp\
	src/renderer/visible_world.cpp\

headers:= \
	src/renderer/render.hpp\
	src/renderer/window.hpp\
	src/renderer/visible_world.hpp\
	src/renderer/primitives.hpp\

_shaders:= \
	shaders/compiled/vert.spv\
	shaders/compiled/frag.spv\
	shaders/compiled/rayGenVert.spv\
	shaders/compiled/rayGenFrag.spv\
	shaders/compiled/blockify.spv\
	shaders/compiled/copy.spv\
	shaders/compiled/map.spv\
	shaders/compiled/comp.spv\

# flags = 

obj/render.o: src/renderer/render.cpp src/renderer/render.hpp src/renderer/window.cpp src/renderer/visible_world.hpp src/renderer/primitives.hpp
	g++ src/renderer/render.cpp -c -o obj/render.o $(F) $(I) $(args)
obj/visible_world.o: src/renderer/visible_world.cpp src/renderer/visible_world.hpp src/renderer/primitives.hpp
	g++ src/renderer/visible_world.cpp -c -o obj/visible_world.o $(F) $(I) $(args)
obj/window.o: src/renderer/window.cpp src/renderer/window.hpp
	g++ src/renderer/window.cpp -c -o obj/window.o $(F) $(I) $(args)
obj/main.o: src/main.cpp $(headers)
	g++ src/main.cpp -c -o obj/main.o $(F) $(I) $(args)
client: $(objs) $(_shaders)
	g++ $(objs) -o client.exe $(F) $(I) $(L) -lglfw3 -lvolk $(args)
client_opt: $(src) $(headers) $(_shaders)
	g++ $(srcs) $(F) $(I) $(L) $(D) -lglfw3 -lvolk -Oz -fdata-sections -ffunction-sections -o client.exe -s -fno-stack-protector -fomit-frame-pointer -fmerge-all-constants -momit-leaf-frame-pointer -mfancy-math-387 -fno-math-errno -Wl,--gc-sections $(args)

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
shaders/compiled/comp.spv: shaders/comp.comp
	glslc shaders/comp.comp -o shaders/compiled/comp.spv $(SA)

init:
	mkdir obj
run: client
	client.exe
opt: client_opt
	client.exe

test: test.cpp
	g++ test.cpp -o test
	test
#FUCK CMAKE