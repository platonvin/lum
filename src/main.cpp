// #include <renderer/window.hpp>
// #include "main_pch.hpp"
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/scalar_constants.hpp>
#include <renderer/render.hpp>
#include <defines.hpp>

// VisualWorld world = {};
Renderer render = {};
int itimish = 0;
int main() {
    render.init(8, 8, 8, BLOCK_PALETTE_SIZE, 1024, vec2((3)), false, false);
    vkDeviceWaitIdle(render.device);


    table3d<BlockID_t> single_chunk = {};
    //user responsible for managing chunks somehow and also in future translating them to main chunk
    single_chunk.allocate(8,8,8);
    single_chunk.set(0);
    // for(int x=0; x<16; x++){
    // for(int y=0; y<16; y++){
    //     single_chunk(x,y,0) = 1;
    // }}
    // render.update_SingleChunk(single_chunk.data());
    render.origin_world.set(0);

    // dyn_mesh.vertexes = world.loadedChunks(0,0,0).mesh.vertexes;
    // dyn_mesh.indexes  = world.loadedChunks(0,0,0).mesh.indexes;
    // dyn_mesh = world.loadedChunks(0,0,0).mesh;
    Mesh robot = {};
        // render.load_mesh(&robot, "assets/scene.vox");
        // render.load_mesh(&robot, "assets/robot.vox");
        render.load_mesh(&robot, "assets/Room_1.vox");
        // robot.size = ivec3(16);
        robot.transform = translate(robot.transform, vec3(14.1,10.1,10.1));
        // robot.transform = rotate(robot.transform, pi<float>()/2, vec3(0,0,1));
        robot.transform = rotate(robot.transform, 0.0001f, vec3(0,0,1));
    printl(robot.size.x);
    printl(robot.size.y);
    printl(robot.size.z);

    printl(render.swapChainExtent.width);
    printl(render.swapChainExtent.height);

    //TEMP: emtpy palette just to make things run. No static block atm
    Block* block_palette = (Block*)calloc(BLOCK_PALETTE_SIZE, sizeof(Block));
    for(int x=0; x<16; x++){
    for(int y=0; y<16; y++){
    for(int z=0; z<16; z++){
        block_palette[1].voxels[x][y][z] = 0;
    }}}
    // Mesh block154 = {};
        // render.make_vertices(&block154, &block_palette[1].voxels[0][0][0], 16, 16, 16);

    render.update_Block_Palette(block_palette);
    render.update_Material_Palette(render.mat_palette);

    // void* ptr = malloc((size_t)(void *)malloc); 
    // char* dump;
    // vmaBuildStatsString(render.VMAllocator, &dump, 1);
    // cout << dump;
    // vmaFreeStatsString(render.VMAllocator, dump);
    while(!glfwWindowShouldClose(render.window.pointer) and glfwGetKey(render.window.pointer, GLFW_KEY_ESCAPE) != GLFW_PRESS){
        glfwPollEvents();
        // robot.transform = glm::translate(robot.transform, vec3(0.001));
        // robot.transform = glm::rotate(robot.transform, 0.0001f, vec3(0,0,1));
        render.start_Frame();
            render.startRaygen();
            // vec3 d = -vec3(.0001);
            // robot.old_transform = robot.transform;
                render.RaygenMesh(robot);
            render.endRaygen();

            render.startCompute();
                render.startBlockify();
                    render.blockifyMesh(robot);
                render.endBlockify();

                render.startMap();
                    render.mapMesh(robot);
                render.endMap();
                // robot.transform = glm::translate(robot.transform, -d*10.f);
                
                // render.recalculate_df();

                render.raytrace();
                render.denoise();
                if(render.is_scaled){
                    render.upscale();
                }
            render.endCompute();

            render.present();
            // render.end_Present();
        render.end_Frame();
    }
    //lol this was in main loop
    vkDeviceWaitIdle(render.device);

    //TODO: temp

    render.free_mesh(&robot);
    // render.free_mesh(&block154);
    // render.free_mesh(&dyn_mesh1);
    // render.free_mesh(&dyn_mesh2);
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

    start_map
        map_mesh
    end_map

end_frame
*/


