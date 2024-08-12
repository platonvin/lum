#version 450 core

precision highp float;

/*
triagnle strip version. Same blade drawn.
as we all (should) know, gpu's do work in workgroups (wavegronts)
and they are typically 32/64
thus if we have 11 vertices, we may be*(1) wasting a lot of perfomance
this can be fixed by simply packing multiple blades in same instance
but 32 is clearly not multiply of 11, and sadly 11x3=33 is one more than 32

*1) Measurements showed that i was clearly was wrong. But strip still faster

*/

layout(push_constant) uniform constants{
    vec4 shift;
    int size; //total size*size blades
    int time; //seed
} pco;

layout(binding = 0, set = 0) uniform UniformBufferObject {
    mat4 trans_w2s;
    // mat4 trans_w2s_old; //just leave it
} ubo;
layout(set = 0, binding = 1) uniform sampler2D state;

layout(location = 0) lowp flat out float fmat;
layout(location = 1) lowp      out vec3 norm;

const int BLOCK_PALETTE_SIZE_X = 64;
const int STATIC_BLOCK_COUNT = 15; // 0 + 1..static block count so >=STATIC_BLOCK_COUNT
const float PI = 3.1415926535;
const ivec3 world_size = ivec3(48,48,16);

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
        float angle = rnd01 * PI * 2.0 * 420.0; //increasing randomness
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
    vec2 relative_pos = ((vec2(blade_x, blade_y) + 0.5)/ vec2(pco.size));

    vec3 normal;
    float rand01 = rand(relative_pos);
    vec3 rel2world = get_blade_vert(blade_vertex_id, normal, rand01, relative_pos);

    vec2 rel2tile_shift = relative_pos * 16.0; //for visibility
    vec3 rel2tile = rel2world + vec3(rel2tile_shift,0);

    vec4 world_pos = vec4(rel2tile,1) + pco.shift;
    vec3 clip_coords = (ubo.trans_w2s*world_pos).xyz;
         clip_coords.z = -clip_coords.z;

    // // uv_shift = (clip_coords_old.xy - clip_coords.xy)/2.0; //0..1
    gl_Position  = vec4(clip_coords, 1);    

    norm = normal;
    // norm = normalize(vec3(1));
    uint mat = uint(9);
    fmat = (float(mat)-127.0)/127.0;
}