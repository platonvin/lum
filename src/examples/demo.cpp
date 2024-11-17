#include "renderer.hpp"
#include "../input/input.hpp"
#include "../engine/engine.hpp"
#include "renderer/api/opaque_renderer_members.hpp"
#include <random>
// #include "defines/macros.hpp"

using glm::vec2 , glm::vec3;
using glm::dvec3, glm::dvec4;
using glm::dmat4, glm::ivec3, glm::quat;
using glm::defaultp;

vec3 rnVec3(float minValue, float maxValue);
void printFPS();
void setup_input(Input& input, Lum::Renderer& render);
void process_animations(Lum::Renderer& lum);
void process_physics(Lum::Renderer& lum);
void cleanup(Lum::Renderer& lum);
quat find_quat(vec3 v1, vec3 v2);

// but for simplicity lets stick to this
Lum::MeshModel     tank_body = {};
Lum::MeshTransform tank_body_trans = {};
Lum::MeshModel     tank_head = {};
Lum::MeshTransform tank_head_trans = {};
float physical_body_height;
float interpolated_body_height;
vec3 tank_direction_forward, tank_direction_right;

// defining ECS components
typedef struct {vec2 pos;} physical_leg_point;
typedef struct {vec2 pos;} interpolated_leg_point;
typedef struct {vec2 pos;} target_leg_point;
typedef struct {vec3 pos;} leg_center;
typedef struct {vec3 pos;} leg_joint_shift;
typedef struct {vec3 pos;} leg_point;
typedef struct {quat rot;} rotation_mul;

//defining ECS functions

// function called on entity when its created. Aka constructor
void init (Lum::MeshModel&, Lum::MeshTransform&, leg_point&, leg_joint_shift&, target_leg_point&, physical_leg_point&, interpolated_leg_point&, leg_center&, rotation_mul&) {}
// function called on entity when its destroyed. Aka destructor
void destroy (Lum::MeshModel&, Lum::MeshTransform&, leg_point&, leg_joint_shift&, target_leg_point&, physical_leg_point&, interpolated_leg_point&, leg_center&, rotation_mul&) {}

