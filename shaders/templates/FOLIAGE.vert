#version 450 core

precision highp float;
precision highp int;
#extension GL_GOOGLE_include_directive : require
#include "..\common\ext.glsl"
#include "..\common\ubo.glsl"

layout(set = 0, binding = 1) uniform sampler2D state;

layout(push_constant) uniform restrict readonly PushConstants {
    vec4 shift;
    int size;
    int time;
    int x_flip;
    int y_flip;
} pco;

layout(location = 0) lowp flat out uvec4 mat_norm;

// TODO: spec constants 
const int BLOCK_PALETTE_SIZE_X = 64;
const int STATIC_BLOCK_COUNT = 15; // 0 + 1..static block count so >=STATIC_BLOCK_COUNT
const ivec3 WORLD_SIZE = ivec3(48,48,16);

// local_pos is in [0,1]
// returns sampled WIND global worldspace offset
// you then can multiple it by normalized height to get wind wiggles
vec2 load_offset(vec2 local_pos){
    vec2 offset;
    vec2 world_pos = local_pos*16.0 + pco.shift.xy;
    vec2 state_pos = world_pos / vec2(WORLD_SIZE.xy*16);
    offset = texture(state, state_pos).xy;
    return offset;
}

void main() {
    // do smth like this for faster depth testing
    // it is literally placing them in different order to place closest to camera first
    if(pco.x_flip == 0) my_blade_x = pco.size - blade_x;
    if(pco.y_flip != 0) my_blade_y = pco.size - blade_y;
    
    vec3 normal;

    vec4 world_pos;
    world_pos = YOUR COMPUTATIONS;

    // world-space to screen-space
    vec3 clip_coords = (ubo.trans_w2s*world_pos).xyz;
         clip_coords.z = 1.0+clip_coords.z;

    // common
    gl_Position  = vec4(clip_coords, 1);    

    // Fix for shitty normals like mine
    if(dot(ubo.camdir.xyz,normal) > 0) normal = -normal;

    //little coloring. They are hardcoded palette indices
    uint mat = (rand01 > (rand(pco.shift.yx) - length(relative_pos - 8.0)/32.0))? uint(9) : uint(10);
    vec3 norm = normal;

    // part where attributes are setten
    float fmat = (float(mat)-127.0)/127.0;
    vec4 fmat_norm = vec4(fmat, norm);
    mat_norm = uvec4(((fmat_norm+1.0)/2.0)*255.0);
}