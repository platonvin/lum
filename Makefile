.ONESHELL:

#libs are -lglfw3 -lglew32s -lopengl32 -lgdi32 -lccd -lenet64 -lws2_32 -lwinmm
# GCC = C:\msys64\mingw64\bin\gcc.exe

I = -I./src -I${VULKAN_SDK}/Include
L = -L${VULKAN_SDK}/Lib
objs := \
	obj/main.o\
	obj/render.o\
	obj/window.o

srcs := \
	src/main.c\
	src/renderer/render.c\
	src/renderer/window.c

headers:= \
	src/renderer/render.h\
	src/renderer/window.h

obj/render.o: src/renderer/render.c src/renderer/render.h src/renderer/window.h
	gcc src/renderer/render.c -c -o obj/render.o -pipe $(I)

obj/window.o: src/renderer/window.c src/renderer/window.h
	gcc src/renderer/window.c -c -o obj/window.o -pipe $(I)

obj/main.o: src/main.c $(srcs) $(headers)
	gcc src/main.c -c -o obj/main.o -pipe $(I)

client: $(objs)
	gcc $(objs) -o client.exe -pipe $(I) $(L) -lglfw3 -lvolk

client_opt: $(src) $(headers)
	gcc $(srcs) -o client.exe -pipe $(I) $(L) -lglfw3 -lvolk

run: client
	client.exe
opt: client_opt
	client.exe