void calculateLegJointsAndRotations(leg_point& leg, leg_center& center, Lum::MeshTransform& trans, rotation_mul& mul) {
    leg.pos = tank_body_trans.shift + tank_body_trans.rot * center.pos;
    trans.rot = mul.rot * tank_body_trans.rot;
}
void calculateAndUpdateLegPoints(leg_point& leg, target_leg_point& target, physical_leg_point& physical, leg_center& center) {
    vec3 joint_pos = leg.pos;
    target.pos = vec2(joint_pos) + vec2(tank_direction_right) + vec2(tank_direction_forward) * 3.0f * (float(rand()) / float(RAND_MAX));
    
    if (distance(physical.pos, target.pos) > 6.0f) {
        physical.pos = target.pos;
    }
}
void interpolateAndCalculateLegRotation(interpolated_leg_point& interpolated, physical_leg_point& physical, Lum::MeshTransform& leg_trans, leg_point& leg) {
    interpolated.pos = mix(interpolated.pos, physical.pos, 0.6f);
    
    vec2 leg_direction = normalize(interpolated.pos - vec2(leg.pos));
    leg_trans.rot *= find_quat(tank_direction_right, vec3(leg_direction, 0));
}
void applyShiftAndDrawLeg(Lum::MeshTransform& leg_trans, leg_point& leg, leg_joint_shift& shift, Lum::MeshModel& leg_mesh) {
    vec3 leg_joint_shift = leg_trans.rot * shift.pos;
    leg_trans.shift = leg.pos - leg_joint_shift;
}
Lum::Renderer* lum_ptr;
void drawLeg(Lum::MeshTransform& leg_trans, Lum::MeshModel& leg_mesh) {
    lum_ptr->drawModel(leg_mesh, leg_trans);
}
int main(){
    // input system i designed. You can use, but it is not neccessary for Lum::Renderer
    Input input;
    // ecs system i designed. You can use, but it is not neccessary for Lum::Renderer
    auto anim_system = Lum::makeECSystem<
        Lum::MeshModel,
        Lum::MeshTransform,
        leg_point,
        leg_joint_shift,
        target_leg_point,
        physical_leg_point,
        interpolated_leg_point,
        leg_center,
        rotation_mul
    > (init, destroy, 
       calculateLegJointsAndRotations,
       calculateAndUpdateLegPoints,
       interpolateAndCalculateLegRotation,
       applyShiftAndDrawLeg);

    // creating legs
    Lum::EntityID rf_leg = anim_system.createEntity();
    Lum::EntityID lf_leg = anim_system.createEntity();
    Lum::EntityID rb_leg = anim_system.createEntity();
    Lum::EntityID lb_leg = anim_system.createEntity();
    // setting up constants
    anim_system.getEntityComponent<leg_center&>(rf_leg) = {vec3(14.0, 19.5, 6.5)};
    anim_system.getEntityComponent<leg_center&>(lb_leg) = {vec3( 3.0,  3.5, 6.5)};
    anim_system.getEntityComponent<leg_center&>(lf_leg) = {vec3( 3.0, 19.5, 6.5)};
    anim_system.getEntityComponent<leg_center&>(rb_leg) = {vec3(14.0,  3.5, 6.5)};
    anim_system.getEntityComponent<leg_joint_shift&>(rf_leg) = {vec3(10.0, 6.5, 12.5)};
    anim_system.getEntityComponent<leg_joint_shift&>(lb_leg) = {vec3(10.0, 6.5, 12.5)};
    anim_system.getEntityComponent<leg_joint_shift&>(lf_leg) = {vec3( 0.0, 6.5, 12.5)};
    anim_system.getEntityComponent<leg_joint_shift&>(rb_leg) = {vec3( 0.0, 6.5, 12.5)};
    anim_system.getEntityComponent<rotation_mul&>(rf_leg) = {quat(-vec3(0,0,glm::pi<float>()))};
    anim_system.getEntityComponent<rotation_mul&>(lb_leg) = {glm::quat_identity<float, defaultp>()};
    anim_system.getEntityComponent<rotation_mul&>(lf_leg) = {quat(-vec3(0,0,glm::pi<float>()))};
    anim_system.getEntityComponent<rotation_mul&>(rb_leg) = {glm::quat_identity<float, defaultp>()};

    Lum::Settings settings = {};
        settings.fullscreen = false;
        settings.vsync = false;
        settings.world_size = ivec3(48, 48, 16);
        settings.static_block_palette_size = 15;
        settings.maxParticleCount = 8128;
    Lum::Renderer lum(15, 4096, 64, 64, 64);
    lum_ptr = &lum;

    // ATTENTION: all foliage has to be declared BEFORE init()
    // this restriction just makes everything 100x simpler
    // i might implement dynamic Vulkan resources in future, but it will only hurt perfomance until ~20k foliage meshes
    // and you are supposed to compile shader to SPIRV yourself (for GLSL, use glslang / shaderc)
    Lum::MeshFoliage grass = lum.loadFoliage("shaders/compiled/grass.vert.spv", 6, 10);

    lum.init(settings);
    lum.loadWorld("assets/scene");

    // preparation stage. These functions also can be called in runtime
    // If no palette is found, first models defines it
        // this DOES set palette
    tank_body = lum.loadModel("assets/tank_body.vox", /*extract palette if no found = */ true);
    tank_body_trans.shift += vec3(13.1,14.1,3.1)*16.0f;
        // this DOES NOT set palette cause already setten.
    tank_head = lum.loadModel("assets/tank_head.vox", /*extract palette if no found = */ true);
        // material palette can also be loaded from alone
        // lum.loadPalette("my_magicavox_filescene_with_palette_i_want_to_extract.vox")
        // good way to handle this would be to have voxel for each material placed in scene to view them
    anim_system.getEntityComponent<Lum::MeshModel&>(rf_leg) = lum.loadModel("assets/tank_rf_lb_leg.vox");
    anim_system.getEntityComponent<Lum::MeshModel&>(lf_leg) = lum.loadModel("assets/tank_lf_rb_leg.vox");
    anim_system.getEntityComponent<Lum::MeshModel&>(rb_leg) = lum.loadModel("assets/tank_lf_rb_leg.vox");
    anim_system.getEntityComponent<Lum::MeshModel&>(lb_leg) = lum.loadModel("assets/tank_rf_lb_leg.vox");
    
    Lum::MeshLiquid  water = lum.loadLiquid(69, 42);
    Lum::MeshVolumetric smoke = lum.loadVolumetric(1, .5, {});

    lum.loadBlock(  1, "assets/dirt.vox");
    lum.loadBlock(  2, "assets/grass.vox");
    lum.loadBlock(  3, "assets/grassNdirt.vox");
    lum.loadBlock(  4, "assets/stone_dirt.vox");
    lum.loadBlock(  5, "assets/bush.vox");
    lum.loadBlock(  6, "assets/leaves.vox");
    lum.loadBlock(  7, "assets/iron.vox");
    lum.loadBlock(  8, "assets/lamp.vox");
    lum.loadBlock(  9, "assets/stone_brick.vox");
    lum.loadBlock( 10, "assets/stone_brick_cracked.vox");
    lum.loadBlock( 11, "assets/stone_pack.vox");
    lum.loadBlock( 12, "assets/bark.vox");
    lum.loadBlock( 13, "assets/wood.vox");
    lum.loadBlock( 14, "assets/planks.vox");
    // total 15 max blocks specified, so loadBlock(15) is illegal

    // literally uploads data to gpu. You can call them in runtime, but atm its not recommended (a lot of overhead curently, can be significantly reduced)
    lum.uploadBlockPaletteToGPU();
    lum.uploadMaterialPaletteToGPU();

    lum.waitIdle();

    // callback functions for glfw action input system 
    setup_input(input, lum);

    while(not lum.should_close){
        input.pollUpdates();
        glfwPollEvents();
        lum.should_close |= glfwWindowShouldClose((GLFWwindow*)lum.getGLFWptr());
        lum.should_close |= (glfwGetKey((GLFWwindow*)lum.getGLFWptr(), GLFW_KEY_ESCAPE) == GLFW_PRESS);

        process_physics(lum);
        process_animations(lum);
        anim_system.update(); // also animations
        
        lum.startFrame();
        // this *could* be implicit
        lum.drawWorld();
        lum.drawParticles();
        
        lum.drawModel(tank_body, tank_body_trans);
        lum.drawModel(tank_head, tank_head_trans);
        // lum.drawModel(tank_rf_leg, tank_rf_leg_trans);
        // lum.drawModel(tank_lf_leg, tank_lf_leg_trans);
        // lum.drawModel(tank_rb_leg, tank_rb_leg_trans);
        // lum.drawModel(tank_lb_leg, tank_lb_leg_trans);
        anim_system.updateSpecific(drawLeg);
        

        // literally procedural grass placement every frame. You probably want to store it as entities in your own structures
        for(int xx=4; xx<20; xx++){
        for(int yy=4; yy<20; yy++){
            if((xx>=5) and (xx<12) and (yy>=6) and (yy<16)) continue;
            ivec3 pos = ivec3(xx*16, yy*16, 16);
            lum.drawFoliageBlock(grass, pos);
        }}

        // literally procedural water placement every frame. You probably want to store it as entities in your own structures
        for(int xx=5; xx<12; xx++){
        for(int yy=6; yy<16; yy++){
            ivec3 pos = ivec3(xx*16, yy*16, 14);
            lum.drawLiquidBlock(water, pos);
            lum.opaque_members->render.specialRadianceUpdates.push_back(i8vec4(ivec3(xx,yy,2),0));
            lum.opaque_members->render.specialRadianceUpdates.push_back(i8vec4(ivec3(xx,yy,1),0));
        }}

        // literally procedural smoke placement every frame. You probably want to store it as entities in your own structures
        for(int xx=8; xx<10; xx++){
        for(int yy=10; yy<13; yy++){
            ivec3 pos = ivec3(xx*16, yy*16, 20);
            lum.drawVolumetricBlock(smoke, pos);
            // render.render.specialRadianceUpdates.push_back(i8vec4(ivec3(xx,yy,2),0));
            // render.render.specialRadianceUpdates.push_back(i8vec4(ivec3(xx,yy,1),0));
        }}

        lum.prepareFrame();
        lum.submitFrame();
    }

    // prints GPU profile data
    // if (lum.lumal.render.settings.profile){
    //     printf("%-22s: %7.3f: %7.3f\n", lum.lumal.render.timestampNames[0], 0.0, 0.0);    
    //     for (int i=1; i<lum.lumal.render.currentTimestamp; i++){
    //         double time_point = double(lum.lumal.render.average_ftimestamps[i] - lum.lumal.render.average_ftimestamps[0]);
    //         double time_diff = double(lum.lumal.render.average_ftimestamps[i] - lum.lumal.render.average_ftimestamps[i-1]);
    //         printf("%3d %-22s: %7.3f: %7.3f\n", i, lum.lumal.render.timestampNames[i], time_point, time_diff);
    //     }
    // }
    cleanup(lum);
}

