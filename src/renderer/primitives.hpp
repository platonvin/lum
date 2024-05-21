#pragma once

#include "vulkan/vulkan_core.h"
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
#define    BLOCK_PALETTE_SIZE 128 //0 always empty

typedef   u8 MatID_t;
typedef  i32 BlockID_t;

typedef struct Material {
    // glm::vec4<u8> casd;
    vec4 color; //colo   r AND transparancy
    f32 emmit; //emmits same color as reflect
    f32 rough;
} Material;
typedef u8 Voxel;
typedef struct Vertex {
    vec3 pos;
    vec3 norm;
    MatID_t matID;
} Vertex;
// typedef struct MeshData {
//     vector<VkBuffer> vertices;
//     vector<VkBuffer>  indices;
//     vector<VmaAllocation> verticesAllocation;
//     vector<VmaAllocation>  indicesAllocation;
//     u32 icount;
// } MeshData;
// typedef struct VoxelData {
//     vector<VkBuffer> vertices;
//     vector<VkBuffer>  indices;
//     vector<VmaAllocation> verticesAllocation;
//     vector<VmaAllocation>  indicesAllocation;
//     u32 icount;
// } MeshData;
typedef struct Buffer {
    vector<VkBuffer> buf;
    vector<VmaAllocation> alloc;
} Buffers;
typedef struct Image {
    vector<VkImage> image;
    vector<VkImageView> view;
    vector<VmaAllocation> alloc;
} Images;

typedef struct Mesh {
    //everything is Staged per frame in flight, so you can update it faster. But costs double the memory
    //VkBuffers for ray generation
    Buffers vertexes;
    Buffers indexes;
    u32 icount;
    Images voxels; //3d image of voxels in this mesh, used to represent mesh to per-frame world voxel representation

    mat4 transform; //used to transform from self coordinate system to world coordinate system
    mat4 old_transform; //for denoising
    
    ivec3 size;
    // Voxel* host_voxels; // single copy of voxel memory on cpu. If NULL then you store your data yourself
} Mesh;


typedef struct Block {
    Voxel voxels[BLOCK_SIZE][BLOCK_SIZE][BLOCK_SIZE];
} Block;

//just 3d array. Content invalid after allocate()
template <typename Type> class table3d {
private:
    void* memory = NULL;
    ivec3 _size = {};
public:
    //makes content invalid 
    void allocate(int x, int y, int z) {
        _size = ivec3(x,y,z);
        this->memory = malloc(x*y*z);
    }
    //makes content invalid 
    void allocate(ivec3 size) {
        this->allocate(size.x, size.y, size.z);
    }
    Type* data() {
        return this->memory.data();
    }
    uvec3 size() {
        return _size;
    }
    Type& operator()(int x, int y, int z) {
        return this->memory [x + _size.x*y + (_size.x*_size.y)*z];
    }
};