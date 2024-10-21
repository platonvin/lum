#version 450
layout(early_fragment_tests) in;

#define varp highp
precision varp int;
precision varp float;

#include "common/ext.glsl"
#include "common/ubo.glsl"

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