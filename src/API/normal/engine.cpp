#include "engine.hpp"
#include <random>
#include <glm/gtx/quaternion.hpp>

using glm::u8, glm::u16, glm::u16, glm::u32;
using glm::i8, glm::i16, glm::i32;
using glm::f32, glm::f64;
using glm::defaultp;
using glm::ivec2,glm::ivec3,glm::ivec4;
using glm::i8vec2,glm::i8vec3,glm::i8vec4;
using glm::i16vec2,glm::i16vec3,glm::i16vec4;
using glm::uvec2,glm::uvec3,glm::uvec4;
using glm::u8vec2,glm::u8vec3,glm::u8vec4;
using glm::u16vec2,glm::u16vec3,glm::u16vec4;
using glm::vec,glm::vec2,glm::vec3,glm::vec4;
using glm::dvec2,glm::dvec3,glm::dvec4;
using glm::mat, glm::mat2, glm::mat3, glm::mat4;
using glm::dmat2, glm::dmat3, glm::dmat4;
using glm::quat, glm::dquat;
using glm::quat_identity, glm::identity;
using glm::pi;

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

static bool sort_blocks(struct block_render_request const& lhs, struct block_render_request const& rhs){
    // if(lhs.index < rhs.index) {
    //     return true;
    // } else if(lhs.index == rhs.index){
    //     return lhs.cam_dist > rhs.cam_dist; 
    // } else return false;
    // return lhs.index < rhs.index;
    return lhs.cam_dist > rhs.cam_dist; //NEVER CHANGE
}
static bool sort_grass(struct grass_render_request const& lhs, struct grass_render_request const rhs){
    return ((lhs.cam_dist) > (rhs.cam_dist)); //NEVER CHANGE
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
InternalMeshModel tank_body = {};
InternalMeshModel tank_head = {};
InternalMeshModel tank_rf_leg = {};
InternalMeshModel tank_lf_leg = {};
InternalMeshModel tank_rb_leg = {};
InternalMeshModel tank_lb_leg = {};
vec2 physical_points[4] = {}; // that each leg follows
vec2 interpolated_leg_points[4] = {}; // that each leg follows
vec2 target_leg_points[4]; // that each leg follows
float physical_body_height;
float interpolated_body_height;
BlockWithMesh** block_palette = {};

static double curr_time = 0;
static double prev_time = 0;
static double delt_time = 0;
static double block_placement_delay = 0;

void Engine::setup_graphics(){
    Settings settings = {};
        settings.vsync = false; //every time deciding to which image to render, wait until monitor draws current. Icreases perfomance, but limits fps
        settings.fullscreen = false;
        settings.debug = false; //Validation Layers. Use them while developing or be tricked into thinking that your code is correct
        settings.timestampCount = 48;
        settings.profile = true; //monitors perfomance via timestamps. You can place one with PLACE_TIMESTAMP() macro
        settings.fif = 2; // Frames In Flight. If 1, then record cmdbuff and submit it. If multiple, cpu will (might) be ahead of gpu by FIF-1, which makes GPU wait less

        settings.deviceFeatures.vertexPipelineStoresAndAtomics = VK_TRUE;
        settings.deviceFeatures.fragmentStoresAndAtomics = VK_TRUE;
        settings.deviceFeatures.geometryShader = VK_TRUE;
        settings.deviceFeatures.independentBlend = VK_TRUE;
        settings.deviceFeatures.shaderInt16 = VK_TRUE;
        settings.deviceFeatures11.storagePushConstant16 = VK_TRUE;
        settings.deviceFeatures12.storagePushConstant8 = VK_TRUE;
        settings.deviceFeatures12.shaderInt8 = VK_TRUE;
        settings.deviceFeatures12.storageBuffer8BitAccess = VK_TRUE;
        // for assumeEXT, currently disabled and will be brought back with new shader system
        // VkPhysicalDeviceShaderExpectAssumeFeaturesKHR 
        //     expect;
        //     expect.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_EXPECT_ASSUME_FEATURES_KHR;
        //     expect.shaderExpectAssume = VK_TRUE;
        // settings.addExtraFeaturesPnext(&expect);
        
        settings.deviceExtensions.push_back(VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME); //simplifies renderer for negative cost lol
        settings.deviceExtensions.push_back(VK_KHR_SHADER_FLOAT16_INT8_EXTENSION_NAME); //pco / ubo +perfomance
        settings.deviceExtensions.push_back(VK_KHR_16BIT_STORAGE_EXTENSION_NAME); //just explicit control
        settings.deviceExtensions.push_back(VK_KHR_8BIT_STORAGE_EXTENSION_NAME); //just explicit control
        settings.deviceExtensions.push_back(VK_EXT_SCALAR_BLOCK_LAYOUT_EXTENSION_NAME); // aka zero-pain buffers
        // settings.deviceExtensions.push_back(VK_KHR_SHADER_EXPECT_ASSUME_EXTENSION_NAME); // GLSL's assumeEXT(statement)

    render.init(settings);
TRACE();
    vkDeviceWaitIdle(render.render.device);
TRACE();

    render.load_scene("assets/scene");
TRACE();
    // for(int xx=0; xx< render.world_size.x; xx++){
    // for(int yy=0; yy< render.world_size.y; yy++){
    //     render.origin_world(xx,yy,0) = 2;
    // }}
    // render.origin_world.set(0);
        render.load_mesh(&tank_body, "assets/tank_body.vox");
TRACE();
        tank_body.shift += vec3(13.1,14.1,3.1)*16.0f;
        render.load_mesh(&tank_head, "assets/tank_head.vox");

        render.load_mesh(&tank_rf_leg, "assets/tank_rf_lb_leg.vox");
        render.load_mesh(&tank_lb_leg, "assets/tank_rf_lb_leg.vox");

        render.load_mesh(&tank_lf_leg, "assets/tank_lf_rb_leg.vox");
        render.load_mesh(&tank_rb_leg, "assets/tank_lf_rb_leg.vox");
TRACE();

    block_palette = (BlockWithMesh**)calloc(render.static_block_palette_size, sizeof(BlockWithMesh*));
    
TRACE();
    //block_palette[0] is "air"
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
TRACE();

    render.updateBlockPalette(block_palette);
TRACE();
    render.updateMaterialPalette(render.mat_palette);

    vkDeviceWaitIdle(render.render.device);
TRACE();
}
void Engine::setup_ui(){
TRACE();
    ui.renderer = &render;
    ui.setup();
    vkDeviceWaitIdle(render.render.device);
TRACE();
}

void Engine::update_system(){
    glfwPollEvents();
    should_close |= glfwWindowShouldClose(render.render.window.pointer);
}

void Engine::setup_input() {
    input.setup(render.render.window.pointer);
    
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
    input.setActionCallback(GameAction::MOVE_CAMERA_FORWARD, [this](GameAction action) {
        render.camera.cameraPos += delt_time * dvec3(dvec2(render.camera.cameraDir), 0) * 400.5 / render.camera.pixelsInVoxel;
    });

    input.setActionCallback(GameAction::MOVE_CAMERA_BACKWARD, [this](GameAction action) {
        render.camera.cameraPos -= delt_time * dvec3(dvec2(render.camera.cameraDir), 0) * 400.5 / render.camera.pixelsInVoxel;
    });

    input.setActionCallback(GameAction::MOVE_CAMERA_LEFT, [this](GameAction action) {
        dvec3 camera_direction_to_right = dquat(dvec3(0.0, 0.0, pi<double>() / 2.0)) * render.camera.cameraDir;
        render.camera.cameraPos += delt_time * dvec3(dvec2(camera_direction_to_right), 0) * 400.5 / render.camera.pixelsInVoxel;
    });
    input.setActionCallback(GameAction::MOVE_CAMERA_RIGHT, [this](GameAction action) {
        dvec3 camera_direction_to_right = dquat(dvec3(0.0, 0.0, pi<double>() / 2.0)) * render.camera.cameraDir;
        render.camera.cameraPos -= delt_time * dvec3(dvec2(camera_direction_to_right), 0) * 400.5 / render.camera.pixelsInVoxel;
    });

    input.setActionCallback(GameAction::TURN_CAMERA_LEFT, [this](GameAction action) {
        render.camera.cameraDir = rotate(identity<dmat4>(), +0.60 * delt_time, dvec3(0, 0, 1)) * dvec4(render.camera.cameraDir, 0);
        render.camera.cameraDir = normalize(render.camera.cameraDir);
    });
    input.setActionCallback(GameAction::TURN_CAMERA_RIGHT, [this](GameAction action) {
        render.camera.cameraDir = rotate(identity<dmat4>(), -0.60 * delt_time, dvec3(0, 0, 1)) * dvec4(render.camera.cameraDir, 0);
        render.camera.cameraDir = normalize(render.camera.cameraDir);
    });

    input.setActionCallback(GameAction::MOVE_TANK_FORWARD, [this](GameAction action) {
        vec3 tank_direction_forward = tank_body.rot * vec3(0, 1, 0);
        float tank_speed = 50.0 * delt_time;
        tank_body.shift += tank_direction_forward * tank_speed;
    });
    input.setActionCallback(GameAction::MOVE_TANK_BACKWARD, [this](GameAction action) {
        vec3 tank_direction_forward = tank_body.rot * vec3(0, 1, 0);
        float tank_speed = 50.0 * delt_time;
        tank_body.shift -= tank_direction_forward * tank_speed;
    });

    input.setActionCallback(GameAction::TURN_TANK_LEFT, [this](GameAction action) {
        quat old_rot = tank_body.rot;
        tank_body.rot *= quat(vec3(0, 0, +0.05f * 50.0f * delt_time));
        quat new_rot = tank_body.rot;

        vec3 old_center = old_rot * vec3(tank_body.size) / 2.0f;
        vec3 new_center = new_rot * vec3(tank_body.size) / 2.0f;

        vec3 difference = new_center - old_center;
        tank_body.shift -= difference;

        tank_body.rot = normalize(tank_body.rot);
    });

    input.setActionCallback(GameAction::TURN_TANK_RIGHT, [this](GameAction action) {
        quat old_rot = tank_body.rot;
        tank_body.rot *= quat(vec3(0, 0, -0.05f * 50.0f * delt_time));
        quat new_rot = tank_body.rot;

        vec3 old_center = old_rot * vec3(tank_body.size) / 2.0f;
        vec3 new_center = new_rot * vec3(tank_body.size) / 2.0f;

        vec3 difference = new_center - old_center;
        tank_body.shift -= difference;

        tank_body.rot = normalize(tank_body.rot);
    });

    for (int i = 1; i < 10; ++i) {
        input.setActionCallback(static_cast<GameAction>(GameAction::SET_BLOCK_1 + (i - 1)), [&, i](GameAction action) {
            ivec3 block_to_set = ivec3(vec3(tank_body.shift) + tank_body.rot*(vec3(tank_body.size)/2.0f))/16;
                block_to_set = clamp(block_to_set, ivec3(0), ivec3(render.world_size)-ivec3(1));
            render.origin_world(block_to_set.x, block_to_set.y, block_to_set.z) = i;
        });
        input.rebindKey(static_cast<GameAction>(GameAction::SET_BLOCK_1 + (i - 1)), GLFW_KEY_0 + i);
    }

    // for 0 key (special case for air block)
    input.setActionCallback(GameAction::SET_BLOCK_0, [&](GameAction action) {
        ivec3 block_to_set = ivec3(vec3(tank_body.shift) + tank_body.rot*(vec3(tank_body.size)/2.0f))/16;
            block_to_set = clamp(block_to_set, ivec3(0), ivec3(render.world_size)-ivec3(1));
        render.origin_world(block_to_set.x, block_to_set.y, block_to_set.z - 1) = 0;
    });
    input.rebindKey(GameAction::SET_BLOCK_0, GLFW_KEY_0);

    // For F1-F5 keys (GLFW_KEY_F1 to GLFW_KEY_F5)
    for (int i = 0; i < 5; ++i) {
        input.setActionCallback(static_cast<GameAction>(GameAction::SET_BLOCK_F1 + i), [&, i](GameAction action) {
            ivec3 block_to_set = ivec3(vec3(tank_body.shift) + tank_body.rot*(vec3(tank_body.size)/2.0f))/16;
                block_to_set = clamp(block_to_set, ivec3(0), ivec3(render.world_size)-ivec3(1));
            render.origin_world(block_to_set.x, block_to_set.y, block_to_set.z) = 10 + i;
        });
        input.rebindKey(static_cast<GameAction>(GameAction::SET_BLOCK_F1 + i), GLFW_KEY_F1 + i);
    }


    input.setActionCallback(GameAction::SHOOT, [this](GameAction action) {
        Particle 
            p = {};
            p.lifeTime = 12.0;
            p.pos = tank_head.rot*vec3(16.5,3,9.5) + tank_head.shift;
            p.matID = 249;
            p.vel = (rnVec3(0,1)*2.f - 1.f)*.5f + tank_head.rot*vec3(0,-1,0)*100.f;
        render.particles.push_back(p);

        p.lifeTime = 2.5;
        p.matID = 19;
        for(int i=0; i< 30; i++){
            p.vel = (rnVec3(0,1)*2.f - 1.f)*25.f;
        render.particles.push_back(p);
        }
    });

    input.setActionCallback(GameAction::INCREASE_ZOOM, [this](GameAction action){
            render.camera.pixelsInVoxel *= 1.0 + delt_time;
    });
    input.setActionCallback(GameAction::DECREASE_ZOOM, [this](GameAction action){
            render.camera.pixelsInVoxel /= 1.0 + delt_time;
    });
}

void Engine::handle_input(){
    should_close |= (glfwGetKey(render.render.window.pointer, GLFW_KEY_ESCAPE) == GLFW_PRESS);
    input.pollUpdates();
    
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

// some inverse kinematics
void Engine::process_animations(){
    vec3 tank_direction_forward = tank_body.rot * vec3(0,1,0);
    vec3 tank_direction_right   = tank_body.rot * vec3(1,0,0);

    Particle p = {};
        p.lifeTime = 10.0 * (float(rand()) / float(RAND_MAX));
        p.pos = tank_head.rot*vec3(17,42,27) + tank_head.shift;
        p.vel = (rnVec3(0,1)*2.f - 1.f)*1.1f;
        p.matID = 79;
    render.particles.push_back(p);

    float interpolation = glm::clamp(float(delt_time)*4.20f);
    tank_head.rot = normalize(glm::mix(tank_head.rot, quat(vec3(0,0,pi<float>()))*tank_body.rot, interpolation));
    interpolated_body_height = glm::mix(interpolated_body_height, physical_body_height, interpolation);
    tank_body.shift.z = interpolated_body_height;
        
    vec3 body_head_joint_shift = tank_body.rot * vec3(8.5,12.5, tank_body.size.z);
    vec3 head_joint_shift = tank_head.rot * vec3(33/2.0,63/2.0, 0);
    vec3 head_joint = tank_body.shift + body_head_joint_shift;

    tank_head.shift = head_joint - head_joint_shift;
    
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

#include <glm/gtx/string_cast.hpp>
void Engine::cull_meshes(){
    block_que.clear();
    for(int xx=0; xx<48; xx++){
    for(int yy=0; yy<48; yy++){
    for(int zz=0; zz<16;  zz++){
        InternalMeshModel* block_mesh = NULL;
        
        int block_id = render.origin_world(xx,yy,zz);
        if(block_id != 0){
            struct block_render_request /*goes*/ brr = {}; //
            brr.pos = ivec3(xx*16,yy*16, zz*16);
            brr.index = block_id;

            vec3 clip_coords = (render.camera.cameraTransform * vec4(brr.pos,1));
                clip_coords.z = -clip_coords.z;

            brr.cam_dist = clip_coords.z;

            if(is_block_visible(render.camera.cameraTransform, dvec3(brr.pos))){
                block_que.push_back(brr);
            }
        }
        assert(block_id <= render.static_block_palette_size);
    }}}
    std::sort(block_que.begin(), block_que.end(), &sort_blocks);
    // for (auto b : block_que){
    //     cout << b.cam_dist << " " << b.index << "\n";
    // }

    grass_que.clear();
    for(int xx=4; xx<20; xx++){
    for(int yy=4; yy<20; yy++){
        // for(int xx=5; xx<12; xx++){
        // for(int yy=6; yy<16; yy++){
        if((xx>=5) and (xx<12) and (yy>=6) and (yy<16)) continue;
        struct grass_render_request grr = {};
        grr.pos = ivec3(xx*16, yy*16, 16);

        dvec3 clip_coords = (render.camera.cameraTransform * dvec4(grr.pos,1));
            clip_coords.z = -clip_coords.z;
        grr.cam_dist = clip_coords.z;

        // if(is_block_visible(render.camera.cameraTransform, dvec3(grr.pos))){
        grass_que.push_back(grr);
        // render.specialRadianceUpdates.push_back(i8vec4(ivec3(xx,yy,1),0));
    }}
    // nevery knew std::sort is allowed to segfault on wrong __comp
    std::sort(grass_que.begin(), grass_que.end(), &sort_grass); 

    // for(int xx=0; xx<10; xx++){
    // for(int yy=0; yy<4; yy++){
    //     render.origin_world(xx,yy,00)=2;
    // }}
    water_que.clear();
    for(int xx=5; xx<12; xx++){
    for(int yy=6; yy<16; yy++){
        // render.origin_world(xx,yy,00)=0;
        struct grass_render_request wrr = {};
        wrr.pos = ivec3(xx*16, yy*16, 14);

        dvec3 clip_coords = (render.camera.cameraTransform * dvec4(wrr.pos,1));
            clip_coords.z = -clip_coords.z;
        wrr.cam_dist = clip_coords.z;
 
        // if(is_block_visible(render.camera.cameraTransform, dvec3(grr.pos))){
        water_que.push_back(wrr);
        render.specialRadianceUpdates.push_back(i8vec4(ivec3(xx,yy,2),0));
        render.specialRadianceUpdates.push_back(i8vec4(ivec3(xx,yy,1),0));
    }}
    std::sort(water_que.begin(), water_que.end(), &sort_grass);
}

void Engine::draw()
{
TRACE();
    render.start_frame();
TRACE();

        // render.start_compute();
            render.start_blockify();
TRACE();
                render.blockify_mesh(&tank_body);
                render.blockify_mesh(&tank_head);
            
                render.blockify_mesh(&tank_rf_leg);
                render.blockify_mesh(&tank_lb_leg);
                render.blockify_mesh(&tank_lf_leg);
                render.blockify_mesh(&tank_rb_leg);
            render.end_blockify();
TRACE();
            render.update_radiance();
            // render.recalculate_df(); // currently unused. per-voxel Distance Field 
            // render.recalculate_bit(); // currently unused. Bitpacking, like (block==Air) ? 0 : 1 
TRACE();
            render.updade_grass({});
            render.updade_water();
TRACE();
            render.exec_copies();
TRACE();
                render.start_map();
TRACE();
                    render.map_mesh(&tank_body);
                    render.map_mesh(&tank_head);
                    render.map_mesh(&tank_rf_leg);
TRACE();
                    render.map_mesh(&tank_lb_leg);
                    render.map_mesh(&tank_lf_leg);
                    render.map_mesh(&tank_rb_leg);
TRACE();
                render.end_map();
TRACE();
            render.end_compute();
                // render.raytrace();
TRACE();
                render.start_lightmap();
TRACE();
                //yeah its wrong
                render.lightmap_start_blocks();
                    for(auto b : block_que){
                        Mesh* block_mesh = NULL;
                            block_mesh = &block_palette[b.index]->mesh;
                            block_mesh->shift = vec3(b.pos);
                        render.lightmap_block(block_mesh, b.index, b.pos);
                    }
TRACE();
                render.lightmap_start_models();
                    render.lightmap_model(&tank_body);
                    render.lightmap_model(&tank_head);
                    render.lightmap_model(&tank_rf_leg);
                    render.lightmap_model(&tank_lb_leg);
                    render.lightmap_model(&tank_lf_leg);
                    render.lightmap_model(&tank_rb_leg);
                render.end_lightmap();
TRACE();

                render.start_raygen();
TRACE();  
                // printl(block_que.size());
                render.raygen_start_blocks();
                    for(auto b : block_que){
                        Mesh* block_mesh = NULL;

                        block_mesh = &block_palette[b.index]->mesh;
                        block_mesh->shift = vec3(b.pos);
                        
                        render.raygen_block(block_mesh, b.index, b.pos);
                    }
                    
                render.raygen_start_models();
                    render.raygen_model(&tank_body);
                    render.raygen_model(&tank_head);
                     
                    render.raygen_model(&tank_rf_leg);
                    render.raygen_model(&tank_lb_leg);
TRACE();
                    render.raygen_model(&tank_lf_leg);
                    render.raygen_model(&tank_rb_leg);

TRACE();
                render.update_particles();
TRACE();
                render.raygen_map_particles();
TRACE();      
                render.raygen_start_grass();
                    // for(int xx=0; xx<16;xx++){
                    // for(int yy=0; yy<16;yy++){
                    //     if(render.origin_world(4+xx,4+yy,1) == 0){
                    //         render.raygen_map_grass(vec4(64+xx*16,64+yy*16,16,0), 16);
                    //     }
                    // }}
                    for(auto g : grass_que){
                        render.raygen_map_grass(vec4(g.pos,0), 10);
                    }
                    // render.raygen_map_grass(&grass, vec4(128+16*2,128+16*1,16,0), 16);
                    // render.raygen_map_grass(&grass, vec4(128+16*1,128+16*2,16,0), 16);
                    // render.raygen_map_grass(&grass, vec4(128+16*2,128+16*2,16,0), 16);
TRACE();

                render.raygen_start_water();
                    for(auto w : water_que){
                        render.raygen_map_water(vec4(w.pos,0), 32);
                    }
                render.end_raygen();
TRACE();
                render.start_2nd_spass();
TRACE();
                render.diffuse();
TRACE();
                render.ambient_occlusion(); 
TRACE();
                render.glossy_raygen();
TRACE();
                render.raygen_start_smoke();
TRACE();
                render.glossy();
TRACE();
                render.smoke();
TRACE();
                render.tonemap();
TRACE();
            render.start_ui(); 
TRACE();
                ui.update();
TRACE();
                ui.draw();
TRACE();
        render.end_ui(); 
TRACE();
        render.end_2nd_spass();
TRACE();
    render.end_frame();
}

void Engine::setup(){
    setup_graphics();
    setup_ui();
    setup_input();
}

void Engine::update(){
    prev_time = curr_time;
    curr_time = glfwGetTime();
    delt_time = curr_time-prev_time;
    render.deltaTime = delt_time;

TRACE();
    update_system();
    handle_input();
    process_physics();
    process_animations();
    cull_meshes();
    draw();
    printFPS();
}

void Engine::cleanup(){
    vkDeviceWaitIdle(render.render.device);

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

    vkDeviceWaitIdle(render.render.device);
    ui.cleanup();
    render.cleanup();
}