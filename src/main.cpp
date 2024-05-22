// #include "renderer/primitives.hpp"
// #include "renderer/primitives.hpp"
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/scalar_constants.hpp>
// #include <iostream>
// #include <ostream>
// #include <stdio.h>
#include <Volk/volk.h>
#include <GLFW/glfw3.h>
#include <renderer/visible_world.hpp>
#include <renderer/render.hpp>
// #include <renderer/window.hpp>
#include <defines.hpp>
// #include <string>
#include <glm/gtx/string_cast.hpp>

// VisualWorld world = {};
Renderer render = {};
int itimish = 0;
int main() {
    // Renderer::init(
    render.init();
    vkDeviceWaitIdle(render.device);
    // world.init();

    // std::cout << "\n" <<(int) world.blocksPalette[0].voxels[1][1][1].matID;
    // std::cout << "\n" <<(int) world.blocksPalette[0].voxels[1][1][1].matID;

    table3d<BlockID_t> single_chunk = {};
    //user responsible for managing chunks somehow and also in future translating them to main chunk
    single_chunk.allocate(8,8,8);
    single_chunk.set(0) ;
    render.update_SingleChunk(single_chunk.data());

    // dyn_mesh.vertexes = world.loadedChunks(0,0,0).mesh.vertexes;
    // dyn_mesh.indexes  = world.loadedChunks(0,0,0).mesh.indexes;
    // dyn_mesh = world.loadedChunks(0,0,0).mesh;
    Mesh dyn_mesh1 = {};
    render.load_mesh(&dyn_mesh1, "assets/scene.vox");
    // dyn_mesh1 = world.loadedChunks(0,0,0).mesh;
    // dyn_mesh1.voxels = render.create_RayTrace_VoxelImages((MatID_t*)world.blocksPalette[1].voxels, {16,16,16});
    // dyn_mesh1.size = ivec3(16);
    dyn_mesh1.transform = identity<mat4>();
    dyn_mesh1.transform = translate(dyn_mesh1.transform, vec3(20));
    Mesh dyn_mesh2 = {};
    render.load_mesh(&dyn_mesh2, "assets/mirror.vox");

    //TEMP: emtpy palette just to make things run. No static block atm
    Block* block_palette = (Block*)calloc(BLOCK_PALETTE_SIZE, sizeof(Block));
    render.update_Block_Palette(block_palette);
    render.update_Material_Palette(render.mat_palette);
    // dyn_mesh2 = world.loadedChunks(0,0,1).mesh;
    // dyn_mesh2.voxels = render.create_RayTrace_VoxelImages((MatID_t*)world.blocksPalette[2].voxels, {16,16,16});
    // dyn_mesh2.size = ivec3(16);
    dyn_mesh2.transform = identity<mat4>();
    dyn_mesh2.transform = translate(dyn_mesh2.transform, vec3(20));
    // dyn_mesh2.transform = translate(dyn_mesh2.transform, vec3(16,16,0));
    // dyn_mesh2.voxels = render.create_RayTrace_VoxelImages((MatID_t*)world.blocksPalette[1].voxels, {16,16,16});
    // dyn_mesh.transform = translate(dyn_mesh.transform, vec3(16,16,16));
    
    // rotate(dyn_mesh.transform, pi<float>()/5, vec3(0,1,2));
    //TODO: move to API

    // mat4 isometricRotation = identity<mat4>(); 
    //      isometricRotation = rotate(isometricRotation, radians(+45.0f), vec3(0, 1, 0));
    //      isometricRotation = rotate(isometricRotation, radians(+45.0f), vec3(1, 0, 0));
    // cout << glm::to_string(isometricRotation);
    
    // dyn_mesh2 = dyn_mesh1;

                // dyn_mesh1.transform = rotate(dyn_mesh1.transform, (+0.0037f*75.0f*3.0f), vec3(0,1,0));
                // dyn_mesh1.transform = rotate(dyn_mesh1.transform, (-0.0027f*75.0f*3.0f), vec3(1,0,0));
                // dyn_mesh1.transform = rotate(dyn_mesh1.transform, (+0.0017f*75.0f*3.0f), vec3(0,0,1));
                // dyn_mesh2.transform = rotate(dyn_mesh2.transform, (-(pi<float>())/2.f), vec3(0,0,1));

    //TEMP crutch

    // dyn_mesh2.transform = translate(dyn_mesh2.transform, vec3(30,0,0));
    // void* ptr = malloc((size_t)(void *)malloc); 
    // char* dump;
    // vmaBuildStatsString(render.VMAllocator, &dump, 1);
    // cout << dump;
    // vmaFreeStatsString(render.VMAllocator, dump);

    while(!glfwWindowShouldClose(render.window.pointer) and glfwGetKey(render.window.pointer, GLFW_KEY_ESCAPE) != GLFW_PRESS){
        glfwPollEvents();
        render.start_Frame();
            render.startRaygen();
                render.RaygenMesh(dyn_mesh1);
                render.RaygenMesh(dyn_mesh2);
            render.endRaygen();
                dyn_mesh1.transform = rotate(dyn_mesh1.transform, +0.0037f, vec3(0,1,0));
                // dyn_mesh1.transform = rotate(dyn_mesh1.transform, -0.0027f, vec3(1,0,0));
                // dyn_mesh1.transform = rotate(dyn_mesh1.transform, +0.0017f, vec3(0,0,1));
                dyn_mesh2.transform = rotate(dyn_mesh2.transform, -0.0024f, vec3(0,0,1));
                // dyn_mesh2.transform = rotate(dyn_mesh2.transform, +0.0017f, vec3(0,0,1));
                // dyn_mesh.transform = translate(dyn_mesh.transform, vec3(.0,.0,1.0));
            // dyn_mesh.transform = translate(dyn_mesh.transform, (vec3(0.01, 0.002, 0)));
            // itimish
            // cout << glm::to_string(dyn_mesh.transform) << "\n\n";
            render.startCompute();
                render.startBlockify();
                    render.blockifyMesh(dyn_mesh1);
                    render.blockifyMesh(dyn_mesh2);
                render.endBlockify();
                render.execCopies();
                render.startMap();
                    render.mapMesh(dyn_mesh1);
                    render.mapMesh(dyn_mesh2);
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


    render.free_mesh(&dyn_mesh1);
    render.free_mesh(&dyn_mesh2);
    vkDeviceWaitIdle(render.device);

    render.cleanup();
    // vmaBuildStatsString(render.VMAllocator, &dump, 1);
    // cout << dump;
    // vmaFreeStatsString(render.VMAllocator, dump);
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