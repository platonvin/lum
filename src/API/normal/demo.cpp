#include "input/input.hpp"
#include "lum.hpp"
// #include "defines/macros.hpp"
#include <glm/gtx/quaternion.hpp>
#include <random>

vec3 rnVec3(float minValue, float maxValue);
void printFPS();
void setup_input(Input& input, Lum& render);
void process_animations(Lum& lum);
void process_physics(Lum& lum);
void cleanup(Lum& lum);

// call me evil
// in real world you would store them as ECS components / member in EntityClass
// but for simplicity lets stick to just this
MeshModel tank_body = {};
MeshTransform tank_body_trans = {};
MeshModel tank_head = {};
MeshTransform tank_head_trans = {};
MeshModel tank_rf_leg = {};
MeshTransform tank_rf_leg_trans = {};
MeshModel tank_lf_leg = {};
MeshTransform tank_lf_leg_trans = {};
MeshModel tank_rb_leg = {};
MeshTransform tank_rb_leg_trans = {};
MeshModel tank_lb_leg = {};
MeshTransform tank_lb_leg_trans = {};
vec2 physical_points[4] = {}; // that each leg follows
vec2 interpolated_leg_points[4] = {}; // that each leg follows
vec2 target_leg_points[4]; // that each leg follows
float physical_body_height;
float interpolated_body_height;

int main(){
    // input system i designed. You can use, but it is not neccessary for Lum
    Input input;

    Lum render = {};
        render.settings.fullscreen = false;
        render.settings.vsync = false;
        render.settings.world_size = ivec3(48, 48, 16);
        render.settings.static_block_palette_size = 15;
        render.settings.maxParticleCount = 8128;
    render.init();

    // preparation stage. These functions also can be called in runtime
    // If no palette is found, first models defines it

    // this DOES set palette
    tank_body = render.loadMesh("assets/tank_body.vox", /*extract palette if no found = */ true);
    tank_body_trans.shift += vec3(13.1,14.1,3.1)*16.0f;
    // this DOES NOT set palette cause already setten
    tank_head = render.loadMesh("assets/tank_head.vox", /*extract palette if no found = */ true);

    tank_rf_leg = render.loadMesh("assets/tank_rf_lb_leg.vox");
    tank_lb_leg = render.loadMesh("assets/tank_rf_lb_leg.vox");

    tank_lf_leg = render.loadMesh("assets/tank_lf_rb_leg.vox");
    tank_rb_leg = render.loadMesh("assets/tank_lf_rb_leg.vox");
    
    MeshFoliage grass = render.loadFoliage("", 6, 10);
    MeshLiquid  water = render.loadLiquid(69, 42);
    MeshVolumetric smoke = render.loadVolumetric(1, .5, {});

    render.loadBlock(  1, "assets/dirt.vox");
    render.loadBlock(  2, "assets/grass.vox");
    render.loadBlock(  3, "assets/grassNdirt.vox");
    render.loadBlock(  4, "assets/stone_dirt.vox");
    render.loadBlock(  5, "assets/bush.vox");
    render.loadBlock(  6, "assets/leaves.vox");
    render.loadBlock(  7, "assets/iron.vox");
    render.loadBlock(  8, "assets/lamp.vox");
    render.loadBlock(  9, "assets/stone_brick.vox");
    render.loadBlock( 10, "assets/stone_brick_cracked.vox");
    render.loadBlock( 11, "assets/stone_pack.vox");
    render.loadBlock( 12, "assets/bark.vox");
    render.loadBlock( 13, "assets/wood.vox");
    render.loadBlock( 14, "assets/planks.vox");

    // literally uploads data to gpu. You can call them in runtime, but atm its not recommended (a lot of overhead curently, can be significantly reduced)
    render.uploadBlockPaletteToGPU();
    render.uploadMaterialPaletteToGPU();

    render.render.render.deviceWaitIdle();

    setup_input(input, render);

    MeshTransform trans = {};
    trans.shift += vec3(13.1,14.1,3.1)*16.0f;
    while(not render.should_close){
        input.pollUpdates();
        glfwPollEvents();
        render.should_close |= glfwWindowShouldClose(render.getGLFWptr());
        render.should_close |= (glfwGetKey(render.getGLFWptr(), GLFW_KEY_ESCAPE) == GLFW_PRESS);

        process_physics(render);
        process_animations(render);

        render.startFrame();
        // this *could* be implicit
        render.drawWorld();
        render.drawParticles();
        
        render.drawMesh(tank_body, &tank_body_trans);
        render.drawMesh(tank_head, &tank_head_trans);
        render.drawMesh(tank_rf_leg, &tank_rf_leg_trans);
        render.drawMesh(tank_lf_leg, &tank_lf_leg_trans);
        render.drawMesh(tank_rb_leg, &tank_rb_leg_trans);
        render.drawMesh(tank_lb_leg, &tank_lb_leg_trans);

        // literally procedural grass generation every frame. You probably want to store it as entities in your own structures
        for(int xx=4; xx<20; xx++){
        for(int yy=4; yy<20; yy++){
            if((xx>=5) and (xx<12) and (yy>=6) and (yy<16)) continue;
            ivec3 pos = ivec3(xx*16, yy*16, 16);
            render.drawFoliageBlock(grass, pos);
        }}

        // literally procedural water generation every frame. You probably want to store it as entities in your own structures
        for(int xx=5; xx<12; xx++){
        for(int yy=6; yy<16; yy++){
            ivec3 pos = ivec3(xx*16, yy*16, 14);
            render.drawLiquidBlock(water, pos);
            render.render.specialRadianceUpdates.push_back(i8vec4(ivec3(xx,yy,2),0));
            render.render.specialRadianceUpdates.push_back(i8vec4(ivec3(xx,yy,1),0));
        }}

        // literally procedural smoke generation every frame. You probably want to store it as entities in your own structures
        for(int xx=8; xx<10; xx++){
        for(int yy=10; yy<13; yy++){
            ivec3 pos = ivec3(xx*16, yy*16, 20);
            render.drawVolumetricBlock(smoke, pos);
            // render.render.specialRadianceUpdates.push_back(i8vec4(ivec3(xx,yy,2),0));
            // render.render.specialRadianceUpdates.push_back(i8vec4(ivec3(xx,yy,1),0));
        }}

        render.prepareFrame();
        render.submitFrame();
    }
    cleanup(render);
}

