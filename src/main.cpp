#include <glm/ext/matrix_transform.hpp>
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
    // Renderer::init(
    render.init();
    world.init();

    // std::cout << "\n" <<(int) world.blocksPalette[0].voxels[1][1][1].matID;
    render.update_Block_Palette(world.blocksPalette);
    render.update_Blocks(world.unitedBlocks.data());
    // std::cout << "\n" <<(int) world.blocksPalette[0].voxels[1][1][1].matID;
    render.update_Voxel_Palette(world.matPalette);

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

    table3d<MatID_t> dyn_16_block = {};
    dyn_16_block.set_size(16, 16, 16);
    dyn_16_block(0,0,0)=155;
    dyn_16_block(0,0,1)=111;
    dyn_16_block(1,0,2)=122;
    dyn_16_block(0,3,0)=64;
    dyn_16_block(0,5,0)=53;
    dyn_16_block(0,6,0)=13;
    dyn_16_block(1,0,3)=143;
    Mesh dyn_mesh = {};
    dyn_mesh.voxels = render.create_RayTrace_VoxelImages(dyn_16_block.data(), {16,16,16});
    dyn_mesh.transform = identity<mat4>();
    dyn_mesh.size = ivec3(16);

    while(!glfwWindowShouldClose(render.window.pointer) and glfwGetKey(render.window.pointer, GLFW_KEY_ESCAPE) != GLFW_PRESS){
        glfwPollEvents();
        render.start_Frame();
            render.startRaygen();
                render.RaygenMesh(world.loadedChunks(0,0,0).mesh);
            render.endRaygen();
            
            render.startCompute();
                render.startBlockify();
                render.blockifyMesh(dyn_mesh);
                render.endBlockify();
                // render.copy_RayTrace();
                render.startMap();
                render.mapMesh(dyn_mesh);
                render.endMap();
            // render.end_RayTrace();
                render.raytrace();
            render.endCompute();

            render.present();
            // render.end_Present();
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

/*
start_frame

start_raygen
raygen_mesh
end__raygen

start_blockify
blockify_mesh
end_blockify

end_frame
*/