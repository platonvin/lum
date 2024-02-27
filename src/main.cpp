#include <stdio.h>
#include <Volk/volk.h>
#include <GLFW/glfw3.h>
#include <renderer/visible_world.hpp>
#include <renderer/render.hpp>
#include <renderer/window.hpp>
#include <defines.hpp>

VisualWorld world = {};
Renderer render = {};
int main() {
    
println
    render.init();
println
    world.init();

    // std::cout << "\n" <<(int) world.blocksPalette[0].voxels[1][1][1].matID;
println
    render.update_Block_Palette(world.blocksPalette);
println
    render.update_Blocks(world.unitedBlocks.data());
    // std::cout << "\n" <<(int) world.blocksPalette[0].voxels[1][1][1].matID;
println
    render.update_Voxel_Palette(world.matPalette);
println

    // 163
    auto mat = world.matPalette[156];
    printf("\n");
    printl(mat.color.r);
    printl(mat.color.g);
    printl(mat.color.b);
    printl(mat.color.a);
    printl(mat.emmit);
    printl(mat.rough);

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