// binding keys to some functions
void setup_input(Input& input, Lum& render) {
    input.setup(render.getGLFWptr());
    
    input.rebindKey(GameAction::MOVE_CAMERA_FORWARD, GLFW_KEY_W);
    input.rebindKey(GameAction::MOVE_CAMERA_BACKWARD, GLFW_KEY_S);
    input.rebindKey(GameAction::MOVE_CAMERA_LEFT, GLFW_KEY_A);
    input.rebindKey(GameAction::MOVE_CAMERA_RIGHT, GLFW_KEY_D);
    input.rebindKey(GameAction::TURN_CAMERA_LEFT, GLFW_KEY_COMMA);
    input.rebindKey(GameAction::TURN_CAMERA_RIGHT, GLFW_KEY_PERIOD);

    input.rebindKey(GameAction::INCREASE_ZOOM, GLFW_KEY_PAGE_UP);
    input.rebindKey(GameAction::DECREASE_ZOOM, GLFW_KEY_PAGE_DOWN);

    input.rebindKey(GameAction::SHOOT, GLFW_KEY_ENTER);
    input.rebindKey(GameAction::MOVE_TANK_FORWARD, GLFW_KEY_UP);
    input.rebindKey(GameAction::MOVE_TANK_BACKWARD, GLFW_KEY_DOWN);
    input.rebindKey(GameAction::TURN_TANK_LEFT, GLFW_KEY_LEFT);
    input.rebindKey(GameAction::TURN_TANK_RIGHT, GLFW_KEY_RIGHT);

    input.rebindKey(GameAction::SET_BLOCK_1, GLFW_KEY_1);
    input.rebindKey(GameAction::SET_BLOCK_2, GLFW_KEY_2);
    input.rebindKey(GameAction::SET_BLOCK_3, GLFW_KEY_3);
    input.rebindKey(GameAction::SET_BLOCK_4, GLFW_KEY_4);
    input.rebindKey(GameAction::SET_BLOCK_5, GLFW_KEY_5);
    input.rebindKey(GameAction::SET_BLOCK_6, GLFW_KEY_6);
    input.rebindKey(GameAction::SET_BLOCK_7, GLFW_KEY_7);
    input.rebindKey(GameAction::SET_BLOCK_8, GLFW_KEY_8);
    input.rebindKey(GameAction::SET_BLOCK_9, GLFW_KEY_9);
    input.rebindKey(GameAction::SET_BLOCK_0, GLFW_KEY_0);
    input.rebindKey(GameAction::SET_BLOCK_F1, GLFW_KEY_F1);
    input.rebindKey(GameAction::SET_BLOCK_F2, GLFW_KEY_F2);
    input.rebindKey(GameAction::SET_BLOCK_F3, GLFW_KEY_F3);
    input.rebindKey(GameAction::SET_BLOCK_F4, GLFW_KEY_F4);
    input.rebindKey(GameAction::SET_BLOCK_F5, GLFW_KEY_F5);

    // Continuous is default
    input.setActionType(GameAction::SHOOT, ActionType::OneShot);
    input.setActionType(GameAction::SET_BLOCK_1, ActionType::OneShot);
    input.setActionType(GameAction::SET_BLOCK_2, ActionType::OneShot);
    input.setActionType(GameAction::SET_BLOCK_3, ActionType::OneShot);
    input.setActionType(GameAction::SET_BLOCK_4, ActionType::OneShot);
    input.setActionType(GameAction::SET_BLOCK_5, ActionType::OneShot);
    input.setActionType(GameAction::SET_BLOCK_6, ActionType::OneShot);
    input.setActionType(GameAction::SET_BLOCK_7, ActionType::OneShot);
    input.setActionType(GameAction::SET_BLOCK_8, ActionType::OneShot);
    input.setActionType(GameAction::SET_BLOCK_9, ActionType::OneShot);
    input.setActionType(GameAction::SET_BLOCK_0, ActionType::OneShot);
    input.setActionType(GameAction::SET_BLOCK_F1, ActionType::OneShot);
    input.setActionType(GameAction::SET_BLOCK_F2, ActionType::OneShot);
    input.setActionType(GameAction::SET_BLOCK_F3, ActionType::OneShot);
    input.setActionType(GameAction::SET_BLOCK_F4, ActionType::OneShot);
    input.setActionType(GameAction::SET_BLOCK_F5, ActionType::OneShot);

    // bind action callbacks
    input.setActionCallback(GameAction::MOVE_CAMERA_FORWARD, [&](GameAction action) {
        render.render.camera.cameraPos += render.delt_time * dvec3(dvec2(render.render.camera.cameraDir), 0) * 400.5 / render.render.camera.pixelsInVoxel;
    });

    input.setActionCallback(GameAction::MOVE_CAMERA_BACKWARD, [&](GameAction action) {
        render.render.camera.cameraPos -= render.delt_time * dvec3(dvec2(render.render.camera.cameraDir), 0) * 400.5 / render.render.camera.pixelsInVoxel;
    });

    input.setActionCallback(GameAction::MOVE_CAMERA_LEFT, [&](GameAction action) {
        dvec3 camera_direction_to_right = glm::dquat(dvec3(0.0, 0.0, glm::pi<double>() / 2.0)) * render.render.camera.cameraDir;
        render.render.camera.cameraPos += render.delt_time * dvec3(dvec2(camera_direction_to_right), 0) * 400.5 / render.render.camera.pixelsInVoxel;
    });
    input.setActionCallback(GameAction::MOVE_CAMERA_RIGHT, [&](GameAction action) {
        dvec3 camera_direction_to_right = glm::dquat(dvec3(0.0, 0.0, glm::pi<double>() / 2.0)) * render.render.camera.cameraDir;
        render.render.camera.cameraPos -= render.delt_time * dvec3(dvec2(camera_direction_to_right), 0) * 400.5 / render.render.camera.pixelsInVoxel;
    });

    input.setActionCallback(GameAction::TURN_CAMERA_LEFT, [&](GameAction action) {
        render.render.camera.cameraDir = rotate(glm::identity<dmat4>(), +0.60 * render.delt_time, dvec3(0, 0, 1)) * dvec4(render.render.camera.cameraDir, 0);
        render.render.camera.cameraDir = normalize(render.render.camera.cameraDir);
    });
    input.setActionCallback(GameAction::TURN_CAMERA_RIGHT, [&](GameAction action) {
        render.render.camera.cameraDir = rotate(glm::identity<dmat4>(), -0.60 * render.delt_time, dvec3(0, 0, 1)) * dvec4(render.render.camera.cameraDir, 0);
        render.render.camera.cameraDir = normalize(render.render.camera.cameraDir);
    });

    input.setActionCallback(GameAction::MOVE_TANK_FORWARD, [&](GameAction action) {
        vec3 tank_direction_forward = tank_body_trans.rot * vec3(0, 1, 0);
        float tank_speed = 50.0 * render.delt_time;
        tank_body_trans.shift += tank_direction_forward * tank_speed;
    });
    input.setActionCallback(GameAction::MOVE_TANK_BACKWARD, [&](GameAction action) {
        vec3 tank_direction_forward = tank_body_trans.rot * vec3(0, 1, 0);
        float tank_speed = 50.0 * render.delt_time;
        tank_body_trans.shift -= tank_direction_forward * tank_speed;
    });

    input.setActionCallback(GameAction::TURN_TANK_LEFT, [&](GameAction action) {
        // assert(tank_body);
        quat old_rot = tank_body_trans.rot;
        tank_body_trans.rot *= quat(vec3(0, 0, +0.05f * 50.0f * render.delt_time));
        quat new_rot = tank_body_trans.rot;

        vec3 old_center = old_rot * vec3(tank_body->size) / 2.0f;
        vec3 new_center = new_rot * vec3(tank_body->size) / 2.0f;

        vec3 difference = new_center - old_center;
        tank_body_trans.shift -= difference;

        tank_body_trans.rot = normalize(tank_body_trans.rot);
    });

    input.setActionCallback(GameAction::TURN_TANK_RIGHT, [&](GameAction action) {
        quat old_rot = tank_body_trans.rot;
        tank_body_trans.rot *= quat(vec3(0, 0, -0.05f * 50.0f * render.delt_time));
        quat new_rot = tank_body_trans.rot;

        vec3 old_center = old_rot * vec3(tank_body->size) / 2.0f;
        vec3 new_center = new_rot * vec3(tank_body->size) / 2.0f;

        vec3 difference = new_center - old_center;
        tank_body_trans.shift -= difference;

        tank_body_trans.rot = normalize(tank_body_trans.rot);
    });

    for (int i = 1; i < 10; ++i) {
        input.setActionCallback(static_cast<GameAction>(GameAction::SET_BLOCK_1 + (i - 1)), [&, i](GameAction action) {
            ivec3 block_to_set = ivec3(vec3(tank_body_trans.shift) + tank_body_trans.rot*(vec3(tank_body->size)/2.0f))/16;
                block_to_set = clamp(block_to_set, ivec3(0), ivec3(render.render.world_size)-ivec3(1));
            render.setWorldBlock(block_to_set.x, block_to_set.y, block_to_set.z, i);
        });
        input.rebindKey(static_cast<GameAction>(GameAction::SET_BLOCK_1 + (i - 1)), GLFW_KEY_0 + i);
    }

    // for 0 key (special case for air block)
    input.setActionCallback(GameAction::SET_BLOCK_0, [&](GameAction action) {
        ivec3 block_to_set = ivec3(vec3(tank_body_trans.shift) + tank_body_trans.rot*(vec3(tank_body->size)/2.0f))/16;
            block_to_set = clamp(block_to_set, ivec3(0), ivec3(render.render.world_size)-ivec3(1));
        render.setWorldBlock(block_to_set.x, block_to_set.y, block_to_set.z - 1, 0);
    });
    input.rebindKey(GameAction::SET_BLOCK_0, GLFW_KEY_0);

    // For F1-F5 keys (GLFW_KEY_F1 to GLFW_KEY_F5)
    for (int i = 0; i < 5; ++i) {
        input.setActionCallback(static_cast<GameAction>(GameAction::SET_BLOCK_F1 + i), [&, i](GameAction action) {
            ivec3 block_to_set = ivec3(vec3(tank_body_trans.shift) + tank_body_trans.rot*(vec3(tank_body->size)/2.0f))/16;
                block_to_set = clamp(block_to_set, ivec3(0), ivec3(render.render.world_size)-ivec3(1));
            render.setWorldBlock(block_to_set.x, block_to_set.y, block_to_set.z, 10 + i);
        });
        input.rebindKey(static_cast<GameAction>(GameAction::SET_BLOCK_F1 + i), GLFW_KEY_F1 + i);
    }


    input.setActionCallback(GameAction::SHOOT, [&](GameAction action) {
        Particle 
            p = {};
            p.lifeTime = 12.0;
            p.pos = tank_head_trans.rot*vec3(16.5,3,9.5) + tank_head_trans.shift;
            p.matID = 249;
            p.vel = (rnVec3(0,1)*2.f - 1.f)*.5f + tank_head_trans.rot*vec3(0,-1,0)*100.f;
        render.render.particles.push_back(p);

        p.lifeTime = 2.5;
        p.matID = 19;
        for(int i=0; i< 30; i++){
            p.vel = (rnVec3(0,1)*2.f - 1.f)*25.f;
        render.render.particles.push_back(p);
        }
    });

    input.setActionCallback(GameAction::INCREASE_ZOOM, [&](GameAction action){
            render.render.camera.pixelsInVoxel *= 1.0 + render.delt_time;
    });
    input.setActionCallback(GameAction::DECREASE_ZOOM, [&](GameAction action){
            render.render.camera.pixelsInVoxel /= 1.0 + render.delt_time;
    });
}

