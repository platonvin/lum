#include "engine.hpp"

vec3 rnVec3(float minValue, float maxValue) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dis(minValue, maxValue);

    float x = dis(gen);
    float y = dis(gen);
    float z = dis(gen);

    return glm::vec3(x, y, z);
}

bool sort_blocks(struct block_render_request const& lhs, struct block_render_request const& rhs){
    return lhs.index < rhs.index;
}

quat find_quat(vec3 v1, vec3 v2){
        quat q;
        vec3 a = cross(v1, v2);
        q = a;
        q.w = sqrt((length(v1) * length(v1)) * (length(v2)*length(v2))) + dot(v1, v2);

        return normalize(q);
}

bool is_block_visible(dmat4 trans, dvec3 pos){
    
    for(int xx = 0; xx < 2; xx++){
    for(int yy = 0; yy < 2; yy++){
    for(int zz = 0; zz < 2; zz++){
        double x = double(xx) * 16.0;
        double y = double(yy) * 16.0;
        double z = double(zz) * 16.0;

        dvec3 new_pos = quat_identity<double, defaultp>() * pos;
        dvec4 corner = dvec4(new_pos + dvec3(x, y, z), 1.0);
        dvec4 clip = trans * corner;

        // Check if within NDC range
        if ((clip.x >= -1.0) && (clip.y >= -1.0) && (clip.z >= -1.0) && 
            (clip.x <= +1.0) && (clip.y <= +1.0) && (clip.z <= +1.0)) {
                return true;
        }
    }}}
    
    return false;
}

//just objects to render
Mesh tank_body = {};
Mesh tank_head = {};
Mesh tank_rf_leg = {};
Mesh tank_lf_leg = {};
Mesh tank_rb_leg = {};
Mesh tank_lb_leg = {};
vec2 physical_points[4] = {}; // that each leg follows
vec2 interpolated_leg_points[4] = {}; // that each leg follows
vec2 target_leg_points[4]; // that each leg follows
float physical_body_height;
float interpolated_body_height;
Block** block_palette = {};

static double curr_time = 0;
static double prev_time = 0;
static double delt_time = 0;
static double block_placement_delay = 0;

void Engine::setup_graphics(){
    render.init(48, 48, 16, 15, 8128, float(1.5), false, false);
// println
    vkDeviceWaitIdle(render.device);

    render.load_scene("assets/scene");
    // render.origin_world.set(0);
    // Mesh tank_head = {};
        render.load_mesh(&tank_body, "assets/tank_body.vox");
        tank_body.shift += vec3(13.1,14.1,3.1)*16.0f;
        render.load_mesh(&tank_head, "assets/tank_head.vox");

        render.load_mesh(&tank_rf_leg, "assets/tank_rf_lb_leg.vox");
        render.load_mesh(&tank_lb_leg, "assets/tank_rf_lb_leg.vox");

        render.load_mesh(&tank_lf_leg, "assets/tank_lf_rb_leg.vox");
        render.load_mesh(&tank_rb_leg, "assets/tank_lf_rb_leg.vox");
// println

    //TEMP: emtpy palette just to make things run. No static block atm
    block_palette = (Block**)calloc(render.static_block_palette_size, sizeof(Block*));
    
    //block_palette[0] is "air"
// println
    render.load_block(&block_palette[1], "assets/dirt.vox");
    render.load_block(&block_palette[2], "assets/grass.vox");
    render.load_block(&block_palette[3], "assets/grassNdirt.vox");
    render.load_block(&block_palette[4], "assets/stone_dirt.vox");
    render.load_block(&block_palette[5], "assets/bush.vox");
    render.load_block(&block_palette[6], "assets/leaves.vox");
    render.load_block(&block_palette[7], "assets/iron.vox");
    render.load_block(&block_palette[8], "assets/lamp.vox");
    render.load_block(&block_palette[9], "assets/stone_brick.vox");
    render.load_block(&block_palette[10], "assets/stone_brick_cracked.vox");
    render.load_block(&block_palette[11], "assets/stone_pack.vox");
    render.load_block(&block_palette[12], "assets/bark.vox");
    render.load_block(&block_palette[13], "assets/wood.vox");
    render.load_block(&block_palette[14], "assets/planks.vox");
// println

    render.update_Block_Palette(block_palette);
// println
    render.update_Material_Palette(render.mat_palette);
    vkDeviceWaitIdle(render.device);
// println
}
void Engine::setup_ui(){
// println
    ui.renderer = &render;
    ui.setup();
    vkDeviceWaitIdle(render.device);
// println
}

void Engine::update_system(){
    glfwPollEvents();
    should_close |= glfwWindowShouldClose(render.window.pointer);
}

// void Engine::update(){

