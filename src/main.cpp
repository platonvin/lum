#include <stdio.h>

#include <Volk/volk.h>
#include <GLFW/glfw3.h>
#include <renderer/visible_world.hpp>
#include <renderer/render.hpp>
#include <renderer/window.hpp>

VisualWorld world = {};
Renderer render = {};
int main() {

    render.init();
    world.init();

    std::cout << "\n" <<(int) world.blocksPalette[0].voxels[1][1][1].matID;
    render.update_Block_Palette(world.blocksPalette);
    std::cout << "\n" <<(int) world.blocksPalette[0].voxels[1][1][1].matID;
    render.update_Blocks(world.unitedBlocks.data());
    render.update_Voxel_Palette(world.matPalette);

    vkDeviceWaitIdle(render.device);

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

    //TODO: temp
    vmaDestroyBuffer(render.VMAllocator, world.loadedChunks(0,0,0).mesh.vertexes.buf[0], world.loadedChunks(0,0,0).mesh.vertexes.alloc[0]);
    vmaDestroyBuffer(render.VMAllocator, world.loadedChunks(0,0,0).mesh.vertexes.buf[1], world.loadedChunks(0,0,0).mesh.vertexes.alloc[1]);
    vmaDestroyBuffer(render.VMAllocator, world.loadedChunks(0,0,0).mesh.indexes.buf[0], world.loadedChunks(0,0,0).mesh.indexes.alloc[0]);
    vmaDestroyBuffer(render.VMAllocator, world.loadedChunks(0,0,0).mesh.indexes.buf[1], world.loadedChunks(0,0,0).mesh.indexes.alloc[1]);

    vkDeviceWaitIdle(render.device);

    render.cleanup();
    return 0;
}
