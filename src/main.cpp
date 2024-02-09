#include <stdio.h>

#include <Volk/volk.h>
#include <GLFW/glfw3.h>
#include <renderer/visible_world.hpp>
#include <renderer/render.hpp>
#include <renderer/window.hpp>

// #include <slmath/sources/

VisualWorld world = {};
Renderer render = {};
int main() {
    // init();
    render.init();
    world.init();

    while(!glfwWindowShouldClose(render.window.pointer) and glfwGetKey(render.window.pointer, GLFW_KEY_ESCAPE) != GLFW_PRESS){
        glfwPollEvents();
        render.start_Frame();
            render.start_RayGen();
            render.draw_RayGen_Mesh(world.loadedChunks(0,0,0).mesh);
            render.end_RayGen();
            render.start_RayTrace();
            render.end_RayTrace();
            render.start_Present();
            render.end_Present();
        render.end_Frame();
    }
    //lol this was in main loop
    vkDeviceWaitIdle(render.device);

    vmaDestroyBuffer(render.VMAllocator, world.loadedChunks(0,0,0).mesh.data.vertices[0], world.loadedChunks(0,0,0).mesh.data.verticesAllocation[0]);
    vmaDestroyBuffer(render.VMAllocator, world.loadedChunks(0,0,0).mesh.data.vertices[1], world.loadedChunks(0,0,0).mesh.data.verticesAllocation[1]);
    vmaDestroyBuffer(render.VMAllocator, world.loadedChunks(0,0,0).mesh.data.indices[0] , world.loadedChunks(0,0,0).mesh.data.indicesAllocation[0] );
    vmaDestroyBuffer(render.VMAllocator, world.loadedChunks(0,0,0).mesh.data.indices[1] , world.loadedChunks(0,0,0).mesh.data.indicesAllocation[1] );

    render.cleanup();
    return 0;
}