// }

void Engine::handle_input(){
    should_close |= (glfwGetKey(render.window.pointer, GLFW_KEY_ESCAPE) == GLFW_PRESS);
    
    #define set_key(key, action) if(glfwGetKey(render.window.pointer, key) == GLFW_PRESS) {action;}
    set_key(GLFW_KEY_W, render.camera_pos += dvec3(dvec2(render.camera_dir),0) * 1.4);
    set_key(GLFW_KEY_S, render.camera_pos -= dvec3(dvec2(render.camera_dir),0) * 1.4);

    dvec3 camera_direction_to_right = dquat(dvec3(0.0, 0.0, pi<double>()/2.0)) * render.camera_dir;

    set_key(GLFW_KEY_A, render.camera_pos += dvec3(dvec2(camera_direction_to_right),0) * 1.4);
    set_key(GLFW_KEY_D, render.camera_pos -= dvec3(dvec2(camera_direction_to_right),0) * 1.4);
    // set_key(GLFW_KEY_SPACE     , render.camera_pos += vec3(0,0,+10)/20.0f);
    // set_key(GLFW_KEY_LEFT_SHIFT, render.camera_pos += vec3(0,0,-10)/20.0f);
    set_key(GLFW_KEY_COMMA  , render.camera_dir = rotate(identity<mat4>(), +0.01f, vec3(0,0,1)) * vec4(render.camera_dir,0));
    set_key(GLFW_KEY_PERIOD , render.camera_dir = rotate(identity<mat4>(), -0.01f, vec3(0,0,1)) * vec4(render.camera_dir,0));

    vec3 tank_direction_forward = tank_body.rot * vec3(0,1,0);
    vec3 tank_direction_right    = tank_body.rot * vec3(1,0,0); //
    // render.particles[0].vel = vec3(0);
    float tank_speed = 50.0 * delt_time;

    // set_key(GLFW_KEY_LEFT_SHIFT, tank_speed = 1.5);
    // set_key(GLFW_KEY_LEFT      , tank_body.shift -=   tank_direction_right*tank_speed);
    // set_key(GLFW_KEY_RIGHT     , tank_body.shift +=   tank_direction_right*tank_speed);
    set_key(GLFW_KEY_UP        , tank_body.shift += tank_direction_forward*tank_speed);
    set_key(GLFW_KEY_DOWN      , tank_body.shift -= tank_direction_forward*tank_speed);
    // set_key(GLFW_KEY_LEFT_CONTROL  , tank_body.shift.z += tank_speed);
    // set_key(GLFW_KEY_LEFT_ALT      , tank_body.shift.z -= tank_speed);

    ivec3 block_to_set = ivec3(vec3(tank_body.shift) + tank_body.rot*(vec3(tank_body.size)/2.0f))/16;
    block_to_set = clamp(block_to_set, ivec3(0), render.world_size-1);

    // if(block_placement_delay < 0){
        for(int i=1; i<10; i++){
            set_key(GLFW_KEY_0+i, render.origin_world(block_to_set.x, block_to_set.y, block_to_set.z) = 0+i);
        }
        for(int i=0; i<5; i++){
            set_key(GLFW_KEY_F1+i, render.origin_world(block_to_set.x, block_to_set.y, block_to_set.z) = 10+i);
        }    
        set_key(GLFW_KEY_0, render.origin_world(block_to_set.x, block_to_set.y, block_to_set.z-1) = 0);
        block_placement_delay = 0.1;
    // } else {
    //     block_placement_delay -= delt_time;
    // }

    if(glfwGetKey(render.window.pointer, GLFW_KEY_LEFT) == GLFW_PRESS){
        quat old_rot = tank_body.rot;
        tank_body.rot *= quat(vec3(0,0,+.03 * 50.0*delt_time));
        quat new_rot = tank_body.rot;

        vec3 old_center = old_rot * vec3(tank_body.size)/2.f;
        vec3 new_center = new_rot * vec3(tank_body.size)/2.f;

        vec3 difference = new_center - old_center;

        tank_body.shift -= difference;
    }
    if(glfwGetKey(render.window.pointer, GLFW_KEY_RIGHT) == GLFW_PRESS){
        quat old_rot = tank_body.rot;
        tank_body.rot *= quat(vec3(0,0,-.03 * 50.0*delt_time));
        quat new_rot = tank_body.rot;

        vec3 old_center = old_rot * vec3(tank_body.size)/2.f;
        vec3 new_center = new_rot * vec3(tank_body.size)/2.f;

        vec3 difference = new_center - old_center;

        tank_body.shift -= difference;
    }
    tank_body.rot = normalize(tank_body.rot);
}

