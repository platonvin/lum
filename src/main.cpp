#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/scalar_constants.hpp>
// #include <iostream>
// #include <ostream>
// #include <stdio.h>
#include <Volk/volk.h>
#include <GLFW/glfw3.h>
#include <renderer/visible_world.hpp>
#include <renderer/render.hpp>
#include <renderer/window.hpp>
#include <defines.hpp>
// #include <string>
#include <glm/gtx/string_cast.hpp>

VisualWorld world = {};
Renderer render = {};
int itimish = 0;
int main() {
    // Renderer::init(
    render.init();
    world.init();

    // std::cout << "\n" <<(int) world.blocksPalette[0].voxels[1][1][1].matID;
    render.update_Block_Palette(world.blocksPalette);
    render.update_Blocks(world.unitedBlocks.data());
    // std::cout << "\n" <<(int) world.blocksPalette[0].voxels[1][1][1].matID;
    render.update_Voxel_Palette(world.matPalette);


    Mesh dyn_mesh1 = {};
    Mesh dyn_mesh2 = {};
    // dyn_mesh.vertexes = world.loadedChunks(0,0,0).mesh.vertexes;
    // dyn_mesh.indexes  = world.loadedChunks(0,0,0).mesh.indexes;
    // dyn_mesh = world.loadedChunks(0,0,0).mesh;
    dyn_mesh1 = world.loadedChunks(0,0,0).mesh;
    dyn_mesh1.voxels = render.create_RayTrace_VoxelImages((MatID_t*)world.blocksPalette[1].voxels, {16,16,16});
    // dyn_mesh.transform = translate(dyn_mesh.transform, vec3(16,16,16));
    
    // rotate(dyn_mesh.transform, pi<float>()/5, vec3(0,1,2));
    //TODO: move to API
    dyn_mesh1.size = ivec3(16);
    dyn_mesh1.transform = identity<mat4>();
    dyn_mesh1.transform = translate(dyn_mesh1.transform, vec3(30));


    mat4 isometricRotation = identity<mat4>(); 
         isometricRotation = rotate(isometricRotation, radians(+45.0f), vec3(0, 1, 0));
         isometricRotation = rotate(isometricRotation, radians(+45.0f), vec3(1, 0, 0));
    cout << glm::to_string(isometricRotation);
    
    dyn_mesh2 = dyn_mesh1;

                dyn_mesh1.transform = rotate(dyn_mesh1.transform, (+0.0037f*75.0f*3.0f), vec3(0,1,0));
                dyn_mesh1.transform = rotate(dyn_mesh1.transform, (-0.0027f*75.0f*3.0f), vec3(1,0,0));
                dyn_mesh1.transform = rotate(dyn_mesh1.transform, (+0.0017f*75.0f*3.0f), vec3(0,0,1));
                dyn_mesh2.transform = rotate(dyn_mesh2.transform, (-0.0017f*75.0f*3.0f), vec3(0,0,1));

    vkDeviceWaitIdle(render.device);
    // dyn_mesh2.transform = translate(dyn_mesh2.transform, vec3(30,0,0));
    // void* ptr = malloc((size_t)(void *)malloc); 

    while(!glfwWindowShouldClose(render.window.pointer) and glfwGetKey(render.window.pointer, GLFW_KEY_ESCAPE) != GLFW_PRESS){
        glfwPollEvents();
        render.start_Frame();
            render.startRaygen();
                render.RaygenMesh(dyn_mesh1);
                render.RaygenMesh(dyn_mesh2);
            render.endRaygen();
                dyn_mesh1.transform = rotate(dyn_mesh1.transform, +0.0037f, vec3(0,1,0));
                dyn_mesh1.transform = rotate(dyn_mesh1.transform, -0.0027f, vec3(1,0,0));
                dyn_mesh1.transform = rotate(dyn_mesh1.transform, +0.0017f, vec3(0,0,1));
                dyn_mesh2.transform = rotate(dyn_mesh2.transform, -0.0017f, vec3(0,0,1));
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