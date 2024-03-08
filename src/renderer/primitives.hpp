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
#define    BLOCK_PALETTE_SIZE 2 //0 always empty

typedef   u8 MatID_t;
typedef  i32 BlockID_t;

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
} Buffer;
typedef struct Image {
    vector<VkImage> image;
    vector<VkImageView> view;
    vector<VmaAllocation> alloc;
} Image;

typedef struct Mesh {
    //everything is Staged per frame in flight, so you can update it faster. But costs double the memory
    //VkBuffers for ray generation
    Buffer vertexes;
    Buffer indexes;
    u32 icount;
    //used to transform from self coordinate system to world coordinate system
    mat4 transform;
    //3d image of voxels in this mesh, used to represent mesh to per-frame world voxel representation
    Image voxels;
    ivec3 size;
    // vector<VkBuffer> voxels;
} Mesh;
typedef struct Block {
    Voxel voxels[BLOCK_SIZE][BLOCK_SIZE][BLOCK_SIZE];
} Block;

template <typename Type>
class table3d {
private:
    vector<Type> memory = {};
    uvec3 _size = {};
public:
    //makes content invalid 
    void set_size(u32 x, u32 y, u32 z) {
        _size = uvec3(x,y,z);
        this->memory.resize(x*y*z);
    }
    Type* data() {
        return this->memory.data();
    }
    uvec3 size() {
        return _size;
    }
    Type& operator()(u32 x, u32 y, u32 z) {
        return this->memory [x + _size.x*y + (_size.x*_size.y)*z];
    }
};