// binding keys to some functions
void setup_input(Input& input, Lum::Renderer& render) {
    input.setup((GLFWwindow*)render.getGLFWptr());
    
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
    input.setActionType(GameAction::SHOOT, Lum::ActionType::OneShot);
    input.setActionType(GameAction::SET_BLOCK_1, Lum::ActionType::OneShot);
    input.setActionType(GameAction::SET_BLOCK_2, Lum::ActionType::OneShot);
    input.setActionType(GameAction::SET_BLOCK_3, Lum::ActionType::OneShot);
    input.setActionType(GameAction::SET_BLOCK_4, Lum::ActionType::OneShot);
    input.setActionType(GameAction::SET_BLOCK_5, Lum::ActionType::OneShot);
    input.setActionType(GameAction::SET_BLOCK_6, Lum::ActionType::OneShot);
    input.setActionType(GameAction::SET_BLOCK_7, Lum::ActionType::OneShot);
    input.setActionType(GameAction::SET_BLOCK_8, Lum::ActionType::OneShot);
    input.setActionType(GameAction::SET_BLOCK_9, Lum::ActionType::OneShot);
    input.setActionType(GameAction::SET_BLOCK_0, Lum::ActionType::OneShot);
    input.setActionType(GameAction::SET_BLOCK_F1, Lum::ActionType::OneShot);
    input.setActionType(GameAction::SET_BLOCK_F2, Lum::ActionType::OneShot);
    input.setActionType(GameAction::SET_BLOCK_F3, Lum::ActionType::OneShot);
    input.setActionType(GameAction::SET_BLOCK_F4, Lum::ActionType::OneShot);
    input.setActionType(GameAction::SET_BLOCK_F5, Lum::ActionType::OneShot);

    // bind action callbacks
    input.setActionCallback(GameAction::MOVE_CAMERA_FORWARD, [&](GameAction action) {
        render.getCamera().cameraPos += render.delta_time * dvec3(dvec2(render.getCamera().cameraDir), 0) * 400.5 / render.getCamera().pixelsInVoxel;
    });

    input.setActionCallback(GameAction::MOVE_CAMERA_BACKWARD, [&](GameAction action) {
        render.getCamera().cameraPos -= render.delta_time * dvec3(dvec2(render.getCamera().cameraDir), 0) * 400.5 / render.getCamera().pixelsInVoxel;
    });

    input.setActionCallback(GameAction::MOVE_CAMERA_LEFT, [&](GameAction action) {
        dvec3 camera_direction_to_right = glm::dquat(dvec3(0.0, 0.0, glm::pi<double>() / 2.0)) * render.getCamera().cameraDir;
        render.getCamera().cameraPos += render.delta_time * dvec3(dvec2(camera_direction_to_right), 0) * 400.5 / render.getCamera().pixelsInVoxel;
    });
    input.setActionCallback(GameAction::MOVE_CAMERA_RIGHT, [&](GameAction action) {
        dvec3 camera_direction_to_right = glm::dquat(dvec3(0.0, 0.0, glm::pi<double>() / 2.0)) * render.getCamera().cameraDir;
        render.getCamera().cameraPos -= render.delta_time * dvec3(dvec2(camera_direction_to_right), 0) * 400.5 / render.getCamera().pixelsInVoxel;
    });

    input.setActionCallback(GameAction::TURN_CAMERA_LEFT, [&](GameAction action) {
        render.getCamera().cameraDir = rotate(glm::identity<dmat4>(), +0.60 * render.delta_time, dvec3(0, 0, 1)) * dvec4(render.getCamera().cameraDir, 0);
        render.getCamera().cameraDir = normalize(render.getCamera().cameraDir);
    });
    input.setActionCallback(GameAction::TURN_CAMERA_RIGHT, [&](GameAction action) {
        render.getCamera().cameraDir = rotate(glm::identity<dmat4>(), -0.60 * render.delta_time, dvec3(0, 0, 1)) * dvec4(render.getCamera().cameraDir, 0);
        render.getCamera().cameraDir = normalize(render.getCamera().cameraDir);
    });

    input.setActionCallback(GameAction::MOVE_TANK_FORWARD, [&](GameAction action) {
        vec3 tank_direction_forward = tank_body_trans.rot * vec3(0, 1, 0);
        float tank_speed = 50.0 * render.delta_time;
        tank_body_trans.shift += tank_direction_forward * tank_speed;
    });
    input.setActionCallback(GameAction::MOVE_TANK_BACKWARD, [&](GameAction action) {
        vec3 tank_direction_forward = tank_body_trans.rot * vec3(0, 1, 0);
        float tank_speed = 50.0 * render.delta_time;
        tank_body_trans.shift -= tank_direction_forward * tank_speed;
    });

    input.setActionCallback(GameAction::TURN_TANK_LEFT, [&](GameAction action) {
        // assert(tank_body);
        quat old_rot = tank_body_trans.rot;
        tank_body_trans.rot *= quat(vec3(0, 0, +0.05f * 50.0f * render.delta_time));
        quat new_rot = tank_body_trans.rot;

        vec3 old_center = old_rot * vec3(tank_body.getSize()) / 2.0f;
        vec3 new_center = new_rot * vec3(tank_body.getSize()) / 2.0f;

        vec3 difference = new_center - old_center;
        tank_body_trans.shift -= difference;

        tank_body_trans.rot = normalize(tank_body_trans.rot);
    });

    input.setActionCallback(GameAction::TURN_TANK_RIGHT, [&](GameAction action) {
        quat old_rot = tank_body_trans.rot;
        tank_body_trans.rot *= quat(vec3(0, 0, -0.05f * 50.0f * render.delta_time));
        quat new_rot = tank_body_trans.rot;

        vec3 old_center = old_rot * vec3(tank_body.getSize()) / 2.0f;
        vec3 new_center = new_rot * vec3(tank_body.getSize()) / 2.0f;

        vec3 difference = new_center - old_center;
        tank_body_trans.shift -= difference;

        tank_body_trans.rot = normalize(tank_body_trans.rot);
    });

    for (int i = 1; i < 10; ++i) {
        input.setActionCallback(static_cast<GameAction>(GameAction::SET_BLOCK_1 + (i - 1)), [&, i](GameAction action) {
            ivec3 block_to_set = ivec3(vec3(tank_body_trans.shift) + tank_body_trans.rot*(vec3(tank_body.getSize())/2.0f))/16;
                block_to_set = clamp(block_to_set, ivec3(0), ivec3(render.getWorldSize())-ivec3(1));
            render.setWorldBlock(block_to_set.x, block_to_set.y, block_to_set.z, i);
        });
        input.rebindKey(static_cast<GameAction>(GameAction::SET_BLOCK_1 + (i - 1)), GLFW_KEY_0 + i);
    }

    // for 0 key (special case for air block)
    input.setActionCallback(GameAction::SET_BLOCK_0, [&](GameAction action) {
        ivec3 block_to_set = ivec3(vec3(tank_body_trans.shift) + tank_body_trans.rot*(vec3(tank_body.getSize())/2.0f))/16;
            block_to_set = clamp(block_to_set, ivec3(0), ivec3(render.getWorldSize())-ivec3(1));
        render.setWorldBlock(block_to_set.x, block_to_set.y, block_to_set.z - 1, 0);
    });
    input.rebindKey(GameAction::SET_BLOCK_0, GLFW_KEY_0);

    // For F1-F5 keys (GLFW_KEY_F1 to GLFW_KEY_F5)
    for (int i = 0; i < 5; ++i) {
        input.setActionCallback(static_cast<GameAction>(GameAction::SET_BLOCK_F1 + i), [&, i](GameAction action) {
            ivec3 block_to_set = ivec3(vec3(tank_body_trans.shift) + tank_body_trans.rot*(vec3(tank_body.getSize())/2.0f))/16;
                block_to_set = clamp(block_to_set, ivec3(0), ivec3(render.getWorldSize())-ivec3(1));
            render.setWorldBlock(block_to_set.x, block_to_set.y, block_to_set.z, 10 + i);
        });
        input.rebindKey(static_cast<GameAction>(GameAction::SET_BLOCK_F1 + i), GLFW_KEY_F1 + i);
    }


    input.setActionCallback(GameAction::SHOOT, [&](GameAction action) {
        Lum::Particle 
            p = {};
            p.lifeTime = 12.0;
            p.pos = tank_head_trans.rot*vec3(16.5,3,9.5) + tank_head_trans.shift;
            p.matID = 249;
            p.vel = (rnVec3(0,1)*2.f - 1.f)*.5f + tank_head_trans.rot*vec3(0,-1,0)*100.f;
        render.addParticle(p);

        p.lifeTime = 2.5;
        p.matID = 19;
        for(int i=0; i< 30; i++){
            p.vel = (rnVec3(0,1)*2.f - 1.f)*25.f;
        render.addParticle(p);
        }
    });

    input.setActionCallback(GameAction::INCREASE_ZOOM, [&](GameAction action){
            render.getCamera().pixelsInVoxel *= 1.0 + render.delta_time;
    });
    input.setActionCallback(GameAction::DECREASE_ZOOM, [&](GameAction action){
            render.getCamera().pixelsInVoxel /= 1.0 + render.delta_time;
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

void cleanup(Lum::Renderer& lum){
    // save_scene functions are not really supposed to be used, i implemented them for demo
    // this is not api.
    lum.opaque_members->render.save_scene("assets/scene");

    // automatically frees all allocated blocks and meshes
    lum.cleanup();
}

int findBlockUnderTank(Lum::Renderer& lum, const ivec2& block_center, int block_under_body) {
    int selected_block = 0;
    for (int i = -1; i <= 1; ++i) {
        if (lum.getWorldBlock(block_center.x, block_center.y, block_under_body + i) != 0) {
            return block_under_body + i;
        }
    }
    for (selected_block = lum.getWorldSize().z - 1; selected_block >= 0; --selected_block) {
        if (lum.getWorldBlock(block_center.x, block_center.y, selected_block) != 0) {
            break;
        }
    }
    return selected_block;
}

void process_physics(Lum::Renderer& lum) {
     vec3 tank_body_center = tank_body_trans.rot * vec3(8.5,12.5, 0) + tank_body_trans.shift;
    ivec2 tank_body_center_in_blocks = ivec2(tank_body_center) / 16;
    int block_under_body = int(tank_body_center.z) / 16;

    if(any(lessThan(tank_body_center_in_blocks, ivec2(0)))) return;
    if(any(greaterThanEqual(tank_body_center_in_blocks, ivec2(lum.getWorldSize())))) return;

    int selected_block = 0;
    
    if(lum.getWorldBlock(tank_body_center_in_blocks.x, tank_body_center_in_blocks.y, block_under_body+1) != 0){
        selected_block = block_under_body+1;
    } else if(lum.getWorldBlock(tank_body_center_in_blocks.x, tank_body_center_in_blocks.y, block_under_body) != 0){
        selected_block = block_under_body;
    }else if(lum.getWorldBlock(tank_body_center_in_blocks.x, tank_body_center_in_blocks.y, block_under_body-1) != 0){
        selected_block = block_under_body-1;
    } else {
        for(selected_block=lum.getWorldSize().z-1; selected_block>=0; selected_block--){
            Lum::MeshBlock blockId = lum.getWorldBlock(tank_body_center_in_blocks.x, tank_body_center_in_blocks.y, selected_block);

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

// some inverse kinematics for body&head
void process_animations(Lum::Renderer& lum){
    tank_direction_forward = tank_body_trans.rot * vec3(0,1,0);
    tank_direction_right   = tank_body_trans.rot * vec3(1,0,0);

    Lum::Particle p = {};
        p.lifeTime = 10.0 * (float(rand()) / float(RAND_MAX));
        p.pos = tank_head_trans.rot*vec3(17,42,27) + tank_head_trans.shift;
        p.vel = (rnVec3(0,1)*2.f - 1.f)*1.1f;
        p.matID = 79;
    lum.addParticle(p);

    float interpolation = glm::clamp(float(lum.delta_time)*4.20f);
    tank_head_trans.rot = normalize(glm::mix(tank_head_trans.rot, quat(vec3(0,0,glm::pi<float>()))*tank_body_trans.rot, interpolation));
    interpolated_body_height = glm::mix(interpolated_body_height, physical_body_height, interpolation);
    tank_body_trans.shift.z = interpolated_body_height;
        
    vec3 body_head_joint_shift = tank_body_trans.rot * vec3(8.5,12.5, tank_body.getSize().z);
    vec3 head_joint_shift = tank_head_trans.rot * vec3(33/2.0,63/2.0, 0);
    vec3 head_joint = tank_body_trans.shift + body_head_joint_shift;

    tank_head_trans.shift = head_joint - head_joint_shift;
}