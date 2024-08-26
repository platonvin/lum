#version 450 core

precision highp float;

/*
it is basically cascade heightmap sampling
rendered via instanced triangle strips that represent long thin rectangles
can probably be optimized by few times
*/

layout(push_constant) uniform readonly restrict constants{
    vec4 shift;
    int size;
    int time; //that thing that no one can define 
} pco;
layout(binding = 0, set = 0) uniform readonly restrict UniformBufferObject {
    mat4 trans_w2s;
} ubo;
layout(set = 0, binding = 1) uniform sampler2D state;

layout(location = 0) out vec3 orig;

const int BLOCK_PALETTE_SIZE_X = 64;
const int STATIC_BLOCK_COUNT = 15; // 0 + 1..static block count so >=STATIC_BLOCK_COUNT
const float PI = 3.1415926535;
const ivec3 world_size = ivec3(48,48,16);
const int lods = 6;

ivec3 voxel_in_palette(ivec3 relative_voxel_pos, int block_id) {
    int block_x = block_id % BLOCK_PALETTE_SIZE_X;
    int block_y = block_id / BLOCK_PALETTE_SIZE_X;

    return relative_voxel_pos + ivec3(16*block_x, 16*block_y, 0);
}

const int BLADES_PER_INSTANCE = 1;
const int VERTICES_PER_BLADE = 11; //3 for padding
const int MAX_HEIGHT = 5;

float rand(vec2 co){
    return fract(sin(dot(co, vec2(12.9898, 78.233))) * 43758.5453);
}

// waves! sum of sin()'s
float get_height(vec2 globalpos, float time){
    vec2 direction = vec2(sin(pco.time / 100.0), cos(pco.time / 100.0));
    direction = vec2(1,1);

    float total_height = 0;
    // yeah ~32 iteration, but this will be offloaded to compute shader+texture soon
    // for(float freq=3.0; freq < 100.0; freq *= 1.15){
    //     float ampl = 3.0 / freq;
    //     direction = vec2(sin(pco.time/300.0 + freq), cos(pco.time/300.0 + freq));        
    //     total_height += sin((time + dot(pos,direction)/10.0)*freq) * ampl;
    // }

    total_height += texture(state, globalpos/13.0).x * (13.0)/55.0;
    total_height += texture(state, globalpos/31.0).y * (31.0)/55.0;
    total_height += texture(state, globalpos/35.0).z * (35.0)/55.0;
    total_height += texture(state, globalpos/42.0).w * (42.0)/55.0;

    return total_height / 1.0;
}
#define make_offset(s, off)\
    float s  = textureOffset(state, globalpos/13.0, off).x * (13.0)/55.0;\
          s += textureOffset(state, globalpos/31.0, off).y * (31.0)/55.0;\
          s += textureOffset(state, globalpos/35.0, off).z * (35.0)/55.0;\
          s += textureOffset(state, globalpos/42.0, off).w * (42.0)/55.0;\

vec3 get_normal(vec2 globalpos, float time){
    vec2 direction = vec2(sin(pco.time / 100.0), cos(pco.time / 100.0));
    direction = vec2(1,1);

    vec2 total_height_d = vec2(0);
    // yeah ~32 iteration, but this will be offloaded to compute shader+texture soon
    // for(float freq=3.0; freq < 100.0; freq *= 1.15){
    //     float ampl = 3.0 / freq;
    //     direction = vec2(sin(pco.time/300.0 + freq), cos(pco.time/300.0 + freq));
    //     total_height_d += ((direction/10.0)*freq)* cos((time + dot(pos,direction)/10.0)*freq) * ampl;
    // }

    const vec2 size = vec2(2.0,0.0);
    const ivec3 off = ivec3(-1,0,1);

    vec4 wave = texture(state, globalpos/13.0);
    // float s11 = wave.x1
    make_offset(s01, off.xy);
    make_offset(s21, off.zy);
    make_offset(s10, off.yx);
    make_offset(s12, off.yz);

    vec3 va = normalize(vec3(size.xy,s21-s01));
    vec3 vb = normalize(vec3(size.yx,s12-s10));
    vec3 norm = vec3( cross(va,vb));

    return norm;
}

void wave_water_vert(in vec2 pos, vec2 shift, out float height, out vec3 normal){
    float t = pco.time / 300.0;
    height = get_height(pos+shift, t);

    //unused
    normal = get_normal(pos+shift, t);
    
    return;
}

vec3 get_water_vert(int vert_index, int instance_index, vec2 shift, out vec3 normal){
    vec3 vertex;

    //instance is "tape"
    int instance_y_shift = instance_index;
    int y_shift = (vert_index % 2);
    int x_shift = ((vert_index+1) / 2);
    vertex.x = (float(x_shift                   ) / float(pco.size))*16.0;//*4;
    vertex.y = (float(y_shift + instance_y_shift) / float(pco.size))*16.0;//*4;
    
    wave_water_vert(vertex.xy, shift, vertex.z, normal);

    return vertex;
}

void main() {
    int vert_id = gl_VertexIndex;    
    int instance_id = gl_InstanceIndex;    
    
    vec3 normal;
    vec3 rel2world = get_water_vert(vert_id, instance_id, pco.shift.xy, normal);

    vec4 world_pos = vec4(rel2world,1) + pco.shift;
    vec3 clip_coords = (ubo.trans_w2s*world_pos).xyz;
         clip_coords.z = 1.0+clip_coords.z;

    gl_Position  = vec4(clip_coords, 1);    

    vec3 norm = normal;
    // norm = normalize(vec3(1));
    uint mat = uint(30);
    float fmat = (float(mat)-127.0)/127.0;
    vec4 fmat_norm = vec4(fmat, norm);
    // mat_norm = uvec4(((fmat_norm+1.0)/2.0)*255.0);

    orig = world_pos.xyz;
}