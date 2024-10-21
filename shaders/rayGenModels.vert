#version 450

#define varp highp
precision varp int;
precision varp float;

#include "common/ext.glsl"
#include "common/ubo.glsl"

layout(location = 0) in lowp uvec3 posIn;
layout(location = 0) out vec3 sample_point;
layout(location = 1) out flat uint normal_encoded_packed;
// layout(location = 2) out vec3 n;

layout(scalar, push_constant) uniform restrict readonly constants{
    vec4 rot;
    vec4 shift;
    vec4 fnormal; //not encoded
} pco;

vec3 qtransform( vec4 q, vec3 v ){ 
	return v + 2.0*cross(cross(v, -q.xyz ) + q.w*v, -q.xyz);
    // return (q*vec4(v,0)).xyz;
} 

void main() {
    vec3 fpos = vec3(posIn);
    vec3 fnorm_ms = normalize(vec3(pco.fnormal.xyz));

    vec3 local_pos = qtransform(pco.rot, fpos);
    // vec3 local_pos = fpos;

    vec4 world_pos = vec4(local_pos + pco.shift.xyz ,1);

    vec3 clip_coords = (ubo.trans_w2s*world_pos).xyz; //move up
         clip_coords.z = 1+clip_coords.z;

    gl_Position  = vec4(clip_coords, 1);    
    
    vec3 fnorm_ws = (qtransform(pco.rot,fnorm_ms)); //move up
    // normal_encoded_packed = 
    //     (norm_encoded.x<<0) |
    //     (norm_encoded.y<<8) |
    //     (norm_encoded.z<<16) ;
    normal_encoded_packed = packUnorm4x8(vec4((fnorm_ws+1.0)/2.0, 0));
    sample_point = fpos - fnorm_ms * 0.5; //for better rounding lol
}