void Engine::process_physics(){
     vec3 tank_body_center = tank_body.rot * vec3(8.5,12.5, 0) + tank_body.shift;
    ivec2 tank_body_center_in_blocks = ivec2(tank_body_center) / 16;
    int block_under_body = int(tank_body_center.z) / 16;

    if(any(lessThan(tank_body_center_in_blocks, ivec2(0)))) return;
    if(any(greaterThanEqual(tank_body_center_in_blocks, ivec2(render.world_size)))) return;

    int selected_block = 0;
    
    if(render.origin_world(tank_body_center_in_blocks.x, tank_body_center_in_blocks.y, block_under_body+1) != 0){
        selected_block = block_under_body+1;
    } else if(render.origin_world(tank_body_center_in_blocks.x, tank_body_center_in_blocks.y, block_under_body) != 0){
        selected_block = block_under_body;
    }else if(render.origin_world(tank_body_center_in_blocks.x, tank_body_center_in_blocks.y, block_under_body-1) != 0){
        selected_block = block_under_body-1;
    } else {
        for(selected_block=render.world_size.z-1; selected_block>=0; selected_block--){
            BlockID_t blockId = render.origin_world(tank_body_center_in_blocks.x, tank_body_center_in_blocks.y, selected_block);

            if (blockId != 0) break;
        }
    }

    physical_body_height = float(selected_block)*16.0 + 16.0;
    // tank_body.shift.z = float(selected_block)*16.0 + 16.0;
}

void Engine::process_animations(){
    vec3 tank_direction_forward = tank_body.rot * vec3(0,1,0);
    vec3 tank_direction_right    = tank_body.rot * vec3(1,0,0); //

    Particle p = {};
        p.lifeTime = 10.0 * (float(rand()) / float(RAND_MAX));
        p.pos = tank_head.rot*vec3(17,42,27) + tank_head.shift;
        p.vel = (rnVec3(0,1)*2.f - 1.f)*0.1f;
        // p.matID = 170;
        p.matID = 79;
    render.particles.push_back(p);
    if(glfwGetKey(render.window.pointer, GLFW_KEY_ENTER) == GLFW_PRESS) {
            p.lifeTime = 12.0;
            p.pos = tank_head.rot*vec3(16.5,3,9.5) + tank_head.shift;
            p.matID = 249;
            p.vel = (rnVec3(0,1)*2.f - 1.f)*.5f + tank_head.rot*vec3(0,-1,0)*100.f;
        render.particles.push_back(p);

        p.lifeTime = 2.5;
        // p.pos = vec3(16.5,3,9.5) + tank_head.shift;
        p.matID = 19;
        for(int i=0; i< 30; i++){
            p.vel = (rnVec3(0,1)*2.f - 1.f)*25.f;
        render.particles.push_back(p);
        }
    }
    tank_head.rot = normalize(glm::mix(tank_head.rot, quat(vec3(0,0,pi<float>()))*tank_body.rot, 0.05f));
    // tank_head.rot*=quat(vec3(0,0,pi<float>()));

    interpolated_body_height = glm::mix(interpolated_body_height, physical_body_height, 0.05);
    tank_body.shift.z = interpolated_body_height;
        
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

    target_leg_points[0] = vec2(body_rf_leg_joint) + vec2( tank_direction_right) + vec2(tank_direction_forward) * 3.0f * (float(rand()) / float(RAND_MAX));
    target_leg_points[1] = vec2(body_lb_leg_joint) + vec2(-tank_direction_right) + vec2(tank_direction_forward) * 3.0f * (float(rand()) / float(RAND_MAX));
    target_leg_points[2] = vec2(body_lf_leg_joint) + vec2(-tank_direction_right) + vec2(tank_direction_forward) * 3.0f * (float(rand()) / float(RAND_MAX));
    target_leg_points[3] = vec2(body_rb_leg_joint) + vec2( tank_direction_right) + vec2(tank_direction_forward) * 3.0f * (float(rand()) / float(RAND_MAX));

    for (int i=0; i<4; i++){
        if(distance(physical_points[i], target_leg_points[i]) > 6.f) {
            physical_points[i] = target_leg_points[i];
        };
        interpolated_leg_points[i] = mix(interpolated_leg_points[i], physical_points[i], 0.6);
    }
    
    vec2 rf_direction = normalize(interpolated_leg_points[0] - vec2(body_rf_leg_joint));
    vec2 lb_direction = normalize(interpolated_leg_points[1] - vec2(body_lb_leg_joint));
    vec2 lf_direction = normalize(interpolated_leg_points[2] - vec2(body_lf_leg_joint));
    vec2 rb_direction = normalize(interpolated_leg_points[3] - vec2(body_rb_leg_joint));
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
}

