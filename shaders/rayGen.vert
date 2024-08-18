#version 450

#define varp highp

precision varp int;
precision varp float;

#extension GL_EXT_shader_8bit_storage : enable
#extension GL_EXT_shader_16bit_storage : enable
#extension GL_EXT_shader_explicit_arithmetic_types : enable

layout(location = 0) in lowp uvec3 posIn;
// layout(location = 1) in varp ivec3 normIn;
// layout(location = 1) in varp uint MatIDIn;

layout(location = 0) lowp flat out uvec3 norm;
layout(location = 1) out vec3 sample_point;
layout(location = 2) flat out uint sample_block;

//no reason to move up in pipeline cause sm load is like ~ 6% in vs
layout(binding = 0, set = 0) uniform restrict readonly UniformBufferObject {
    mat4 trans_w2s;
    vec4 campos;
    vec4 camdir;
    vec4 horizline_scaled;
    vec4 vertiline_scaled;
    vec4 globalLightDir;
    mat4 lightmap_proj;
    int timeseed;
} ubo;
layout(binding = 1, set = 0) uniform usampler3D blockPalette;

//quatornions!
layout(push_constant) uniform restrict readonly constants{
    vec4 rot;
    vec4 shift;
    vec4 normal;
    // uint block_id;
} pco;

precision highp float;

vec3 qtransform( vec4 q, vec3 v ){ 
	return v + 2.0*cross(cross(v, -q.xyz ) + q.w*v, -q.xyz);
    // return (q*vec4(v,0)).xyz;
} 

void main() {
    vec3 fpos = vec3(posIn);
    vec3 fnorm = vec3(pco.normal.xyz);

    vec3 local_pos = qtransform(pco.rot, fpos);

    vec4 world_pos = vec4(local_pos + pco.shift.xyz ,1);

    vec3 clip_coords = (ubo.trans_w2s*world_pos).xyz; //move up
         clip_coords.z = 1+clip_coords.z;

    gl_Position  = vec4(clip_coords, 1);    
    
    vec3 _fnorm = (qtransform(pco.rot,fnorm)); //move up
    // uint mat = uint(MatIDIn);
    // float fmat = (float(mat)-127.0)/127.0;
    // vec4 fmat_norm = vec4(fmat, norm);
    norm = uvec3(((_fnorm+1.0)/2.0)*255.0);
    
    sample_point = local_pos - _fnorm * 0.5; //for better rounding lol
    sample_block = uint(pco.normal.w);
}