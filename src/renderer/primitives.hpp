#pragma once

#include <vector>

#include <vulkan/vulkan.h>
#include <VMA/vk_mem_alloc.h>
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

using namespace glm;
using namespace std;

//Everything is X -> Y -> Z order in arrays (vectors)
#define BLOCK_SIZE 16 // each block in common world is BLOCK_SIZE x BLOCK_SIZE x BLOCK_SIZE
#define MATERIAL_PALETTE_SIZE 256 //0 always empty
#define    BLOCK_PALETTE_SIZE 256 //0 always empty

typedef   u8 MatID_t;
typedef char BlockID_t;

typedef struct Material {
    // glm::vec4<u8> casd;
    vec4 color; //colo   r AND transparancy
    f32 emmit; //emmits same color as reflect
    f32 rough;
} Material;
typedef struct Voxel {
    MatID_t matID;
} Voxel;
typedef struct Vertex {
    vec3 pos;
    vec3 norm;
    MatID_t matID;
} Vertex;
typedef struct MeshData {
    vector<VkBuffer> vertices;
    vector<VkBuffer>  indices;
    vector<VmaAllocation> verticesAllocation;
    vector<VmaAllocation>  indicesAllocation;
    u32 icount;
} MeshData;
typedef struct Mesh {
    MeshData data;
    mat4 transform;
} Mesh;
typedef struct Block {
    Voxel voxels[BLOCK_SIZE][BLOCK_SIZE][BLOCK_SIZE];
} Block;