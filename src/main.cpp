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
    // Renderer::init(
    render.init(8, 8, 8, BLOCK_PALETTE_SIZE, 1024);
    vkDeviceWaitIdle(render.device);
    // world.init();

    // std::cout << "\n" <<(int) world.blocksPalette[0].voxels[1][1][1].matID;
    // std::cout << "\n" <<(int) world.blocksPalette[0].voxels[1][1][1].matID;

    table3d<BlockID_t> single_chunk = {};
    //user responsible for managing chunks somehow and also in future translating them to main chunk
    single_chunk.allocate(8,8,8);
    single_chunk.set(0);
    // for(int x=0; x<16; x++){
    // for(int y=0; y<16; y++){
    //     single_chunk(x,y,0) = 1;
    // }}
    render.update_SingleChunk(single_chunk.data());

    // dyn_mesh.vertexes = world.loadedChunks(0,0,0).mesh.vertexes;
    // dyn_mesh.indexes  = world.loadedChunks(0,0,0).mesh.indexes;
    // dyn_mesh = world.loadedChunks(0,0,0).mesh;
    Mesh robot = {};
        // render.load_mesh(&robot, "assets/scene.vox");
        // render.load_mesh(&robot, "assets/robot.vox");
        render.load_mesh(&robot, "assets/Room_1.vox");
        // robot.size = ivec3(16);
        robot.transform = translate(robot.transform, vec3(14,10,10));
        // robot.transform = rotate(robot.transform, pi<float>()/2, vec3(0,0,1));
        // robot.transform = rotate(robot.transform, 0.01f, vec3(0,1,0));
    printl(robot.size.x);
    printl(robot.size.y);
    printl(robot.size.z);

    printl(render.swapChainExtent.width);
    printl(render.swapChainExtent.height);

    // Mesh dyn_mesh1 = {};
    //     render.load_mesh(&dyn_mesh1, "assets/scene.vox");
    //     dyn_mesh1.transform = identity<mat4>();,
    //     dyn_mesh1.transform = translate(dyn_mesh1.transform vec3(20));
    // Mesh dyn_mesh2 = {};
    //     render.load_mesh(&dyn_mesh2, "assets/mirror.vox");
    //     dyn_mesh2.transform = identity<mat4>();
    //     dyn_mesh2.transform = translate(dyn_mesh2.transform, vec3(20));

    //TEMP: emtpy palette just to make things run. No static block atm
    Block* block_palette = (Block*)calloc(BLOCK_PALETTE_SIZE, sizeof(Block));
    for(int x=0; x<16; x++){
    for(int y=0; y<16; y++){
    for(int z=0; z<16; z++){
        block_palette[1].voxels[x][y][z] = 154;
    }}}
    // Mesh block154 = {};
        // render.make_vertices(&block154, &block_palette[1].voxels[0][0][0], 16, 16, 16);

    render.update_Block_Palette(block_palette);
    render.update_Material_Palette(render.mat_palette);
    // dyn_mesh2 = world.loadedChunks(0,0,1).mesh;
    // dyn_mesh2.voxels = render.create_RayTrace_VoxelImages((MatID_t*)world.blocksPalette[2].voxels, {16,16,16});
    // dyn_mesh2.size = ivec3(16);
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
                render.RaygenMesh(robot);

                // for(int x=0; x<16; x++){
                // for(int y=0; y<16; y++){
                //     block154.transform = translate(identity<mat4>(), vec3(x*16,y*16,0));
                //     // render.RaygenMesh(block154);
                // }}
                // render.RaygenMesh(dyn_mesh2);
            render.endRaygen();
                // robot.transform = translate(robot.transform, -vec3(60,60,16));
                // robot.transform = translate(robot.transform, -vec3(robot.size)/2.0f);
                // robot.transform = rotate(robot.transform, -0.0001f, vec3(0,0,1));
                // robot.transform = translate(robot.transform, +vec3(0.01));
                // robot.transform = translate(robot.transform, vec3(60,60,16));
                // dyn_mesh1.transform = rotate(dyn_mesh1.transform, -0.0027f, vec3(1,0,0));
                // dyn_mesh1.transform = rotate(dyn_mesh1.transform, +0.0017f, vec3(0,0,1));
                // dyn_mesh2.transform = rotate(dyn_mesh2.transform, -0.0024f, vec3(0,0,1));
                // dyn_mesh2.transform = rotate(dyn_mesh2.transform, +0.0017f, vec3(0,0,1));
                // robot.transform = translate(robot.transform, vec3(.0,.1,0));
            // dyn_mesh.transform = translate(dyn_mesh.transform, (vec3(0.01, 0.002, 0)));
            // itimish
            // cout << glm::to_string(dyn_mesh.transform) << "\n\n";
            render.startCompute();
                render.startBlockify();
                    render.blockifyMesh(robot);
                    // render.blockifyMesh(dyn_mesh1);
                    // render.blockifyMesh(dyn_mesh2);
                render.endBlockify();
                render.execCopies();
                //render.recalculate_df();
                render.startMap();
                    render.mapMesh(robot);
                    // render.mapMesh(dyn_mesh1);
                    // render.mapMesh(dyn_mesh2);
                render.endMap();
                // render.recalculate_df();
                render.raytrace();
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

end_frame
*/