#pragma once
#include <renderer/render.hpp>
#include <renderer/ui.hpp>
#include <defines/macros.hpp>
#include "input/input.hpp"

struct block_render_request{
    ivec3 pos;
    int index;
    float cam_dist;
};

struct grass_render_request{
    ivec3 pos;
    float cam_dist;
};
// struct water_render_request{
//     ivec3 pos;
//     float cam_dist;
// };
 
class Engine {
public:
    LumRenderer render = LumRenderer();
    Ui ui = {};

    //cpu culled sorted array of blocks to raygen as example. You egnine responsible for doing this, bot renderer
    vector<block_render_request> block_que = {};
    vector<grass_render_request> grass_que = {};
    vector<grass_render_request> water_que = {};
    bool should_close = false;

    Input input;
    
    void setup_graphics();
    void setup_ui();
    void setup_input();

    // void update();
    void update_system();
    void handle_input();
    void update_visual(); //renderer and ui intersect for perfomance reasons
    void process_physics();
    void process_animations();
    void cull_meshes();
    void draw();

    void setup();
    void update();
    void cleanup();

    void update_tank_joints(const InternalMeshModel& body, InternalMeshModel& head);

    void update_tank_leg_ik(
        const InternalMeshModel& body, InternalMeshModel& leg, const InternalMeshModel& body_leg_joint, 
        const vec3& tank_direction, vec2& target_leg_point, 
        vec2& physical_point, vec2& interpolated_leg_point
    );
};


vec3 rndVec3(float minValue, float maxValue);
quat find_quat(vec3 v1, vec3 v2);
bool is_block_visible(dmat4 trans, dvec3 pos);