#version 450
layout(early_fragment_tests) in;

#define varp highp
precision varp int;
precision varp float;

#extension GL_EXT_shader_8bit_storage : enable
#extension GL_EXT_shader_16bit_storage : enable
#extension GL_EXT_shader_explicit_arithmetic_types : enable
#extension GL_EXT_scalar_block_layout : enable

layout(binding = 0, set = 0) uniform restrict readonly UniformBufferObject {
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

layout(binding = 0, set = 1) uniform usampler3D modelVoxels;

layout(location = 0) in vec3 sample_point;
layout(location = 1) in flat uint normal_encoded_packed;
// layout(location = 2) in vec3 n;

layout(location = 0) lowp out uvec4 outMatNorm;

int GetModelVoxel(ivec3 relative_voxel_pos){    
    int voxel;
    voxel = int(texelFetch(modelVoxels, (relative_voxel_pos), 0).r);
    return (voxel);
}

void main() {
    uvec3 normal_encoded = uvec4(
        ((normal_encoded_packed>>0 ) & (255)),
        ((normal_encoded_packed>>8 ) & (255)),
        ((normal_encoded_packed>>16) & (255)),
        0
    ).xyz;


    outMatNorm.yzw = uvec3(normal_encoded);
    // outMatNorm.yzw = uvec3(((n+1.0)/2.0)*255.0);
    
    ivec3 ipos = ivec3(sample_point);

    outMatNorm.x = GetModelVoxel(ipos).x;
    // outMatNorm.x = 9;
}