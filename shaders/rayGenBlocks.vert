#version 450

#define varp highp
precision varp int;
precision varp float;

#extension GL_GOOGLE_include_directive : require
#include "common\ext.glsl"
#include "common\ubo.glsl"

layout(location = 0) in lowp uvec3 posIn;
layout(location = 0) out vec3 sample_point;
layout(location = 1) out flat uint bunorm;

layout(binding = 1, set = 0) uniform usampler3D blockPalette;

layout(scalar, push_constant) restrict readonly uniform constants{
    int16_t block;
    i16vec3 shift;
    u8vec4 unorm;
} pco;

vec3 qtransform( vec4 q, vec3 v ){ 
	return v + 2.0*cross(cross(v, -q.xyz ) + q.w*v, -q.xyz);
} 

void main() {
    ivec3 upos = ivec3(posIn);
    uint normal_encoded = pco.unorm.x;
    uint s = (normal_encoded & (1<<7))>>7; //0 if position 1 if negative
    uvec3 axis = uvec3(
        ((normal_encoded & (1<<0))>>0),
        ((normal_encoded & (1<<1))>>1),
        ((normal_encoded & (1<<2))>>2)
    );
    ivec3 inorm = ivec3(axis) * (1 - int(s)*2);
    vec3 fnorm = (vec3(inorm));

    ivec3 uworld_pos = ivec3(upos + (pco.shift));
     vec4 fworld_pos = vec4(uworld_pos, 1);

    vec3 clip_coords = (ubo.trans_w2s*fworld_pos).xyz; //move up
         clip_coords.z = 1+clip_coords.z;

    gl_Position  = vec4(clip_coords, 1);    
    
    sample_point = vec3(upos) - fnorm * 0.5; //for better rounding lol

    uint sample_block = uint(pco.block);
    // uvec3 normal_encoded = uvec3(((ivec3(pco.inorm) + 1)*255)/2);

    bunorm = packUint2x16(u16vec2(sample_block, pco.unorm.x));
}