#version 450 core

precision highp float;

/*
triagnle strip version. Same blade drawn.
as we all (should) know, gpu's do work in workgroups (wavegronts)
and they are typically 32/64
thus if we have 11 vertices, we may be*(1) wasting a lot of perfomance
this can be fixed by simply packing multiple blades in same instance
but 32 is clearly not multiply of 11, and sadly 11x3=33 is one more than 32

*1) Measurements showed that i clearly was wrong. But strip still faster
*/


layout(binding = 0, set = 0) uniform restrict readonly UniformBufferObject {
    mat4 trans_w2s;
    vec4 campos;
    vec4 camdir;
    vec4 horizline_scaled;
    vec4 vertiline_scaled;
    vec4 globalLightDir;
    mat4 lightmap_proj;
    vec2 frame_size;
    int timeseed;
} ubo;
layout(set = 0, binding = 1) uniform sampler2D state;

layout(push_constant) uniform restrict readonly PushConstants{
    vec4 shift;
    int size; //total size*size blades
    int time; //seed
    int x_flip;
    int y_flip;
} pco;

layout(location = 0) lowp flat out uvec4 mat_norm;

const int BLOCK_PALETTE_SIZE_X = 64;
const int STATIC_BLOCK_COUNT = 15; // 0 + 1..static block count so >=STATIC_BLOCK_COUNT
const float PI = 3.1415926535;
const ivec3 world_size = ivec3(48,48,16);
const int size = 16; //total size*size blades

ivec3 voxel_in_palette(ivec3 relative_voxel_pos, int block_id) {
    int block_x = block_id % BLOCK_PALETTE_SIZE_X;
    int block_y = block_id / BLOCK_PALETTE_SIZE_X;

    return relative_voxel_pos + ivec3(16*block_x, 16*block_y, 0);
}

// const int BLADES_PER_INSTANCE = 2;
// const int VERTICES_PER_BLADE = 11+3; //3 for padding
const int BLADES_PER_INSTANCE = 1;
const int VERTICES_PER_BLADE = 6;
const int MAX_HEIGHT = 3;

uint hash21(uvec2 p){
    p *= uvec2(73333,7777);
    p ^= (uvec2(3333777777)>>(p>>28));
    uint n = p.x*p.y;
    return n^(n>>15);
}

float rand(vec2 p){
    uint h = hash21(floatBitsToUint(p));

    return float(h)*(1.0/float(0xffffffffU));
}

float get_blade_width(float height){
    float max_height = float(MAX_HEIGHT-1);

    // return (max_height+height) / max_height;
    // return 1.0;
    return (max_height-height) / max_height;
}

//ONLY WORKS WITH BLADE NON TRANSFORMED VERTS
void rotate_blade_vert(float rnd01, inout vec3 vertex, inout vec3 normal){
    // {//obvious version. Is left to explaing 
        float angle = rnd01 * PI * 2.0 * 42.0; //increasing randomness
        float cos_rot = cos(angle);
        float sin_rot = sin(angle);
    // }

    // vertex.xy -= 0.5;
    
    // float cos_rot = rnd01*0.0; //
    // float sin_rot = sqrt(1 - cos_rot*cos_rot);

    float vxNew =  vertex.x * cos_rot + vertex.y * sin_rot;
    float vyNew = -vertex.x * sin_rot + vertex.y * cos_rot;

    vertex.x = vxNew;
    vertex.y = vyNew;
    // vertex.xy += 0.5;
    // normal.x = 
    float nxNew =  normal.x * cos_rot + normal.y * sin_rot;
    float nyNew = -normal.x * sin_rot + normal.y * cos_rot;
    normal.x = nxNew;
    normal.y = nyNew;

    return;
}
void displace_blade(float rnd01, inout vec3 vertex, inout vec3 normal){
    vec2 shift = vec2(
        sin(rnd01*42.1424)*1.0,
        sin(rnd01*58.1424)*1.0);
        
    vertex.xy += shift;

    return;
}
void scale_blade_vert(float rnd01, inout vec3 vertex, inout vec3 normal){
    float scale = 0.5 + (rnd01)*0.5;
        
    vertex*= scale;

    return;
}
void curve_blade_vert(float rnd01, inout vec3 vertex, inout vec3 normal){
    vertex.y = (vertex.z / MAX_HEIGHT) * 1.5;// * sign(rnd01 - 0.5);

    return;
}

//local_pos is in [0,1]
//returns sampled global offset
vec2 load_offset(vec2 local_pos){
    vec2 offset;

    vec2 world_pos = local_pos*16.0 + pco.shift.xy;
    vec2 state_pos = world_pos / vec2(world_size.xy*16);

    offset = texture(state, state_pos).xy;

    return offset;
}

