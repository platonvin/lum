#version 450
layout(early_fragment_tests) in;

#define varp highp
precision varp int;
precision varp float;

#extension GL_GOOGLE_include_directive : require
#include "common\ext.glsl"
#include "common\ubo.glsl"
#include "common\consts.glsl"

layout(binding = 1, set = 0) uniform usampler3D blockPalette;

layout(location = 0) in vec3 sample_point;
layout(location = 1) in flat uint bunorm;

layout(location = 0) lowp out uvec4 outMatNorm;

const int BLOCK_PALETTE_SIZE_X = 64;
ivec3 voxel_in_palette(ivec3 relative_voxel_pos, int block_id) {
    int block_x = block_id % BLOCK_PALETTE_SIZE_X;
    int block_y = block_id / BLOCK_PALETTE_SIZE_X;

    return relative_voxel_pos + ivec3(16*block_x, 16*block_y, 0);
}

ivec3 voxel_in_bit_palette(ivec3 relative_voxel_pos, int block_id) {
    int block_x = block_id % BLOCK_PALETTE_SIZE_X;
    int block_y = block_id / BLOCK_PALETTE_SIZE_X;

    return relative_voxel_pos + ivec3(0+2*block_x, 0+16*block_y,0);
}

int GetVoxel(in int block_id, ivec3 relative_voxel_pos){    
    int voxel;
    ivec3 voxel_pos = voxel_in_palette(relative_voxel_pos, block_id);
    voxel = int(texelFetch(blockPalette, (voxel_pos), 0).r);

    return (voxel);
}

void main() {
    uint sample_block = unpackUint2x16(bunorm.x).x;
    uint normal_encoded = unpackUint2x16(bunorm.x).y;

    uvec3 axis = uvec3(
        normal_encoded        & 01,
        (normal_encoded >> 1) & 01,
        (normal_encoded >> 2) & 01
    );
    int _sign = 1 - 2 * int((normal_encoded >> 7) & 01);
    ivec3 inorm = ivec3(axis) * _sign;
    outMatNorm.yzw = uvec3((ivec3(inorm + 1) * 255) / 2);
    // outMatNorm.x = 9;

    ivec3 ipos = ivec3(sample_point);

    outMatNorm.x = GetVoxel(int(sample_block), ipos).x;
}