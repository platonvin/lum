#version 450

// #extension vertexPipelineStoresAndAtomics : enable

precision highp float;

layout(location = 0) in  vec3 posIn;
layout(location = 1) in  vec3 velIn; //why not maybe effects
layout(location = 2) in float lifeTimeIn;
layout(location = 3) in  uint matIDIn;

// layout(location = 0)      out float depth;
layout(location = 0) out VS_OUT {
        //  vec2 old_uv;
        float size; 
    flat uint mat;
} vs_out;

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
layout(set = 0, binding = 1, r16i) uniform restrict  readonly iimage3D blocks;
layout(set = 0, binding = 2, r8ui) uniform restrict writeonly uimage3D blockPalette;

const int BLOCK_PALETTE_SIZE_X = 64;
const int STATIC_BLOCK_COUNT = 15; // 0 + 1..static block count so >=STATIC_BLOCK_COUNT

ivec3 voxel_in_palette(ivec3 relative_voxel_pos, int block_id) {
    int block_x = block_id % BLOCK_PALETTE_SIZE_X;
    int block_y = block_id / BLOCK_PALETTE_SIZE_X;

    return relative_voxel_pos + ivec3(16*block_x, 16*block_y, 0);
}


void main() {

    float deltaTime = 1.0/75.0;

    vec4 world_pos     = vec4(posIn,0);
    vec4 world_pos_old = vec4(posIn - velIn*deltaTime,0);

    vec3 clip_coords = (ubo.trans_w2s*world_pos).xyz;
         clip_coords.z = 1.0+clip_coords.z;
    // vec3 clip_coords_old = (ubo.trans_w2s_old*world_pos_old).xyz;

    gl_Position  = vec4(clip_coords, 1);    
    
    // mat3 m2w_normals = transpose(inverse(mat3(pco.trans_m2w))); 

    // vs_out.old_uv = (clip_coords_old.xy - clip_coords.xy)/2.0; //0..1
    vs_out.mat = uint(matIDIn);
    float size = lifeTimeIn / 14.0;
    vs_out.size = size;

    bool should_map = false;
    if(size > .15){
        ivec3 target_voxel_in_world = ivec3(posIn);
        ivec3 target_block_in_world = target_voxel_in_world / 16;

        int target_block_id = imageLoad(blocks, target_block_in_world).x;
        ivec3 target_voxel_in_palette = voxel_in_palette(target_voxel_in_world % 16, target_block_id);
        if(target_block_id>=STATIC_BLOCK_COUNT){
            imageStore(blockPalette, target_voxel_in_palette, uvec4(matIDIn));
        }
    }
    //LOL dont even need map_particles shader - can be done here. Price is compatibility
}