void wiggle_blade_vert(float rnd01, inout vec3 vertex, inout vec3 normal, in vec2 pos){
    //own ~random wiggling
    // vertex.y += 0.1*sin(float(pco.time)/200.0 + (rnd01*2.0*PI));
    // vertex.x += 0.1*sin(float(pco.time)/200.0 + (rnd01*2.0*PI));

    vec2 direction = vec2(sin(pco.time / 600.0), cos(pco.time / 600.0));
    
    //global wave wiggling from wind
    vec2 global_offset = load_offset(pos);

    //per-blade unique shift
    vec2 local_offset = vec2(0);
    for(float freq=1.0; freq < 4.0; freq += 1.2){
        float ampl = 0.05;
        float t = pco.time / 200.0;
        local_offset += sin(t*freq + rnd01*400.0*freq) * ampl;
    }

    vec2 offset = local_offset + global_offset;
    vertex.xy += offset * vertex.z * 1.0;

    //makes it rotate, not STRETCH
    float old_size = (vertex.z*vertex.z);
    float new_size = (vertex.z*vertex.z) + dot(vertex.xy, vertex.xy);
    // float len
    float descale = sqrt(new_size / old_size);
    vertex /= descale;

    return;
}

/*
blade structure - triangle strip, by triangle index
 6
4  5
2  3
0  1
*/
vec3 get_blade_vert(int iindex, out vec3 normal, in float rnd01, in vec2 pos){
    vec3 vertex;

    //iindex is in [0..10]
    float z_height = float(iindex / 2);
    float x_pos = float(iindex % 2);
    //top vertex
    if(iindex == (VERTICES_PER_BLADE-1)) x_pos = 0.5; 

    vertex = vec3(x_pos,0,z_height);

    //narrowing to the end
    //width is [0..1], so it is uscaled
    float width = get_blade_width(vertex.z);
    float width_diff = 1.0 - width;
    vertex.x = width * vertex.x + width_diff / 2.0;

    
    //making it not flat with little bit of magic 
    vec3 n1 = vec3(-0.5,1,0);
    vec3 n2 = vec3(+0.5,1,0);
    normal = normalize(mix(n1,n2,x_pos));

    vertex.y *= 3.7; // increase width
    vertex.x *= 3.7; // increase width
    vertex.z *= 5./3.;

    curve_blade_vert(rnd01, vertex, normal);

    rotate_blade_vert(rnd01, vertex, normal);

    wiggle_blade_vert(rnd01, vertex, normal, pos);    

    displace_blade(rnd01, vertex, normal);

    // increase height
    vertex.z *= 1.5 + (rnd01*1.5)*(rnd01*1.5);//less uniform distribution

    return vertex;
}
void main() {
    int sub_blade_id = gl_VertexIndex / VERTICES_PER_BLADE;
    int blade_id = gl_InstanceIndex*BLADES_PER_INSTANCE + sub_blade_id;    
    int blade_vertex_id = gl_VertexIndex % VERTICES_PER_BLADE;
    int blade_x = blade_id % size;
    int blade_y = blade_id / size;
    //for faster depth testing
    if(pco.x_flip == 0) blade_x = size - blade_x;
    if(pco.y_flip != 0) blade_y = size - blade_y;
    
    vec2 relative_pos = ((vec2(blade_x, blade_y) + 0.5)/ vec2(size));

    vec3 normal;
    float rand01 = rand(relative_pos + pco.shift.xy);
    vec3 rel2world = get_blade_vert(blade_vertex_id, normal, rand01, relative_pos);

    vec2 rel2tile_shift = relative_pos * 16.0;
    vec3 rel2tile = rel2world + vec3(rel2tile_shift,0);

    vec4 world_pos = vec4(rel2tile,1) + pco.shift;
    vec3 clip_coords = (ubo.trans_w2s*world_pos).xyz;
         clip_coords.z = 1.0+clip_coords.z;

    // // uv_shift = (clip_coords_old.xy - clip_coords.xy)/2.0; //0..1
    gl_Position  = vec4(clip_coords, 1);    

    //TODO probably can just flip x/y
    if(dot(ubo.camdir.xyz,normal) > 0) normal = -normal;

    vec3 norm = normal;
    // norm = normalize(vec3(1));

    //little coloring
    uint mat = (rand01 > (rand(pco.shift.yx) - length(relative_pos - 8.0)/32.0))? uint(9) : uint(10);
    // uint mat = (rand01 > (rand(pco.shift.yx)))? uint(9) : uint(10);
    float fmat = (float(mat)-127.0)/127.0;
    vec4 fmat_norm = vec4(fmat, norm);
    mat_norm = uvec4(((fmat_norm+1.0)/2.0)*255.0);
}