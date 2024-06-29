// #include <renderer/window.hpp>
// #include "main_pch.hpp"
#include <algorithm>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/scalar_constants.hpp>
#include <random>
#include <renderer/render.hpp>
#include <defines.hpp>
// #define GLM_FORCE_RIGHT_HANDED
// VisualWorld world = {};
Renderer render = {};
// int itimish = 0;
int iicounter = 0;
vec3 rndVec3(float minValue, float maxValue) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dis(minValue, maxValue);

    float x = dis(gen);
    float y = dis(gen);
    float z = dis(gen);

    return glm::vec3(x, y, z);
}
struct block_render{
    ivec3 pos;
    int index;
};
vector<block_render> que;
bool sort_blocks(struct block_render const& lhs, struct block_render const& rhs){
    return lhs.index < rhs.index;
}

quat find_quat(vec3 v1, vec3 v2){
        quat q;
        vec3 a = cross(v1, v2);
        q = a;
        q.w = sqrt((length(v1) * length(v1)) * (length(v2)*length(v2))) + dot(v1, v2);

        return normalize(q);
}
int main() {
    // render.init(48, 48, 16, BLOCK_PALETTE_SIZE, 8128, vec2(1) false);
    render.init(48, 48, 16, BLOCK_PALETTE_SIZE, 8128, vec2(2.0), true, false);
    vkDeviceWaitIdle(render.device);


    // table3d<BlockID_t> single_chunk = {};
    //user responsible for managing chunks somehow and also in future translating them to main chunk
    // single_chunk.allocate(16,16,16);
    // single_chunk.set(0);
    render.origin_world.set(0);

// println
    for(int xx=0; xx<48; xx++){
    for(int yy=0; yy<48; yy++){
        render.origin_world(xx, yy, 1) = (((xx+yy) % 2) == 0) ? 2 : 1;
    }}
    for(int xx=0; xx<48; xx++){
    for(int yy=0; yy<48; yy++){
        if(((xx+yy*yy) % 9) == 4){
            render.origin_world(xx, yy, 2) = 3;
        }
        if(((xx*yy / 2) % 17) == 6){
            render.origin_world(xx, yy, 2) = 5;
        }
        if(((2*xx/(yy+1)) % 11) == 5){
            render.origin_world(xx, yy, 2) = 4;
        }
    }}
// println
    Mesh tank_body = {};
    Mesh tank_head = {};
    Mesh tank_rf_leg = {};
    Mesh tank_lf_leg = {};
    Mesh tank_rb_leg = {};
    Mesh tank_lb_leg = {};
    vec2 points[4] = {}; // that each leg follows
    vec2 interpolated_points[4] = {}; // that each leg follows
    vec2 target_points[4]; // that each leg follows
    // Mesh tank_head = {};
        render.load_mesh(&tank_body, "assets/tank_body.vox");
        tank_body.shift += vec3(34.1,30.1,38.1);
        render.load_mesh(&tank_head, "assets/tank_head.vox");

        render.load_mesh(&tank_rf_leg, "assets/tank_rf_lb_leg.vox");
        render.load_mesh(&tank_lb_leg, "assets/tank_rf_lb_leg.vox");

        render.load_mesh(&tank_lf_leg, "assets/tank_lf_rb_leg.vox");
        render.load_mesh(&tank_rb_leg, "assets/tank_lf_rb_leg.vox");

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

    // p.pos += .1f;
    // render.particles.push_back(p);
    // p.pos += .1f;
    // render.particles.push_back(p);
    // p.pos += .1f;
    // render.particles.push_back(p);
    srand(666);
    vkDeviceWaitIdle(render.device);
    while(!glfwWindowShouldClose(render.window.pointer) and glfwGetKey(render.window.pointer, GLFW_KEY_ESCAPE) != GLFW_PRESS){
        glfwPollEvents();

        #define set_key(key, action) if(glfwGetKey(render.window.pointer, key) == GLFW_PRESS) {action;}
        set_key(GLFW_KEY_W, render.camera_pos += vec3(0,+1.,0));
        set_key(GLFW_KEY_S, render.camera_pos += vec3(0,-1.,0));
        set_key(GLFW_KEY_A, render.camera_pos += vec3(  -1.,0,0));
        set_key(GLFW_KEY_D, render.camera_pos += vec3(  +1.,0,0));
        set_key(GLFW_KEY_SPACE     , render.camera_pos += vec3(0,0,+10)/20.0f);
        set_key(GLFW_KEY_LEFT_SHIFT, render.camera_pos += vec3(0,0,-10)/20.0f);
        set_key(GLFW_KEY_COMMA  , render.camera_dir = rotate(identity<mat4>(), +0.01f, vec3(0,0,1)) * vec4(render.camera_dir,0));
        set_key(GLFW_KEY_PERIOD , render.camera_dir = rotate(identity<mat4>(), -0.01f, vec3(0,0,1)) * vec4(render.camera_dir,0));

        vec3 tank_direction_forward = tank_body.rot * vec3(0,1,0);
        vec3 tank_direction_right    = tank_body.rot * vec3(1,0,0); //
        // render.particles[0].vel = vec3(0);
        set_key(GLFW_KEY_LEFT      , tank_body.shift -=   tank_direction_right*0.4f);
        set_key(GLFW_KEY_RIGHT     , tank_body.shift +=   tank_direction_right*0.4f);
        set_key(GLFW_KEY_UP        , tank_body.shift += tank_direction_forward*0.4f);
        set_key(GLFW_KEY_DOWN      , tank_body.shift -= tank_direction_forward*0.4f);
        set_key(GLFW_KEY_LEFT_CONTROL       , tank_body.shift.z += 0.501f);
        set_key(GLFW_KEY_LEFT_ALT      , tank_body.shift.z -= 0.501f);


        if(glfwGetKey(render.window.pointer, GLFW_KEY_PAGE_UP) == GLFW_PRESS){
            quat old_rot = tank_body.rot;
            tank_body.rot *= quat(vec3(0,0,+.05));
            quat new_rot = tank_body.rot;

            vec3 old_center = old_rot * vec3(tank_body.size)/2.f;
            vec3 new_center = new_rot * vec3(tank_body.size)/2.f;

            vec3 difference = new_center - old_center;

            tank_body.shift -= difference;
        }
        if(glfwGetKey(render.window.pointer, GLFW_KEY_PAGE_DOWN) == GLFW_PRESS){
            quat old_rot = tank_body.rot;
            tank_body.rot *= quat(vec3(0,0,-.05));
            quat new_rot = tank_body.rot;

            vec3 old_center = old_rot * vec3(tank_body.size)/2.f;
            vec3 new_center = new_rot * vec3(tank_body.size)/2.f;

            vec3 difference = new_center - old_center;

            tank_body.shift -= difference;
        }
        // set_key(GLFW_KEY_PAGE_UP   , tank_body.rot *= quat(vec3(0,0,+.05)); tank_body.shift -= quat(vec3(0,0,+.05)) * vec3(tank_body.size)); //todo around center of smth
        // set_key(GLFW_KEY_PAGE_DOWN , tank_body.rot *= quat(vec3(0,0,-.05)); tank_body.shift -= quat(vec3(0,0,-.05)) * vec3(tank_body.size)); //todo around center of smth
        
        tank_body.rot = normalize(tank_body.rot);
        Particle p = {};
            p.lifeTime = 10.0 * (float(rand()) / float(RAND_MAX));
            p.pos = vec3(17,42,27) + tank_head.shift;
            p.vel = (rndVec3(0,1)*2.f - 1.f)*1.5f;
            // p.matID = 170;
            p.matID = 79;
        render.particles.push_back(p);
        if(iicounter++ % 100 == 0) {
                p.lifeTime = 7.0;
                p.pos = vec3(16.5,3,9.5) + tank_head.shift;
                p.matID = 249;
                p.vel = (rndVec3(0,1)*2.f - 1.f)*.5f + vec3(0,-1,0)*100.f;
            render.particles.push_back(p);

            p.lifeTime = 2.5;
            p.pos = vec3(16.5,3,9.5) + tank_head.shift;
            p.matID = 177;
            for(int i=0; i< 50; i++){
                p.vel = (rndVec3(0,1)*2.f - 1.f)*15.f;
            render.particles.push_back(p);
            }
        }
            
        //bind tank parts to body
        vec3 body_head_joint_shift = tank_body.rot * vec3(8.5,12.5, tank_body.size.z);
        vec3 head_joint_shift = tank_head.rot * vec3(33/2.0,63/2.0, 0);
        vec3 head_joint = tank_body.shift + body_head_joint_shift;

        tank_head.shift = head_joint - head_joint_shift;
        // tank_head.shift.y = head_joint.y - head_joint_shift.y;
        // tank_head.shift.z = head_joint.z;

        vec3 body_rf_leg_joint = tank_body.shift + tank_body.rot * vec3(14.0, 19.5, 6.5);
        vec3 body_lb_leg_joint = tank_body.shift + tank_body.rot * vec3( 3.0,  3.5, 6.5);
        vec3 body_lf_leg_joint = tank_body.shift + tank_body.rot * vec3( 3.0, 19.5, 6.5);
        vec3 body_rb_leg_joint = tank_body.shift + tank_body.rot * vec3(14.0,  3.5, 6.5);
        
        tank_rf_leg.rot = quat(-vec3(0,0,pi<float>())) * tank_body.rot;
        tank_lb_leg.rot = tank_body.rot;
        tank_lf_leg.rot = quat(-vec3(0,0,pi<float>())) * tank_body.rot;
        tank_rb_leg.rot = tank_body.rot;

        target_points[0] = vec2(body_rf_leg_joint) + vec2( tank_direction_right) + vec2(tank_direction_forward) * 3.0f * (float(rand()) / float(RAND_MAX));
        target_points[1] = vec2(body_lb_leg_joint) + vec2(-tank_direction_right) + vec2(tank_direction_forward) * 3.0f * (float(rand()) / float(RAND_MAX));
        target_points[2] = vec2(body_lf_leg_joint) + vec2(-tank_direction_right) + vec2(tank_direction_forward) * 3.0f * (float(rand()) / float(RAND_MAX));
        target_points[3] = vec2(body_rb_leg_joint) + vec2( tank_direction_right) + vec2(tank_direction_forward) * 3.0f * (float(rand()) / float(RAND_MAX));

        for (int i=0; i<4; i++){
            if(distance(points[i], target_points[i]) > 6.f) {
                points[i] = target_points[i];
            };
            interpolated_points[i] = mix(interpolated_points[i], points[i], 0.6);
        }
        
        vec2 rf_direction = normalize(interpolated_points[0] - vec2(body_rf_leg_joint));
        vec2 lb_direction = normalize(interpolated_points[1] - vec2(body_lb_leg_joint));
        vec2 lf_direction = normalize(interpolated_points[2] - vec2(body_lf_leg_joint));
        vec2 rb_direction = normalize(interpolated_points[3] - vec2(body_rb_leg_joint));
        //now legs are attached
        //to make them animated, we need to pick a point and try to follow it for while
        //we know tank direction
        quat rf_quat = find_quat(vec3( tank_direction_right), vec3(rf_direction,0));
        quat lb_quat = find_quat(vec3(-tank_direction_right), vec3(lb_direction,0));
        quat lf_quat = find_quat(vec3(-tank_direction_right), vec3(lf_direction,0));
        quat rb_quat = find_quat(vec3( tank_direction_right), vec3(rb_direction,0));

        tank_rf_leg.rot *= rf_quat;
        tank_lb_leg.rot *= lb_quat;
        tank_lf_leg.rot *= lf_quat;
        tank_rb_leg.rot *= rb_quat;

        vec3 rf_leg_joint_shift = tank_rf_leg.rot * vec3(10.0, 6.5, 12.5);
        vec3 lb_leg_joint_shift = tank_lb_leg.rot * vec3(10.0, 6.5, 12.5);
        vec3 lf_leg_joint_shift = tank_lf_leg.rot * vec3( 0.0, 6.5, 12.5);
        vec3 rb_leg_joint_shift = tank_rb_leg.rot * vec3( 0.0, 6.5, 12.5);

        tank_rf_leg.shift = body_rf_leg_joint - rf_leg_joint_shift;
        tank_lb_leg.shift = body_lb_leg_joint - lb_leg_joint_shift;
        tank_lf_leg.shift = body_lf_leg_joint - lf_leg_joint_shift;
        tank_rb_leg.shift = body_rb_leg_joint - rb_leg_joint_shift; 

        que.clear();
        for(int xx=0; xx<48; xx++){
        for(int yy=0; yy<48; yy++){
        for(int zz=0; zz<5; zz++){
            Mesh* block_mesh = NULL;
            
            int block_id = render.origin_world(xx,yy,zz);
            if(block_id != 0){
                struct block_render br = {};
                br.pos = ivec3(xx*16,yy*16, zz*16);
                br.index = block_id;
                que.push_back(br);
            }
        }}}
        std::sort(que.begin(), que.end(), &sort_blocks);

        render.start_Frame();
            render.startRaygen();
                for(auto b : que){
                    Mesh* block_mesh = NULL;
                    block_mesh = &block_palette[b.index]->mesh;
                    block_mesh->shift = vec3(b.pos);
                    block_mesh->old_shift = vec3(b.pos);

                    render.RaygenMesh(block_mesh);
                }
                
                render.RaygenMesh(&tank_body);
                render.RaygenMesh(&tank_head);
                
                render.RaygenMesh(&tank_rf_leg);
                render.RaygenMesh(&tank_lb_leg);
                render.RaygenMesh(&tank_lf_leg);
                render.RaygenMesh(&tank_rb_leg);
                render.rayGenMapParticles();
            render.endRaygen();

            render.startBlockify();
                render.blockifyMesh(&tank_body);
                render.blockifyMesh(&tank_head);
            
                render.blockifyMesh(&tank_rf_leg);
                render.blockifyMesh(&tank_lb_leg);
                render.blockifyMesh(&tank_lf_leg);
                render.blockifyMesh(&tank_rb_leg);
        render.endBlockify();

        render.updateParticles();

            render.startCompute();
            render.execCopies();


                render.startMap();
                    render.mapMesh(&tank_body);
                    render.mapMesh(&tank_head);
                    
                    render.mapMesh(&tank_rf_leg);
                    render.mapMesh(&tank_lb_leg);
                    render.mapMesh(&tank_lf_leg);
                    render.mapMesh(&tank_rb_leg);
                render.endMap();
                
                // render.recalculate_df();

                render.raytrace();
                if(render.is_scaled){
                    render.denoise(5, 2, DENOISE_TARGET_LOWRES);
                    render.upscale();
                }
                render.accumulate();
                render.denoise(2, 2, DENOISE_TARGET_HIGHRES);
            render.endCompute();

            render.present(); 
            // render.end_Present();
        render.end_Frame();
        
    vkDeviceWaitIdle(render.device);
    }
    //lol this was in main loop
    vkDeviceWaitIdle(render.device);

    //TODO: temp

    render.free_mesh(&tank_body);
    render.free_mesh(&tank_head);
    
    render.free_mesh(&tank_rf_leg);
    render.free_mesh(&tank_lb_leg);
    render.free_mesh(&tank_lf_leg);
    render.free_mesh(&tank_rb_leg);

    render.free_block(&block_palette[1]);
    render.free_block(&block_palette[2]);
    render.free_block(&block_palette[3]);
    render.free_block(&block_palette[4]);
    render.free_block(&block_palette[5]);
    vkDeviceWaitIdle(render.device);

    render.cleanup();
    return 0;
}