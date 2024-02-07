#pragma once


#include <glm/glm.hpp>
#include <defines.hpp>
#include <stdio.h>
#include <io.h>
#include <iostream>
#include <vector>
#include <tuple>

#define VOX_IMPLEMENTATION 
#define OGT_VOXEL_MESHIFY_IMPLEMENTATION 
namespace ogt { //this library for some reason uses ogt_ in cases when it will never intersect with others BUT DOES NOT WHEN IT FOR SURE WILL
    #include <ogt_vox.h>
    #include <ogt_voxel_meshify.hpp>
} //so we'll have to use ogt::ogt_func :(

using namespace glm; 
using namespace std;

//Everything is X -> Y -> Z order in arrays (vectors)
#define BLOCK_SIZE 16 // each block in common world is BLOCK_SIZE x BLOCK_SIZE x BLOCK_SIZE
#define CHUNK_SIZE 8  // each chunk in world is CHUNK_SIZE x CHUNK_SIZE x CHUNK_SIZE
// #define WORLD_SIZE... cannot be defined :/
#define VISIBLE_WORLD_SIZE 8  // visible world is !![maximum] WORLD_SIZE x WORLD_SIZE x WORLD_SIZE size in chunks. Typically smaller
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
typedef struct Mesh {
    vector<Vertex> vertices;
    vector<u32> indices;
} Mesh;
typedef struct Block {
    Voxel voxels[BLOCK_SIZE][BLOCK_SIZE][BLOCK_SIZE];
} Block;

// typedef struct MatPalette {
//     Material mats[MATERIAL_PALETTE_SIZE];
// } MatPalette;
// typedef struct BlockPalette {
//     Block blocks[BLOCK_PALETTE_SIZE];
// } PaletteBlockPalette;

typedef struct ChunkInMem {
    //does not change in a runtime
    BlockID_t blocks[CHUNK_SIZE][CHUNK_SIZE][CHUNK_SIZE];
    Mesh mesh;
} ChunkInMem;

typedef struct ChunkInTable {
    //offset, size, file, etc./...
} ChunkInTable;

template <typename Type>
class table3d {
    vector<Type> chunksMem;
    u32 lenx, leny, lenz;
public:
    void resize(u32 x, u32 y, u32 z) {
        this->chunksMem.resize(x*y*z);
    }
    tuple<u32, u32, u32> size(u32 x, u32 y, u32 z) {
        return {lenx, leny, lenz};
    }
    Type& operator()(u32 x, u32 y, u32 z) {
        return this->chunksMem [x + lenx*y + lenx*lenz*z];
    }
};
/*
concept is: chunks are independend, when loaded chunks change we just copy from loaded once to united. Might change*/
class ChunkSystem {
public:
    //size might change in runtime so vectors. Overhead is insignificant
    table3d<ChunkInMem> chunks;
    // table<int>;

    //Changes all the time
    Mesh unitedMesh  ; // used to gen rays
    table3d<BlockID_t> unitedBlocks; // used to traversal rays 

    Material matPalette[MATERIAL_PALETTE_SIZE];
    Block blocksPalette[BLOCK_PALETTE_SIZE];

    void init();
    void update();
    void cleanup();
};

template<typename Type> tuple<Type*, u32> readFileBuffer(const char* path) {
    FILE* fp = fopen(path, "rb");
    u32 buffer_size = _filelength(_fileno(fp));
    Type* buffer = new Type[buffer_size];
    fread(buffer, buffer_size, 1, fp);
    fclose(fp);

    return {buffer, buffer_size};
}