#include <chrono>
void printFPS() {
    static int frame_count = 0;
    static auto last_print = std::chrono::high_resolution_clock::now();
    static auto last_frame = std::chrono::high_resolution_clock::now();
    static double  best_mspf = 1;
    static double worst_mspf = 0;
    
    frame_count++;
    
    auto current_time = std::chrono::high_resolution_clock::now();
    std::chrono::duration<double> last_print_elapsed = current_time - last_print;
    std::chrono::duration<double> last_frame_elapsed = current_time - last_frame;
    
    best_mspf = std::min(best_mspf, last_frame_elapsed.count());
    worst_mspf = std::max(best_mspf, last_frame_elapsed.count());

    if (last_print_elapsed.count() >= 1.0) {
        std::cout << ", " << frame_count;
        std::cout << ", " << 1000.0*best_mspf;
        std::cout << ", " << 1000.0*worst_mspf;
        frame_count = 0;
        last_print = current_time;

        best_mspf = 1;
        worst_mspf = 0;
    }
    last_frame = current_time;
}

vec3 rnVec3(float minValue, float maxValue) {
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_real_distribution<float> dis(minValue, maxValue);

    float x = dis(gen);
    float y = dis(gen);
    float z = dis(gen);

    return glm::vec3(x, y, z);
}

