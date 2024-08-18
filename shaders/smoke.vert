#version 450 

// layout (location = 0) out vec3 origin; //non_clip_pos
// layout (location = 0) out vec2 clip; //non_clip_pos

// layout(binding = 0, set = 0) uniform restrict readonly UniformBufferObject {
//     mat4 trans_w2s;
//     vec4 campos;
//     vec4 camdir;
//     vec4 horizline_scaled;
//     vec4 vertiline_scaled;
//     vec4 globalLightDir;
//     mat4 lightmap_proj;
//     int timeseed;
// } ubo;


void main() 
{
    vec2 outUV = vec2((gl_VertexIndex << 1) & 2, gl_VertexIndex & 2);
    vec2 clip = outUV * 2.0f + -1.0f;

    gl_Position = vec4(clip, 0.0f, 1.0f);
}