void ChunkSystem::init(){
    auto [buffer, buffer_size] = readFileBuffer<u8>("assets/scene.vox");
    const ogt::vox_scene* scene = ogt::vox_read_scene(buffer, buffer_size);
    delete[] buffer;

        printl(scene->num_cameras);
        printl(scene->num_groups);
        printl(scene->num_instances);
        printl(scene->num_layers);
        printl(scene->num_models);

    assert(scene->num_models < BLOCK_PALETTE_SIZE-1);
    for(i32 i=0; i<scene->num_models; i++){
    //    this->blocksPalette[i+1] = scene->models[i]->voxel_data;
        assert(scene->models[i]->size_x == BLOCK_SIZE);
        assert(scene->models[i]->size_y == BLOCK_SIZE);
        assert(scene->models[i]->size_z == BLOCK_SIZE);
        u32 sizeToCopy = BLOCK_SIZE*BLOCK_SIZE*BLOCK_SIZE;
        memcpy(&this->blocksPalette[i+1], scene->models[i]->voxel_data, sizeToCopy);
    }
    //i=0 alwaus empty mat/color
    for(i32 i=0; i<MATERIAL_PALETTE_SIZE; i++){
        this->matPalette[i].color = vec4(
            scene->palette.color[i].r,
            scene->palette.color[i].g,
            scene->palette.color[i].b,
            scene->palette.color[i].a
        );
        this->matPalette[i].emmit = scene->materials.matl[i].emit;
        this->matPalette[i].rough = scene->materials.matl[i].rough;
    }

    //Chunks is just 3d psewdo dynamic array. Same as united blocks. Might change on settings change
    this->chunks.resize(1,1,1);
    this->unitedBlocks.resize(1,1,1);

    ChunkInMem singleChunk = {};
    singleChunk.blocks[0][0][0] = 1;
    ogt::ogt_voxel_meshify_context ctx = {};
    ogt::ogt_mesh_rgba rgbaPalette[256] = {};
    //comepletely unnesessary but who cares
    for(i32 i=0; i<256; i++){
        ivec4 icolor = ivec4(this->matPalette->color);
        rgbaPalette[i].r = icolor.r;
        rgbaPalette[i].g = icolor.g;
        rgbaPalette[i].b = icolor.b;
        rgbaPalette[i].a = icolor.a;
    }
    // rgbaPalette.a
    ogt::ogt_mesh* mesh = ogt::ogt_mesh_from_paletted_voxels_polygon(&ctx, (const u8*)this->blocksPalette[1].voxels, BLOCK_SIZE, BLOCK_SIZE, BLOCK_SIZE, rgbaPalette);
    Mesh singleBlockMesh = {};
printl(mesh->vertex_count);
printl(mesh->index_count);
    for(i32 i=0; i<mesh->vertex_count; i++){
        Vertex v = {};
            v.pos.x = mesh->vertices[i].pos.x;
            v.pos.y = mesh->vertices[i].pos.y;
            v.pos.z = mesh->vertices[i].pos.z;

            v.norm.x = mesh->vertices[i].normal.x;
            v.norm.y = mesh->vertices[i].normal.y;
            v.norm.z = mesh->vertices[i].normal.z;

        assert(mesh->vertices[i].palette_index < 256);
        assert(mesh->vertices[i].palette_index != 0);
        v.matID = mesh->vertices[i].palette_index;

        singleBlockMesh.vertices.push_back(v);
    }
    for(i32 i=0; i<mesh->index_count; i++){
        u32 index = mesh->indices[i];
        singleBlockMesh.indices.push_back(index);
    }
    //so we have a mesh in indexed vertex form
    
    singleChunk.blocks[0][0][0] = 1;
    singleChunk.mesh = singleBlockMesh;
    this->chunks(0,0,0) = singleChunk;
    this->unitedBlocks(0,0,0) = 1; //so 1 from block palette. For testing
    this->unitedMesh = singleChunk.mesh;
}

void ChunkSystem::update(){
/*
nothing here... Yet
*/
}

void ChunkSystem::cleanup(){
/*
nothing here... Yet
*/
}

// int main() {
//     // loader l = {};
//     ChunkSystem cs = {};
//     cs.init();
// }



/*
so the plan is:
world is chunks
chunks are in file
chunk is blocks
blocks are in another file
block side is BLOCK_SIZE size
block is bunch of empty and (ussually and) non-empty voxels 
voxel is Palette number representing material from palette

so gpu is marching through blocks, and blocks are relatively small and there is not that many of them
problem is that not everything is blocks
so we'll have temporary blocks that are potentially modified every frame and contain voxelized divided into blocks versions of models that are not part of the world

RENDERING:
basic frame consept:
    rasterize all the things as typical triangles to create rays
    traversal rays through only-voxel grid
    end rays and write data to another frame
    use it in denoiser and show on screen

each frame (or more likely even more often):
    update animations mat4's
only each frame (with cs):
    update voxel grid

all the objects should be voxelated, so updating them could be done in compute shader:
    for each voxel that might intersect with model:
        copy corresponding model voxel into world.
        if it is 0 it just makes nothing
    also if using sdf should recompute them (probably in fast non presice way)

features:
    1 gig of vram is quite a lot for old GPUs and also does not really fit into caches
    so we'll fix that with palette... For blocks! There'll be plenty of elements that are used multiple times
    so we can just divide world in blocks and blocks are 3d grids of voxels each.
    But it comes at a price - we'll have to write our voxelated non-main-world-aligned objects to blocks and then write blocks indicies into  

this makes reflections kind of crappy but EXTREMELY fast even on non-rt gpus
also when reflections are not glossy it looks just fine
problem is only glossy reflective non-aligned objects (curved mirror). That will look bad
*/