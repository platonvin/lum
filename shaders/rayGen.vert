#version 450

#define varp highp

precision varp int;
precision varp float;

#extension GL_EXT_shader_8bit_storage : enable
#extension GL_EXT_shader_16bit_storage : enable

layout(location = 0) in varp uvec3 posIn;
// layout(location = 1) in varp ivec3 normIn;
layout(location = 1) in varp uint MatIDIn;

layout(location = 0) lowp flat out uvec4 mat_norm;

//no reason to move up in pipeline cause sm load is like ~ 6% in vs
layout(binding = 0, set = 0) uniform UniformBufferObject {
    mat4 trans_w2s;
    // mat4 trans_w2s_old;
} ubo;

//quatornions!
layout(push_constant) uniform constants{
    vec4 rot;
    vec4 shift;
    vec4 normal;
} pco;

precision highp float;

vec3 qtransform( vec4 q, vec3 v ){ 
	return v + 2.0*cross(cross(v, -q.xyz ) + q.w*v, -q.xyz);
    // return (q*vec4(v,0)).xyz;
} 

void main() {
    vec3 fpos = vec3(posIn);
    vec3 fnorm = vec3(pco.normal.xyz);

    vec4 world_pos = vec4(qtransform(pco.rot, fpos) + pco.shift.xyz ,1);

    vec3 clip_coords = (ubo.trans_w2s*world_pos).xyz; //move up
         clip_coords.z = 1+clip_coords.z;

    gl_Position  = vec4(clip_coords, 1);    
    
    vec3 norm = (qtransform(pco.rot,fnorm)); //move up
    uint mat = uint(MatIDIn);
    float fmat = (float(mat)-127.0)/127.0;
    vec4 fmat_norm = vec4(fmat, norm);
    mat_norm = uvec4(((fmat_norm+1.0)/2.0)*255.0);
}