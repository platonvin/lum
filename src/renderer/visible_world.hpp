#pragma once

#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

#include <defines.hpp>
#include <stdio.h>
#include <io.h>
#include <iostream>
#include <vector>
#include <tuple>

#include "render.hpp"

#include <vulkan/vulkan.h>
#include <VMA/vk_mem_alloc.h>

namespace ogt { //this library for some reason uses ogt_ in cases when it will never intersect with others BUT DOES NOT WHEN IT FOR SURE WILL
    #include <ogt_vox.h>
    #include <ogt_voxel_meshify.hpp>
} //so we'll have to use ogt::ogt_func :(

#include "primitives.hpp"

using namespace glm; 
using namespace std;

//Everything is X -> Y -> Z order in arrays (vectors)
#define VISIBLE_WORLD_SIZE 8  // visible world is !![maximum] WORLD_SIZE x WORLD_SIZE x WORLD_SIZE size in chunks. Typically smaller
#define CHUNK_SIZE 8  // each chunk in world is CHUNK_SIZE x CHUNK_SIZE x CHUNK_SIZE
// #define WORLD_SIZE... cannot be defined :/

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

/*
concept is: chunks are independend, when loaded chunks change we just copy from loaded once to united. Might change
*/
// class b {

// }
class VisualWorld {
public:
    vec3 worldShift;
    //size might change in runtime so dynamic. Overhead is insignificant
    table3d<ChunkInMem> loadedChunks;
    // vector<Mesh> objects;
    table3d<BlockID_t> unitedBlocks; // used to traversal rays 

    Material matPalette[MATERIAL_PALETTE_SIZE];
    Block blocksPalette[BLOCK_PALETTE_SIZE];

    void init();
    void update();
    void cleanup();
};
// int main() {
//     // loader l = {};
//     VisualWorld cs = {};
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