#version 450
layout(early_fragment_tests) in;

#define varp highp

precision varp int;
precision varp float;

layout(binding = 0, set = 0) uniform restrict readonly UniformBufferObject {
    mat4 trans_w2s;
} ubo;
layout(binding = 1, set = 0) uniform usampler3D blockPalette;

layout(location = 0) lowp flat in uvec3 unorm;
layout(location = 1) in vec3 sample_point;
layout(location = 2) flat in uint sample_block;

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
    // uint uv_encoded = packUnorm2x16(old_uv);

    // outOldUv = uv_shift;

    // outMatNorm.x = (float(mat)-127.0)/127.0;
    outMatNorm.yzw = uvec3(unorm);
    outMatNorm.x = 9;

    ivec3 ipos = ivec3(sample_point);

    // if(sample_block != 0) {
    outMatNorm.x = GetVoxel(int(sample_block), ipos).x;
    // }
    // outDepth = depth;
    // outGbuffer_downscaled = outGbuffer;
    // outPosMat.xyz = pos;
    // outPosMat.xyz = pos;
    // outPosMat.w = MatID;

    // outNorm = norm;
    // outPosDiff = pos_old;
}