#version 450

//depth writing for lightmaps visibility

precision highp int;
precision highp float;

#extension GL_EXT_shader_8bit_storage : enable
#extension GL_EXT_shader_16bit_storage : enable
#extension GL_EXT_shader_explicit_arithmetic_types : enable
#extension GL_EXT_scalar_block_layout : enable

layout(location = 0) in lowp uvec3 posIn;

layout(binding = 0, set = 0) uniform restrict readonly UniformBufferObject {
    mat4 trans_w2s;
} ubo;

layout(push_constant) uniform restrict readonly constants{
    i16vec4 shift;
} pco;

vec3 qtransform( vec4 q, vec3 v ){ 
	return v + 2.0*cross(cross(v, -q.xyz ) + q.w*v, -q.xyz);
} 

void main() {
    vec3 fpos = vec3(posIn);

    vec3 local_pos = fpos;

    vec4 world_pos = vec4(local_pos + vec3(pco.shift.xyz) ,1);

    vec3 clip_coords = (ubo.trans_w2s*world_pos).xyz; //move up
         clip_coords.z = 1+clip_coords.z;

    gl_Position  = vec4(clip_coords, 1);    
}