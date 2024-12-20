#version 450 

// layout (location = 0) out vec2 outUV;
layout (location = 0) out float end_depth;

#extension GL_GOOGLE_include_directive : require
#include "common\ext.glsl"
#include "common\ubo.glsl"

layout(push_constant) uniform restrict readonly constants{
    vec4 originSize;
} pco;

//slow but does not matter in here
const vec3 vertices[] = {
    vec3(0,1,1),
    vec3(0,1,0),
    vec3(0,0,0),
    vec3(0,0,0),
    vec3(0,0,1),
    vec3(0,1,1),
    vec3(1,0,0),
    vec3(1,1,0),
    vec3(1,1,1),
    vec3(1,1,1),
    vec3(1,0,1),
    vec3(1,0,0),
    vec3(0,0,0),
    vec3(1,0,0),
    vec3(1,0,1),
    vec3(1,0,1),
    vec3(0,0,1),
    vec3(0,0,0),
    vec3(1,1,1),
    vec3(1,1,0),
    vec3(0,1,0),
    vec3(0,1,0),
    vec3(0,1,1),
    vec3(1,1,1),
    vec3(1,1,0),
    vec3(1,0,0),
    vec3(0,0,0),
    vec3(0,0,0),
    vec3(0,1,0),
    vec3(1,1,0),
    vec3(0,0,1),
    vec3(1,0,1),
    vec3(1,1,1),
    vec3(1,1,1),
    vec3(0,1,1),
    vec3(0,0,1),
};

// const uint elements [] = {
// 	3, 2, 6, 7, 4, 2, 0,
// 	3, 1, 6, 5, 4, 1, 0
// };

vec3 get_vert(int index){
    int tri = index / 3;
    int idx = index % 3;
    int face = tri / 2;
    int top = tri % 2;

    int dir = face % 3;
    int pos = face / 3;

    int nz = dir >> 1;
    int ny = dir & 1;
    int nx = 1 ^ (ny | nz);

    vec3 d = vec3(nx, ny, nz);
    float flip = 1 - 2 * pos;

    vec3 n = flip * d;
    vec3 u = -d.yzx;
    vec3 v = flip * d.zxy;

    float mirror =  0 + 2 * top;
    vec3 xyz = n + mirror*(1-2*(idx&1))*u + mirror*(1-2*(idx>>1))*v;

    return xyz;
}

void main() 
{
    // end_depth = 10;
    // uint index = elements[gl_VertexIndex];
    // vec3 vertex = get_vert(gl_VertexIndex);
    vec3 vertex = vertices[gl_VertexIndex];
    
    vertex*= pco.originSize.w*1;

    vec4 world_pos = vec4(vertex + pco.originSize.xyz,1);
    vec3 clip_coords = (ubo.trans_w2s*world_pos).xyz;
         clip_coords.z = 1.0+clip_coords.z;
    end_depth = clip_coords.z;
        //  clip_coords.z = 0;

    gl_Position = vec4(clip_coords, 1);
}