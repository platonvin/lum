#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/scalar_constants.hpp>
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

    Mesh dyn_mesh = {};
    dyn_mesh.voxels = render.create_RayTrace_VoxelImages((MatID_t*)world.blocksPalette[1].voxels, {16,16,16});
    dyn_mesh.transform = identity<mat4>();
    
    // rotate(dyn_mesh.transform, pi<float>()/5, vec3(0,1,2));
    dyn_mesh.size = ivec3(16);

    while(!glfwWindowShouldClose(render.window.pointer) and glfwGetKey(render.window.pointer, GLFW_KEY_ESCAPE) != GLFW_PRESS){
        glfwPollEvents();
        render.start_Frame();
            render.startRaygen();
                render.RaygenMesh(world.loadedChunks(0,0,0).mesh);
            render.endRaygen();
            
            // dyn_mesh.transform = rotate(dyn_mesh.transform, 0.002f, vec3(1,0,0));
            dyn_mesh.transform = translate(dyn_mesh.transform, vec3(0.1,0,0));

            render.startCompute();
                render.startBlockify();
                render.blockifyMesh(dyn_mesh);
                render.endBlockify();
                    // render.execCopies();
                render.startMap();
                render.mapMesh(dyn_mesh);
                render.endMap();
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