void cleanup(Lum& render){
    render.waitIdle();

    // save_scene functions are not really supposed to be used, i implemented them for demo
    render.render.save_scene("assets/scene");

    render.freeMesh(tank_body);
    render.freeMesh(tank_head);
    render.freeMesh(tank_rf_leg);
    render.freeMesh(tank_lb_leg);
    render.freeMesh(tank_lf_leg);
    render.freeMesh(tank_rb_leg);

    render.waitIdle();

    render.cleanup();
}

int findBlockUnderTank(Lum& lum, const ivec2& block_center, int block_under_body) {
    int selected_block = 0;
    for (int i = -1; i <= 1; ++i) {
        if (lum.getWorldBlock(block_center.x, block_center.y, block_under_body + i) != 0) {
            return block_under_body + i;
        }
    }
    for (selected_block = lum.render.world_size.z - 1; selected_block >= 0; --selected_block) {
        if (lum.getWorldBlock(block_center.x, block_center.y, selected_block) != 0) {
            break;
        }
    }
    return selected_block;
}

void process_physics(Lum& lum) {
     vec3 tank_body_center = tank_body_trans.rot * vec3(8.5,12.5, 0) + tank_body_trans.shift;
    ivec2 tank_body_center_in_blocks = ivec2(tank_body_center) / 16;
    int block_under_body = int(tank_body_center.z) / 16;

    if(any(lessThan(tank_body_center_in_blocks, ivec2(0)))) return;
    if(any(greaterThanEqual(tank_body_center_in_blocks, ivec2(lum.render.world_size)))) return;

    int selected_block = 0;
    
    if(lum.getWorldBlock(tank_body_center_in_blocks.x, tank_body_center_in_blocks.y, block_under_body+1) != 0){
        selected_block = block_under_body+1;
    } else if(lum.getWorldBlock(tank_body_center_in_blocks.x, tank_body_center_in_blocks.y, block_under_body) != 0){
        selected_block = block_under_body;
    }else if(lum.getWorldBlock(tank_body_center_in_blocks.x, tank_body_center_in_blocks.y, block_under_body-1) != 0){
        selected_block = block_under_body-1;
    } else {
        for(selected_block=lum.render.world_size.z-1; selected_block>=0; selected_block--){
            BlockID_t blockId = lum.getWorldBlock(tank_body_center_in_blocks.x, tank_body_center_in_blocks.y, selected_block);

            if (blockId != 0) break;
        }
    }

    physical_body_height = float(selected_block)*16.0 + 16.0;
}