void Engine::cull_meshes(){
    que.clear();
    for(int xx=0; xx<48; xx++){
    for(int yy=0; yy<48; yy++){
    for(int zz=0; zz<16;  zz++){
        Mesh* block_mesh = NULL;
        
        int block_id = render.origin_world(xx,yy,zz);
        if(block_id != 0){
            struct block_render_request /*goes*/ brr = {}; //
            brr.pos = ivec3(xx*16,yy*16, zz*16);
            brr.index = block_id;
            if(is_block_visible(render.current_trans, dvec3(brr.pos))){
                que.push_back(brr);
            }
        }
    }}}
    std::sort(que.begin(), que.end(), &sort_blocks);
}

void Engine::draw()
{
// println
    render.start_Frame();
        render.startRaygen();
            for(auto b : que){
                Mesh* block_mesh = NULL;
                block_mesh = &block_palette[b.index]->mesh;
                block_mesh->shift = vec3(b.pos);
                block_mesh->old_shift = vec3(b.pos);

                // printl(block_palette[b.index]->mesh.triangles.Pzz.icount);
                
                render.RaygenMesh(block_mesh);
            }
            
            render.RaygenMesh(&tank_body);
            render.RaygenMesh(&tank_head);
            
            render.RaygenMesh(&tank_rf_leg);
            render.RaygenMesh(&tank_lb_leg);
// println
            render.RaygenMesh(&tank_lf_leg);
            render.RaygenMesh(&tank_rb_leg);
            render.rayGenMapParticles();
// println
        render.endRaygen();
// println

        render.startBlockify();
// println
            render.blockifyMesh(&tank_body);
            render.blockifyMesh(&tank_head);
        
            render.blockifyMesh(&tank_rf_leg);
            render.blockifyMesh(&tank_lb_leg);
            render.blockifyMesh(&tank_lf_leg);
            render.blockifyMesh(&tank_rb_leg);
        render.endBlockify();
// println

        render.updateParticles();
// println

        render.startCompute();
// println
            render.execCopies();
// println
                render.startMap();
// println
                    render.mapMesh(&tank_body);
                    render.mapMesh(&tank_head);
                    
                    render.mapMesh(&tank_rf_leg);
// println
                    render.mapMesh(&tank_lb_leg);
                    render.mapMesh(&tank_lf_leg);
                    render.mapMesh(&tank_rb_leg);
// println
                render.endMap();
// println
                // render.raytrace();
                render.updadeRadiance();
                render.doLight();
// println
                // render.denoise(render.pre_denoiser_count, 1, render.is_scaled? DENOISE_TARGET_LOWRES : DENOISE_TARGET_HIGHRES);
// println
                render.accumulate();
// println
                // render.denoise(render.post_denoiser_count, 1, render.is_scaled? DENOISE_TARGET_LOWRES : DENOISE_TARGET_HIGHRES);
// println
                // render.denoise(7, 2, DENOISE_TARGET_LOWRES);
                // render.denoise(6, 2, DENOISE_TARGET_LOWRES);
                // render.denoise(5, 2, DENOISE_TARGET_LOWRES);
                if(render.is_scaled){
                    // render.denoise(9, 3, DENOISE_TARGET_LOWRES);
                    // render.denoise(5, 1, DENOISE_TARGET_LOWRES);
                    render.upscale();
                }
// println
                // render.denoise(render.final_denoiser_count, 2, DENOISE_TARGET_HIGHRES);
                // render.denoise(1, 2, DENOISE_TARGET_HIGHRES);
                // render.denoise(3, 2, DENOISE_TARGET_HIGHRES);
// println
        render.endCompute();

// println
        render.start_ui(); 
// println
            ui.update();
// println
            ui.draw();
// println
        render.draw_ui(); 
// println
        render.present();
    render.end_Frame();
}

void Engine::setup(){
    setup_graphics();
    setup_ui();
}

void Engine::update(){
    prev_time = curr_time;
    curr_time = glfwGetTime();
    delt_time = curr_time-prev_time;
    render.delta_time = delt_time;

// println
    update_system();
    handle_input();
    process_physics();
    process_animations();
    cull_meshes();
    draw();
}

void Engine::cleanup(){
    vkDeviceWaitIdle(render.device);

    render.save_scene("assets/scene");

    render.free_mesh(&tank_body);
    render.free_mesh(&tank_head);
    render.free_mesh(&tank_rf_leg);
    render.free_mesh(&tank_lb_leg);
    render.free_mesh(&tank_lf_leg);
    render.free_mesh(&tank_rb_leg);

    for(int i=1; i<render.static_block_palette_size; i++){
        render.free_block(&block_palette[i]);
    }

    vkDeviceWaitIdle(render.device);
    ui.cleanup();
    render.cleanup();
}