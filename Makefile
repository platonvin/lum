.ONESHELL:

#libs are -lglfw3 -lglew32s -lopengl32 -lgdi32 -lccd -lenet64 -lws2_32 -lwinmm
# G++ = C:\msys64\mingw64\bin\g++.exe

I = -I./src -I${VULKAN_SDK}/Include
L = -L${VULKAN_SDK}/Lib
objs := \
	obj/main.o\
	obj/render.o\
	obj/window.o

srcs := \
	src/main.cpp\
	src/renderer/render.cpp\
	src/renderer/window.cpp

headers:= \
	src/renderer/render.hpp\
	src/renderer/window.hpp

# flags = 

obj/render.o: src/renderer/render.cpp src/renderer/render.hpp src/renderer/window.hpp
	g++ src/renderer/render.cpp -c -o obj/render.o -pipe $(I)

obj/window.o: src/renderer/window.cpp src/renderer/window.hpp
	g++ src/renderer/window.cpp -c -o obj/window.o -pipe $(I)

obj/main.o: src/main.cpp $(srcs) $(headers)
	g++ src/main.cpp -c -o obj/main.o -pipe $(I)

client: $(objs)
	g++ $(objs) -o client.exe -pipe $(I) $(L) -lglfw3 -lvolk

client_opt: $(src) $(headers)
	g++ $(srcs) -pipe $(I) $(L) -lglfw3 -lvolk -Oz -fdata-sections -ffunction-sections -o client.exe -Wl,--gc-sections 

shaders/vert.spv: shaders/vert.vert
	glslc shaders/vert.vert -o shaders/vert.spv
shaders/frag.spv: shaders/frag.frag
	glslc shaders/frag.frag -o shaders/frag.spv

run: client shaders/vert.spv shaders/frag.spv
	client.exe
opt: client_opt
	client.exe

test: test.cpp
	g++ test.cpp -o test
	test
#FUCK CMAKE