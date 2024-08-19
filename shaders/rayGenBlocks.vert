#version 450

#define varp highp
precision varp int;
precision varp float;

// #extension GL_EXT_shader_8bit_storage : enable
// #extension GL_EXT_shader_16bit_storage : enable
// #extension GL_EXT_shader_explicit_arithmetic_types : enable

layout(location = 0) in lowp uvec3 posIn;
layout(location = 0) out vec3 sample_point;

layout(binding = 0, set = 0) restrict readonly uniform UniformBufferObject {
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
layout(binding = 1, set = 0) uniform usampler3D blockPalette;

//quatornions!
layout(push_constant) restrict readonly uniform constants{
    // vec4 rot;
    vec4 shift;
    vec4 fnormal; //not encoded
    uvec4 unormal; //encoded
    // int block;
} pco;

vec3 qtransform( vec4 q, vec3 v ){ 
	return v + 2.0*cross(cross(v, -q.xyz ) + q.w*v, -q.xyz);
    // return (q*vec4(v,0)).xyz;
} 

void main() {
    vec3 fpos = vec3(posIn);
    vec3 fnorm = vec3(pco.fnormal.xyz);

    vec3 local_pos = fpos;

    vec4 world_pos = vec4(local_pos + pco.shift.xyz ,1);

    vec3 clip_coords = (ubo.trans_w2s*world_pos).xyz; //move up
         clip_coords.z = 1+clip_coords.z;

    gl_Position  = vec4(clip_coords, 1);    
    
    sample_point = local_pos - fnorm * 0.5; //for better rounding lol
}