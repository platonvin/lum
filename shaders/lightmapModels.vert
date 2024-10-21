#version 450

//depth writing for lightmaps visibility

precision highp int;
precision highp float;

layout(location = 0) in lowp uvec3 posIn;

//no reason to move up in pipeline cause sm load is like ~ 6% in vs
layout(binding = 0, set = 0) uniform restrict readonly UniformBufferObject {
    mat4 trans_w2s;
} ubo;

//quatornions!
layout(push_constant) uniform restrict readonly constants{
    vec4 rot;
    vec4 shift;
} pco;

vec3 qtransform( vec4 q, vec3 v ){ 
	return v + 2.0*cross(cross(v, -q.xyz ) + q.w*v, -q.xyz);
    // return (q*vec4(v,0)).xyz;
} 

void main() {
    vec3 fpos = vec3(posIn);

    vec3 local_pos = qtransform(pco.rot, fpos);
    // vec3 local_pos = fpos;

    vec4 world_pos = vec4(local_pos + pco.shift.xyz ,1);

    vec3 clip_coords = (ubo.trans_w2s*world_pos).xyz; //move up
         clip_coords.z = 1+clip_coords.z;

    gl_Position  = vec4(clip_coords, 1);    
}