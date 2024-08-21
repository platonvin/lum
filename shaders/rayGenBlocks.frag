#version 450
layout(early_fragment_tests) in;

#define varp highp
precision varp int;
precision varp float;

#extension GL_EXT_shader_8bit_storage : enable
#extension GL_EXT_shader_16bit_storage : enable
#extension GL_EXT_shader_explicit_arithmetic_types : enable
#extension GL_EXT_scalar_block_layout : enable

// layout(scalar, push_constant) restrict readonly uniform constants{
//     int16_t block;
//     i16vec3 shift;
//     i8vec4 inorm;
// } pco;

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
    // mat4 trans_w2s;
    // vec3 campos;
    // vec3 camdir;
    // vec3 horizline_scaled;
    // vec3 vertiline_scaled;
    // vec2 frame_size;
    // int timeseed;
    // vec3 globalLightDir;
    // mat4 lightmap_proj;
} ubo;

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
    // uint uv_encoded = packUnorm2x16(old_uv);

    // outOldUv = uv_shift;
    // int sample_block = int(pco.block);

    // bunorm.x = packUint2x16(u16vec2(sample_block, normal_encoded.x));
    // bunorm.y = packUint2x16(u16vec2(normal_encoded.y, normal_encoded.z));
    uint sample_block = unpackUint2x16(bunorm.x).x;
    // uvec3 normal_encoded = uvec3(
    //     unpackUint2x16(bunorm.x).y,
    //     unpackUint2x16(bunorm.y)
    // );
    uint normal_encoded = unpackUint2x16(bunorm.x).y;
    uint s = (normal_encoded & (1<<7))>>7; //0 if positive 1 if negative
    uvec3 axis = uvec3(
        ((normal_encoded & (1<<0))>>0),
        ((normal_encoded & (1<<1))>>1),
        ((normal_encoded & (1<<2))>>2)
    );
    ivec3 inorm = ivec3(axis) * (1 - int(s)*2);

    // vec3 norm = uvec3(((pco.normal+1.0)/2.0)*255.0);
    // inorm = ivec3(,0,0);
    uvec3 _normal_encoded = uvec3(((ivec3(inorm) + 1)*255)/2);
    outMatNorm.yzw = uvec3(_normal_encoded);
    // outMatNorm.x = 9;

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