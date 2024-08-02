#version 450 core

precision highp float;

/*
triagnle strip version. Same blade drawn.
as we all (should) know, gpu's do work in workgroups (wavegronts)
and they are typically 32/64
thus if we have 11 vertices, we waste a lot of perfomance
this can be fixed by simply packing multiple blades in same instance
but 32 is clearly not multiply of 11, and sadly 11x3=33 is one more than 32

89.6 idle to
*/

layout(push_constant) uniform constants{
    vec4 shift;
    int size; //total size*size blades
    int time; //seed
} pco;

layout(binding = 0, set = 0) uniform UniformBufferObject {
    mat4 trans_w2s;
    mat4 trans_w2s_old;
} ubo;
layout(set = 0, binding = 1, r16i)  uniform iimage3D blocks;
layout(set = 0, binding = 2, r8ui) uniform uimage3D blockPalette;

layout(location = 0) flat out vec3 norm;
layout(location = 1) flat out uint mat;

const int BLOCK_PALETTE_SIZE_X = 64;
const int STATIC_BLOCK_COUNT = 15; // 0 + 1..static block count so >=STATIC_BLOCK_COUNT
const float PI = 3.1415926535;

ivec3 voxel_in_palette(ivec3 relative_voxel_pos, int block_id) {
    int block_x = block_id % BLOCK_PALETTE_SIZE_X;
    int block_y = block_id / BLOCK_PALETTE_SIZE_X;

    return relative_voxel_pos + ivec3(16*block_x, 16*block_y, 0);
}

// const int BLADES_PER_INSTANCE = 2;
// const int VERTICES_PER_BLADE = 11+3; //3 for padding
const int BLADES_PER_INSTANCE = 1;
const int VERTICES_PER_BLADE = 11; //3 for padding
const int MAX_HEIGHT = 5;

float rand(vec2 co){
    return fract(sin(dot(co, vec2(12.9898, 78.233))) * 43758.5453);
}

vec3 bezier(vec3 p0, vec3 p1, vec3 p2, float t){
    vec3 a = mix(p0, p1, t);
    vec3 b = mix(p1, p2, t);
    return   mix(a,  b,  t);
}
float bezier(float p0, float p1, float p2, float t){
    float a = mix(p0, p1, t);
    float b = mix(p1, p2, t);
    return   mix(a,  b,  t);
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
        float angle = rnd01 * PI * 2.0; //increasing randomness
        float cos_rot = cos(angle);
        float sin_rot = sin(angle);
    // }
    
    // float cos_rot = rnd01; //
    // float sin_rot = sqrt(1 - cos_rot*cos_rot);

    float vxNew =  vertex.x * cos_rot + vertex.y * sin_rot;
    float vyNew = -vertex.x * sin_rot + vertex.y * cos_rot;

    vertex.x = vxNew;
    vertex.y = vyNew;
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
    vertex.y = (vertex.z / MAX_HEIGHT) * 1.5;

    return;
}
void wiggle_blade_vert(float rnd01, inout vec3 vertex, inout vec3 normal, in vec2 pos){
    //own ~random wiggling
    // vertex.y += 0.1*sin(float(pco.time)/200.0 + (rnd01*2.0*PI));
    // vertex.x += 0.1*sin(float(pco.time)/200.0 + (rnd01*2.0*PI));

    vec2 direction = vec2(sin(pco.time / 600.0), cos(pco.time / 600.0));
    
    //global wave wiggling from wind
    vec2 offset = vec2(0);
    //yeah ~32 iteration, but this will be offloaded to compute shader+texture soon
    for(float freq=3.0; freq < 100.0; freq *= 1.15){
        float ampl = 0.3 / freq;
        float t = pco.time / 300.0;
        offset += sin((t + dot(pos,direction)/400.0)*freq) * ampl;
    }

    vertex.xy += offset * vertex.z;


    return;
}

/*
blade structure - triangle strip, by triangle index
 10
8  9
6  7
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
    if(iindex == 10) x_pos = 0.5; 

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


    curve_blade_vert(rnd01, vertex, normal);

    rotate_blade_vert(rnd01, vertex, normal);

    wiggle_blade_vert(rnd01, vertex, normal, pos);    

    displace_blade(rnd01, vertex, normal);

    vertex.x *= 3.7; // increase width
    vertex.z *= 3.0; // increase height

    return vertex;
}

void main() {
    // int global_vertex_id = gl_VertexIndex;
    // int blade_id = global_vertex_id / VTX_IN_BLADE;    
    // int blade_vertex_id = global_vertex_id % VTX_IN_BLADE;

    // int sub_blade_vertex
    int sub_blade_id = gl_VertexIndex / VERTICES_PER_BLADE;
    
    int blade_id = gl_InstanceIndex*BLADES_PER_INSTANCE + sub_blade_id;    
    int blade_vertex_id = gl_VertexIndex % VERTICES_PER_BLADE;

    // if(blade_vertex_id == VERTICES_PER_BLADE-1) rel2world = vec3(1.0/0.0);
    // if(blade_vertex_id == VERTICES_PER_BLADE-2) rel2world = vec3(1.0/0.0);
    // if(blade_vertex_id == VERTICES_PER_BLADE-3) rel2world = vec3(1.0/0.0);
    
    int blade_x = blade_id % pco.size;
    int blade_y = blade_id / pco.size;
    vec2 relative_pos = (vec2(blade_x, blade_y) / vec2(pco.size));
    vec2 rel2tile_shift = relative_pos * 128.0; //for visibility

    vec3 normal;
    float rand01 = rand(rel2tile_shift);
    vec3 rel2world = get_blade_vert(blade_vertex_id, normal, rand01, rel2tile_shift);

    vec3 rel2tile = rel2world + vec3(rel2tile_shift,0);

    vec4 world_pos = vec4(rel2tile,1) + pco.shift;
    vec3 clip_coords = (ubo.trans_w2s*world_pos).xyz;
         clip_coords.z = -clip_coords.z;

    // // uv_shift = (clip_coords_old.xy - clip_coords.xy)/2.0; //0..1
    gl_Position  = vec4(clip_coords, 1);    

    norm = normal;
    // norm = normalize(vec3(1));
    mat = uint(9);
}