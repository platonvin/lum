#version 450 core

precision highp float;

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

const int QUADS_IN_BLADE = 4;
const int VTX_IN_BLADE = QUADS_IN_BLADE*6 + 3;

// vec3 blade_vertices [VTX_IN_BLADE] = {
//     vec3(0,0,0+0), vec3(1,0,0+0), vec3(1,0,1+0),
//     vec3(0,0,0+0), vec3(0,0,1+0), vec3(1,0,1+0),
//     vec3(0,0,0+1), vec3(1,0,0+1), vec3(1,0,1+1),
//     vec3(0,0,0+1), vec3(0,0,1+1), vec3(1,0,1+1),
// };

// vec3 blade_norms[VTX_IN_BLADE] = {
//     vec3(0,0,0), vec3(1,0,0), vec3(0,1,0),
//     vec3(0,0,0), vec3(0,0,1), vec3(0,1,0),
//     vec3(0,0,0), vec3(1,0,0), vec3(0,1,0),
//     vec3(0,0,0), vec3(0,0,1), vec3(0,1,0),
// };

float rand(vec2 co){
    return fract(sin(dot(co, vec2(12.9898, 78.233))) * 43758.5453);
}

vec3 bezier(vec3 p0, vec3 p1, vec3 p2, float t){
    vec3 a = mix(p0, p1, t);
    vec3 b = mix(p1, p2, t);
    return mix(a, b, t);
}

vec3 getQuadVertex(int index) {
    float x = float(index % 2);
    float z = float(index / 2);

    return vec3(x, 0.0, z);
}

float get_blade_width(float height){
    float max_height = float(QUADS_IN_BLADE);

    // return (max_height+height) / max_height;
    // return 1.0;
    return (max_height-height) / max_height;
}

vec3 get_blade_vert(int iindex, out vec3 normal){
    // vec3 vertex = blade_vertices[index];

    // float index = float(iindex);
    int quad_low_corner_height = iindex / 6;
    int internal_index = iindex % 6;
    int internal_quad_index;
    if(internal_index <= 2){
        internal_quad_index = internal_index;
    } else {
        internal_quad_index = 6-internal_index;
    }
    vec3 vertex = getQuadVertex(internal_quad_index);
    
    if((iindex / 3)  == (QUADS_IN_BLADE*2 + 1)){
        if((iindex % 3) == 0) vertex = vec3(0,0,0);
        if((iindex % 3) == 1) vertex = vec3(1,0,0);
        if((iindex % 3) == 2) vertex = vec3(0.5,0,1);
    }
    
    vertex.z += float(quad_low_corner_height);
    
    // normal = blade_norms[index];
    normal = vec3(0,1,0);

    //narrowing to the end
    float width = get_blade_width(vertex.z);
    float width_diff = 1.0 - width;
    vertex.x = width * vertex.x + width_diff / 2.0;

    //curving
    vec3 p0 = vec3(0,0,0);
    vec3 p1 = vec3(0,0,QUADS_IN_BLADE);
    vec3 p2 = vec3(0,0.7,QUADS_IN_BLADE);

    vec3 b = bezier(p0, p1, p2, vertex.z);
    vertex.y = b.y;

    vertex.x *= 3.7;
    
    vertex.z *= 4.0; // increase height
    
    return vertex;
}

//ONLY WORKS WITH BLADE NON TRANSFORMED VERTS
vec3 rotate_blade_vert(vec3 vertex, float rnd01){
    float angle = rnd01 * PI * 31.15;
    float cos_rot = cos(angle);
    float sin_rot = sin(angle);
    
    float xNew =  vertex.x * cos_rot + vertex.y * sin_rot;
    float Ynew = -vertex.x * sin_rot + vertex.y * cos_rot;

    return vec3(xNew, Ynew, vertex.z);
}

// vec3 curve_blade_vert(vec3 vertex, float rnd01){
//     float angle = rnd01 * PI * 31.15;
//     float cos_rot = cos(angle);
//     float sin_rot = sin(angle);
    
//     float xNew =  vertex.x * cos_rot + vertex.y * sin_rot;
//     float Ynew = -vertex.x * sin_rot + vertex.y * cos_rot;

//     return vec3(xNew, Ynew, vertex.z);
// }

void main() {
    int global_vertex_id = gl_VertexIndex;
    int blade_id = global_vertex_id / VTX_IN_BLADE;    
    int blade_vertex_id = global_vertex_id % VTX_IN_BLADE;
    
    vec3 normal;
    vec3 rel2world = get_blade_vert(blade_vertex_id, normal);
    // rel2world = rel2world.zxy;
    
    int blade_x = blade_id % pco.size;
    int blade_y = blade_id / pco.size;
    vec2 rel2tile_shift = (vec2(blade_x, blade_y) / vec2(pco.size)) * 64.0; //for visibility
    // vec3 rel2tile = rel2world + vec3(rel2tile_shift,0);

    float rand_rot = rand(rel2tile_shift);
    rel2world = rotate_blade_vert(rel2world, rand_rot);

    vec3 rel2tile = rel2world + vec3(rel2tile_shift,0);

    vec4 world_pos = vec4(rel2tile,1) + pco.shift;
    // vec4 world_pos = vec4(rel2tile,1) + vec4(0,0,16,0);
    // world_pos.xy = -world_pos.xy;
    // world_pos.
    // world_pos.x = mod(world_pos.x, 16.0);
    // world_pos.y = mod(world_pos.y, 16.0);

    vec3 clip_coords = (ubo.trans_w2s*world_pos).xyz;
         clip_coords.z = -clip_coords.z;

    // // uv_shift = (clip_coords_old.xy - clip_coords.xy)/2.0; //0..1
    gl_Position  = vec4(clip_coords, 1);    

    norm = normal;
    // norm = normalize(vec3(1));
    mat = uint(9);
}