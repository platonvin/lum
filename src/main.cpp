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
    render.init(8, 8, 8, BLOCK_PALETTE_SIZE, 1024, vec2(sqrt(1)), true, false);
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
        // render.load_mesh(&robot, "assets/scene.vox");
        // render.load_mesh(&robot, "assets/robot.vox");

        // robot.size = ivec3(16);
    // Mesh black_block = {};
        // render.load_mesh(&black_block, "assets/black_block.vox");
    // Mesh white_block = {};
        // render.load_mesh(&white_block, "assets/white_block.vox");

    for(int xx=0; xx<8; xx++){
    for(int yy=0; yy<8; yy++){
        render.origin_world(xx, yy, 1) = (((xx+yy) % 2) == 0) ? 2 : 1;
        // render.origin_world(xx, yy, 1) = 1;
    }}
    for(int xx=0; xx<8; xx++){
    for(int yy=0; yy<8; yy++){
        if(((xx+yy) % 6) == 5){
            render.origin_world(xx, yy, 2) = 3;
        }
        if(((xx+yy) % 7) == 2){
            render.origin_world(xx, yy, 2) = 5;
        }
        if(((xx+yy) % 11) == 5){
            render.origin_world(xx, yy, 2) = 4;
        }
        // render.origin_world(xx, yy, 1) = 1;
    }}
    // render.origin_world(5, 5, 2) = 5;

    Mesh robot = {};
        render.load_mesh(&robot, "assets/tank.vox");
        robot.transform = translate(robot.transform, vec3(14.1,10.1,20.1));
        robot.transform = rotate(robot.transform, 0.001f, vec3(0,0,1));

    //TEMP: emtpy palette just to make things run. No static block atm
    Block** block_palette = (Block**)calloc(BLOCK_PALETTE_SIZE, sizeof(Block*));
    render.load_block(&block_palette[1], "assets/dirt.vox");
    render.load_block(&block_palette[2], "assets/grass.vox");
    render.load_block(&block_palette[3], "assets/bush.vox");
    render.load_block(&block_palette[4], "assets/leaves.vox");
    render.load_block(&block_palette[5], "assets/light.vox");


    render.update_Block_Palette(block_palette);
    render.update_Material_Palette(render.mat_palette);

    // for(int i=0; i < BLOCK_PALETTE_SIZE; i++){
    // }
    // void* ptr = malloc((size_t)(void *)malloc); 
    // char* dump;
    // vmaBuildStatsString(render.VMAllocator, &dump, 1);
    // cout << dump;
    // vmaFreeStatsString(render.VMAllocator, dump);

    bool b = true;

    vkDeviceWaitIdle(render.device);
    while(!glfwWindowShouldClose(render.window.pointer) and glfwGetKey(render.window.pointer, GLFW_KEY_ESCAPE) != GLFW_PRESS){
        glfwPollEvents();
if((glfwGetKey(render.window.pointer, GLFW_KEY_D) == GLFW_PRESS) and b){
    robot.transform = translate(robot.transform, vec3(1,0,0));
    robot.transform = rotate(robot.transform, 0.01f, vec3(0,0,1));
    render.camera_pos.x += .9;
}
if((glfwGetKey(render.window.pointer, GLFW_KEY_SPACE) == GLFW_PRESS) and b){
    robot.transform = translate(robot.transform, vec3(0,0,.1));
    // robot.transform = rotate(robot.transform, 0.01f, vec3(0,0,1));
    // render.camera_pos.x += 1.0;
}
if((glfwGetKey(render.window.pointer, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS) and b){
    robot.transform = translate(robot.transform, vec3(0,0,-.1));
    // robot.transform = rotate(robot.transform, 0.01f, vec3(0,0,1));
    // render.camera_pos.x += 1.0;
}
if((glfwGetKey(render.window.pointer, GLFW_KEY_A) == GLFW_PRESS) and b){
    robot.transform = rotate(robot.transform, -0.01f, vec3(0,0,1));
    robot.transform = translate(robot.transform, vec3(-1,0,0));
    render.camera_pos.x -= .9;
}
if((glfwGetKey(render.window.pointer, GLFW_KEY_W) == GLFW_PRESS) and b){
    robot.transform = translate(robot.transform, vec3(0,1,0));
    render.camera_pos.y += .9;
}
if((glfwGetKey(render.window.pointer, GLFW_KEY_S) == GLFW_PRESS) and b){
    robot.transform = translate(robot.transform, vec3(0,-1,0));
    render.camera_pos.y -= .9;
}
if((glfwGetKey(render.window.pointer, GLFW_KEY_LEFT) == GLFW_PRESS) and b){
    // robot.transform = translate(robot.transform, vec3(0,-1,0));
    mat4 rot = rotate(identity<mat4>(), .01f, vec3(0,0,1));
    render.camera_dir = (rot * vec4(render.camera_dir, 1));
    // render.camera_pos.y -= 1.0;
}
if((glfwGetKey(render.window.pointer, GLFW_KEY_RIGHT) == GLFW_PRESS) and b){
    mat4 rot = rotate(identity<mat4>(), -.01f, vec3(0,0,1));
    render.camera_dir = (rot * vec4(render.camera_dir, 1));
}
render.camera_dir = normalize(render.camera_dir);
        // robot.transform = glm::translate(robot.transform, vec3(.01,0,0));
        // robot.transform = glm::rotate(robot.transform, 0.001f, vec3(0,0,1));
        
        render.start_Frame();
            render.startRaygen();
            // vec3 d = -vec3(.0001);
                for(int xx=0; xx<8; xx++){
                for(int yy=0; yy<8; yy++){
                for(int zz=0; zz<8; zz++){
                    Mesh* block_mesh = NULL;
                    
                    int block_id = render.origin_world(xx,yy,zz);
                    if(block_id != 0){
                        block_mesh = &block_palette[block_id]->mesh;

                        assert(block_mesh != NULL);
                    // assert(block_mesh != NULL);
                        block_mesh->    transform = translate(identity<mat4>(), vec3(xx*16,yy*16, zz*16));
                        block_mesh->old_transform = translate(identity<mat4>(), vec3(xx*16,yy*16, zz*16));
                    
                    render.RaygenMesh(block_mesh);
                    }
                }}}
            // robot.old_transform = robot.transform;
                render.RaygenMesh(&robot);
            render.endRaygen();

            render.startCompute();
                render.startBlockify();
                    render.blockifyMesh(&robot);
                render.endBlockify();

                render.startMap();
                    render.mapMesh(&robot);
                render.endMap();
                // robot.transform = glm::translate(robot.transform, -d*10.f);
                
                // render.recalculate_df();

                render.raytrace();
                render.denoise(3);
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