quat find_quat(vec3 v1, vec3 v2){
        quat q;
        vec3 a = cross(v1, v2);
        q = a;
        q.w = sqrt((length(v1) * length(v1)) * (length(v2)*length(v2))) + dot(v1, v2);

        return normalize(q);
}

// some inverse kinematics
void process_animations(Lum& lum){
    vec3 tank_direction_forward = tank_body_trans.rot * vec3(0,1,0);
    vec3 tank_direction_right   = tank_body_trans.rot * vec3(1,0,0);

    Particle p = {};
        p.lifeTime = 10.0 * (float(rand()) / float(RAND_MAX));
        p.pos = tank_head_trans.rot*vec3(17,42,27) + tank_head_trans.shift;
        p.vel = (rnVec3(0,1)*2.f - 1.f)*1.1f;
        p.matID = 79;
    lum.render.particles.push_back(p);

    float interpolation = glm::clamp(float(lum.delt_time)*4.20f);
    tank_head_trans.rot = normalize(glm::mix(tank_head_trans.rot, quat(vec3(0,0,glm::pi<float>()))*tank_body_trans.rot, interpolation));
    interpolated_body_height = glm::mix(interpolated_body_height, physical_body_height, interpolation);
    tank_body_trans.shift.z = interpolated_body_height;
        
    vec3 body_head_joint_shift = tank_body_trans.rot * vec3(8.5,12.5, tank_body->size.z);
    vec3 head_joint_shift = tank_head_trans.rot * vec3(33/2.0,63/2.0, 0);
    vec3 head_joint = tank_body_trans.shift + body_head_joint_shift;

    tank_head_trans.shift = head_joint - head_joint_shift;
    
    vec3 body_rf_leg_joint = tank_body_trans.shift + tank_body_trans.rot * vec3(14.0, 19.5, 6.5);
    vec3 body_lb_leg_joint = tank_body_trans.shift + tank_body_trans.rot * vec3( 3.0,  3.5, 6.5);
    vec3 body_lf_leg_joint = tank_body_trans.shift + tank_body_trans.rot * vec3( 3.0, 19.5, 6.5);
    vec3 body_rb_leg_joint = tank_body_trans.shift + tank_body_trans.rot * vec3(14.0,  3.5, 6.5);
    
    tank_rf_leg_trans.rot = quat(-vec3(0,0,glm::pi<float>())) * tank_body_trans.rot;
    tank_lb_leg_trans.rot = tank_body_trans.rot;
    tank_lf_leg_trans.rot = quat(-vec3(0,0,glm::pi<float>())) * tank_body_trans.rot;
    tank_rb_leg_trans.rot = tank_body_trans.rot;

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

    tank_rf_leg_trans.rot *= rf_quat;
    tank_lb_leg_trans.rot *= lb_quat;
    tank_lf_leg_trans.rot *= lf_quat;
    tank_rb_leg_trans.rot *= rb_quat;

    vec3 rf_leg_joint_shift = tank_rf_leg_trans.rot * vec3(10.0, 6.5, 12.5);
    vec3 lb_leg_joint_shift = tank_lb_leg_trans.rot * vec3(10.0, 6.5, 12.5);
    vec3 lf_leg_joint_shift = tank_lf_leg_trans.rot * vec3( 0.0, 6.5, 12.5);
    vec3 rb_leg_joint_shift = tank_rb_leg_trans.rot * vec3( 0.0, 6.5, 12.5);

    tank_rf_leg_trans.shift = body_rf_leg_joint - rf_leg_joint_shift;
    tank_lb_leg_trans.shift = body_lb_leg_joint - lb_leg_joint_shift;
    tank_lf_leg_trans.shift = body_lf_leg_joint - lf_leg_joint_shift;
    tank_rb_leg_trans.shift = body_rb_leg_joint - rb_leg_joint_shift; 
}
