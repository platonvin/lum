#include "render.hpp"
#include "defines/macros.hpp"
#include <glm/gtx/string_cast.hpp>
#include <glm/gtx/quaternion.hpp>

using std::array;
using std::vector;

using glm::u8, glm::u16, glm::u16, glm::u32;
using glm::i8, glm::i16, glm::i32;
using glm::f32, glm::f64;
using glm::defaultp;
using glm::quat;
using glm::ivec2,glm::ivec3,glm::ivec4;
using glm::i8vec2,glm::i8vec3,glm::i8vec4;
using glm::i16vec2,glm::i16vec3,glm::i16vec4;
using glm::uvec2,glm::uvec3,glm::uvec4;
using glm::u8vec2,glm::u8vec3,glm::u8vec4;
using glm::u16vec2,glm::u16vec3,glm::u16vec4;
using glm::vec,glm::vec2,glm::vec3,glm::vec4;
using glm::dvec2,glm::dvec3,glm::dvec4;
using glm::mat, glm::mat2, glm::mat3, glm::mat4;
using glm::dmat2, glm::dmat3, glm::dmat4;
using glm::quat_identity;

tuple<int, int> get_block_xy (int N);

#ifdef DISTANCE_FIELD
struct bp_tex {u8 r; u8 g;};

#else
struct bp_tex {u8 r;};

#endif
//block palette on cpu side is expected to be array of pointers to blocks
void LumRenderer::updateBlockPalette (Block** blockPalette) {
    //block palette is in u8, but for easy copying we convert it to u16 cause block palette is R16_UNIT
    VkDeviceSize bufferSize = (sizeof (bp_tex)) * 16 * BLOCK_PALETTE_SIZE_X * 16 * BLOCK_PALETTE_SIZE_Y * 16;
    table3d<bp_tex> blockPaletteLinear = {};
    blockPaletteLinear.allocate (16 * BLOCK_PALETTE_SIZE_X, 16 * BLOCK_PALETTE_SIZE_Y, 16);
    blockPaletteLinear.set ({0});
    // printl(static_block_palette_size)
    for (i32 N = 0; N < static_block_palette_size; N++) {
        for (i32 x = 0; x < BLOCK_SIZE; x++) {
            for (i32 y = 0; y < BLOCK_SIZE; y++) {
                for (i32 z = 0; z < BLOCK_SIZE; z++) {
                    auto [block_x, block_y] = get_block_xy (N);
                    if (blockPalette[N] == NULL) {
                        blockPaletteLinear (x + 16 * block_x, y + 16 * block_y, z).r = 0;
                    } else {
                        if (N < static_block_palette_size) {
                            blockPaletteLinear (x + 16 * block_x, y + 16 * block_y, z).r = (u16) blockPalette[N]->voxels[x][y][z];
                        } else {
                            blockPaletteLinear (x + 16 * block_x, y + 16 * block_y, z).r = 0;
                        }
                    }
                }
            }
        }
    }
// TRACE();
    VkBufferCreateInfo 
        stagingBufferInfo = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        stagingBufferInfo.size = bufferSize;
        stagingBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    VmaAllocationCreateInfo 
        stagingAllocInfo = {};
        stagingAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        stagingAllocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
        stagingAllocInfo.requiredFlags = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    VkBuffer stagingBuffer = {};
    VmaAllocation stagingAllocation = {};
    void* data = NULL;
    VK_CHECK (vmaCreateBuffer (render.VMAllocator, &stagingBufferInfo, &stagingAllocInfo, &stagingBuffer, &stagingAllocation, NULL));
    VK_CHECK (vmaMapMemory (render.VMAllocator, stagingAllocation, &data));
    assert (data != NULL);
    assert (bufferSize != 0);
    assert (blockPaletteLinear.data() != NULL);
    memcpy (data, blockPaletteLinear.data(), bufferSize);
    vmaUnmapMemory (render.VMAllocator, stagingAllocation);
    for (i32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        render.copyBufferSingleTime (stagingBuffer, &originBlockPalette[i], uvec3 (16 * BLOCK_PALETTE_SIZE_X, 16 * BLOCK_PALETTE_SIZE_Y, 16));
    }
    vmaDestroyBuffer (render.VMAllocator, stagingBuffer, stagingAllocation);
}

//TODO: do smth with frames in flight
void LumRenderer::updateMaterialPalette (Material* materialPalette) {
    VkDeviceSize bufferSize = sizeof (Material) * 256;
    VkBufferCreateInfo 
        stagingBufferInfo = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        stagingBufferInfo.size = bufferSize;
        stagingBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    VmaAllocationCreateInfo 
        stagingAllocInfo = {};
        stagingAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        stagingAllocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
        stagingAllocInfo.requiredFlags = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    VkBuffer stagingBuffer = {};
    VmaAllocation stagingAllocation = {};
    vmaCreateBuffer (render.VMAllocator, &stagingBufferInfo, &stagingAllocInfo, &stagingBuffer, &stagingAllocation, NULL);
    void* data;
    vmaMapMemory (render.VMAllocator, stagingAllocation, &data);
    memcpy (data, materialPalette, bufferSize);
    vmaUnmapMemory (render.VMAllocator, stagingAllocation);
    for (i32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        render.copyBufferSingleTime (stagingBuffer, &this->materialPalette[i], uvec3 (6, 256, 1));
    }
    vmaDestroyBuffer (render.VMAllocator, stagingBuffer, stagingAllocation);
}

char* readFileBuffer(const char* path, size_t* size) {
    FILE* fp = fopen (path, "rb");
    assert (fp != NULL);
    fseek(fp, 0, SEEK_END);
    *size = ftell(fp);
    rewind(fp);
    char* buffer = (char*) malloc (*size);
    assert (buffer != NULL);
    fread (buffer, *size, 1, fp);
    fclose (fp);
    return buffer;
}
// namespace ogt { //this library for some reason uses ogt_ in cases when it will never intersect with others BUT DOES NOT WHEN IT FOR SURE WILL
#include <ogt_vox.hpp>
#include <ogt_voxel_meshify.hpp>

void LumRenderer::load_scene (const char* vox_file) {
    size_t buffer_size;
    FILE* fp = fopen(vox_file, "rb");
    if (fp == NULL) {
        origin_world.set(0);
        return;
    }
    fseek(fp, 0, SEEK_END);
    buffer_size = ftell(fp);
    rewind(fp);

    if (buffer_size < sizeof(ivec3)) {
        fclose(fp);
        origin_world.set(0);
        return;
    }

    char* buffer = (char*)malloc(buffer_size);

    fread(buffer, buffer_size, 1, fp);
    fclose(fp);

    ivec3* header = (ivec3*)buffer;
    ivec3 stored_world_size = *header;
    BlockID_t* stored_world = (BlockID_t*)(buffer + sizeof(ivec3));

    assert((char*)stored_world < buffer + buffer_size);
    
    DEBUG_LOG(world_size.x);
    DEBUG_LOG(world_size.y);
    DEBUG_LOG(world_size.z);
    DEBUG_LOG(static_block_palette_size);

    ivec3 size2read = glm::clamp(stored_world_size, ivec3(0), ivec3(world_size));

    for (int xx = 0; xx < size2read.x; xx++) {
        for (int yy = 0; yy < size2read.y; yy++) {
            for (int zz = 0; zz < size2read.z; zz++) {
                size_t index = xx + stored_world_size.x * yy + (stored_world_size.x * stored_world_size.y) * zz;
                assert(index < (buffer_size - sizeof(ivec3)) / sizeof(BlockID_t));
                
                BlockID_t loaded_block = stored_world[index];
                origin_world(xx, yy, zz) = glm::clamp(loaded_block, BlockID_t(0), BlockID_t(static_block_palette_size));
            }
        }
    }

    free(buffer);
    return;
}

void LumRenderer::save_scene (const char* vox_file) {
    FILE* fp = fopen(vox_file, "wb");
    assert(fp != NULL);
    assert(world_size.x > 0 && world_size.y > 0 && world_size.z > 0);
    fwrite(&world_size, sizeof(ivec3), 1, fp);
    fwrite(origin_world.data(), sizeof(BlockID_t), world_size.x * world_size.y * world_size.z, fp);
    fclose(fp);
}

//size limited by 256^3
void LumRenderer::load_mesh(Mesh* mesh, const char* vox_file, bool _make_vertices, bool extrude_palette) {
    size_t buffer_size;
    char* buffer = readFileBuffer(vox_file, &buffer_size);
    assert(buffer != NULL);
    const ogt::vox_scene* scene = ogt::vox_read_scene((u8*)buffer, buffer_size);
    free(buffer);
    assert(scene != NULL);
    assert(scene->num_models == 1);
    
    load_mesh(mesh, (Voxel*)scene->models[0]->voxel_data, scene->models[0]->size_x, scene->models[0]->size_y, scene->models[0]->size_z, _make_vertices);
    
    if (extrude_palette && !hasPalette) {
        hasPalette = true;
        for (int i = 0; i < MATERIAL_PALETTE_SIZE; i++) {
            mat_palette[i].color = vec4(
                scene->palette.color[i].r / 256.0,
                scene->palette.color[i].g / 256.0,
                scene->palette.color[i].b / 256.0,
                scene->palette.color[i].a / 256.0
            );
            mat_palette[i].emmit = 0;
            float rough;
            switch (scene->materials.matl[i].type) {
                case ogt::matl_type_diffuse:
                    rough = 1.0;
                    break;
                case ogt::matl_type_emit:
                    mat_palette[i].emmit = scene->materials.matl[i].emit * (2.0 + scene->materials.matl[i].flux * 4.0);
                    rough = 0.5;
                    break;
                case ogt::matl_type_metal:
                    rough = (scene->materials.matl[i].rough + (1.0 - scene->materials.matl[i].metal)) / 2.0;
                    break;
                default:
                    rough = 0;
                    break;
            }
            mat_palette[i].rough = rough;
        }
    }
    /*
        roughness
        IOR
        Specular
        Metallic

        Emmission
        Power - radiant flux
        Ldr
    */
    ogt::vox_destroy_scene(scene);
}

void LumRenderer::load_block(Block** block, const char* vox_file) {
    size_t buffer_size;
    char* buffer = readFileBuffer(vox_file, &buffer_size);
    assert(buffer != NULL);
    const ogt::vox_scene* scene = ogt::vox_read_scene((u8*)buffer, buffer_size);
    free(buffer);
    assert(scene != NULL);
    assert(scene->num_models == 1);
    if ((*block) == NULL) {
        *block = new Block;
    }
    (*block)->mesh.size = ivec3(scene->models[0]->size_x, scene->models[0]->size_y, scene->models[0]->size_z);
    (*block)->mesh.shift = vec3(0);
    (*block)->mesh.rot = quat_identity<float, defaultp>();
    assert(scene->models[0]->size_x > 0 && scene->models[0]->size_y > 0 && scene->models[0]->size_z > 0);
    make_vertices(&((*block)->mesh), (Voxel*)scene->models[0]->voxel_data, scene->models[0]->size_x, scene->models[0]->size_y, scene->models[0]->size_z);
    for (int x = 0; x < scene->models[0]->size_x; x++) {
        for (int y = 0; y < scene->models[0]->size_y; y++) {
            for (int z = 0; z < scene->models[0]->size_z; z++) {
                (*block)->voxels[x][y][z] = (u16)scene->models[0]->voxel_data[x + y * scene->models[0]->size_x + z * scene->models[0]->size_x * scene->models[0]->size_y];
            }
        }
    }
    ogt::vox_destroy_scene(scene);
}

#define free_helper(dir) \
if(not (*block)->mesh.triangles.dir.indexes.empty()) vmaDestroyBuffer(render.VMAllocator, (*block)->mesh.triangles.dir.indexes[i].buffer, (*block)->mesh.triangles.dir.indexes[i].alloc);
void LumRenderer::free_block(Block** block) {
    assert(block != NULL);
    assert(*block != NULL);
    
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        free_helper(Pzz);
        free_helper(Nzz);
        free_helper(zPz);
        free_helper(zNz);
        free_helper(zzP);
        free_helper(zzN);
        if (!(*block)->mesh.triangles.vertexes.empty()) {
            vmaDestroyBuffer(render.VMAllocator, (*block)->mesh.triangles.vertexes[i].buffer, (*block)->mesh.triangles.vertexes[i].alloc);
        }
    }
    delete *block;
    *block = NULL;
}

void LumRenderer::load_mesh(Mesh* mesh, Voxel* Voxels, int x_size, int y_size, int z_size, bool _make_vertices) {
    assert(mesh != NULL);
    assert(Voxels != NULL);
    assert(x_size > 0 && y_size > 0 && z_size > 0);
    mesh->size = ivec3(x_size, y_size, z_size);
    if (_make_vertices) {
        make_vertices(mesh, Voxels, x_size, y_size, z_size);
    }
    mesh->voxels = create_RayTrace_VoxelImages(Voxels, mesh->size);
    mesh->shift = vec3(0);
    mesh->rot = quat_identity<float, defaultp>();
}

//frees only gpu side stuff, not mesh ptr
#undef  free_helper
#define free_helper(dir) \
if(not mesh->triangles.dir.indexes.empty()) vmaDestroyBuffer(render.VMAllocator, mesh->triangles.dir.indexes[i].buffer, mesh->triangles.dir.indexes[i].alloc);
void LumRenderer::free_mesh(Mesh* mesh) {
    assert(mesh != NULL);
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        free_helper(Pzz);
        free_helper(Nzz);
        free_helper(zPz);
        free_helper(zNz);
        free_helper(zzP);
        free_helper(zzN);
        if (!mesh->triangles.vertexes.empty()) {
            vmaDestroyBuffer(render.VMAllocator, mesh->triangles.vertexes[i].buffer, mesh->triangles.vertexes[i].alloc);
        }
        if (!mesh->voxels.empty()) {
            vmaDestroyImage(render.VMAllocator, mesh->voxels[i].image, mesh->voxels[i].alloc);
        }
        if (!mesh->voxels.empty()) {
            vkDestroyImageView(render.device, mesh->voxels[i].view, NULL);
        }
    }
}
bool operator== (const PackedVoxelVertex& one, const PackedVoxelVertex& other) {
    return
        (one.pos.x == other.pos.x) &&
        (one.pos.y == other.pos.y) &&
        (one.pos.z == other.pos.z)
        ;
}

bool operator!= (const PackedVoxelVertex& one, const PackedVoxelVertex& other) {
    return ! (one == other);
}

bool operator< (const PackedVoxelVertex& one, const PackedVoxelVertex& other) {
    return one != other;
}

PackedVoxelQuad pack_quad (const array<PackedVoxelVertex, 6> vertices, uvec3 norm) {
    PackedVoxelQuad quad = {};
    vector<PackedVoxelVertex> uniq;
    unsigned char mat = vertices[0].matID;
    for (auto v : vertices) {
        cout << glm::to_string (v.pos) << " \n";
        // uniq.insert(v);
        for (auto _v : uniq) {
            if (v == _v) {
                goto label;
            }
        }
        uniq.push_back (v);
label:
        ;
        // assert(mat == v.matID);
    }
    assert (uniq.size() == 4);
    cout << " \n";
    uvec3 low = uvec3 (+10000); //_corner
    uvec3 high = uvec3 (0); //_corner
    uvec3 diff = {};
    for (auto v : uniq) {
        cout << glm::to_string (v.pos) << " \n";
        low = glm::min (low, uvec3 (v.pos));
        high = glm::max (high, uvec3 (v.pos));
    }
    //general direction is from negative to positive
    diff = high - low;
    assert (any (greaterThan (diff, {0, 0, 0})));
    uvec3 plane = uvec3 (1) - abs (norm);
    cout << glm::to_string (plane) << " \n";
    cout << glm::to_string (diff) << " \n";
    assert (((diff.x == 0) == (plane.x == 0)));
    assert (((diff.y == 0) == (plane.y == 0)));
    assert (((diff.z == 0) == (plane.z == 0)));
    if (abs (norm).x == 1) {
        quad.size.x = diff.y;
        quad.size.y = diff.z;
    } else if (abs (norm).y == 1) {
        quad.size.x = diff.x;
        quad.size.y = diff.z;
    } else if (abs (norm).z == 1) {
        quad.size.x = diff.x;
        quad.size.y = diff.y;
    }
    quad.pos = low;
    return quad;
}

void repack_helper (vector<PackedVoxelVertex>& vs, vector<PackedVoxelQuad>& qs, uvec3 norm) {
    assert ((vs.size() % 6) == 0);
    for (int i = 0; i < vs.size() / 6; i++) {
        qs.push_back (pack_quad ({
            vs[i + 0], vs[i + 1], vs[i + 2],
            vs[i + 3], vs[i + 4], vs[i + 5],
        }, norm));
    }
}

//defenetly not an example of highly optimized code
void LumRenderer::make_vertices (Mesh* mesh, Voxel* Voxels, int x_size, int y_size, int z_size) {
    ogt::ogt_voxel_meshify_context ctx = {};
    //code to generate cube lol
    // const u8 a[] = {1};
    // ogt::ogt_mesh* m = ogt::ogt_mesh_from_paletted_voxels_simple(&ctx, (const u8*)a, 1, 1, 1, NULL);
    // for(int i=0; i<m->index_count; i++){
    // printf("vec3(%f,%f,%f)\n", m->vertices[m->indices[i]].pos.x, m->vertices[m->indices[i]].pos.y, m->vertices[m->indices[i]].pos.z);
    // }
    // abort();
    //at this point i just hope compilers know how to optimize sweet vectors into mallocs
    vector<Voxel> contour (x_size* y_size* z_size);
    for (int xx = 0; xx < x_size; xx++) {
        for (int yy = 0; yy < y_size; yy++) {
            for (int zz = 0; zz < z_size; zz++) {
                contour[xx + x_size * yy + x_size * y_size * zz] = (Voxels[xx + x_size* yy + x_size* y_size* zz] == 0) ? 0 : 1;
            }
        }
    }
    ogt::ogt_int_mesh* ogt_mesh = ogt::my_int_mesh_from_paletted_voxels (&ctx, (const u8*)contour.data(), x_size, y_size, z_size);
    ogt::my_int_mesh_optimize (&ctx, ogt_mesh);
// TRACE();
    vector<VoxelVertex > verts (ogt_mesh->vertex_count);
    vector<PackedVoxelVertex> packed_verts (ogt_mesh->vertex_count);
    vector<PackedVoxelCircuit> circ_verts (ogt_mesh->vertex_count);
    for (u32 i = 0; i < ogt_mesh->vertex_count; i++) {
        // vec<3, unsigned char, defaultp> packed_posNorm = {};
        // packed_posNorm = uvec3(ogt_mesh->vertices[i].pos);
        // packed_posNorm.x |= (ogt_mesh->vertices[i].normal.x != 0)? (POS_NORM_BIT_MASK) : 0;
        // packed_posNorm.y |= (ogt_mesh->vertices[i].normal.y != 0)? (POS_NORM_BIT_MASK) : 0;
        // packed_posNorm.z |= (ogt_mesh->vertices[i].normal.z != 0)? (POS_NORM_BIT_MASK) : 0;
        verts[i].norm = ivec3 (ogt_mesh->vertices[i].normal);
        verts[i].pos = uvec3 (ogt_mesh->vertices[i].pos);
        // printl((int)verts[i].pos.x);
        verts[i].matID = (MatID_t)ogt_mesh->vertices[i].palette_index;
        assert (verts[i].norm != (vec<3, signed char, defaultp>) (0));
        assert (length (vec3 (verts[i].norm)) == 1.0f);
        assert (!any (greaterThanEqual (ogt_mesh->vertices[i].pos, uvec3 (255))));
        packed_verts[i].pos = uvec3 (ogt_mesh->vertices[i].pos);
        packed_verts[i].matID = (MatID_t)ogt_mesh->vertices[i].palette_index;
        circ_verts[i].pos = uvec3 (ogt_mesh->vertices[i].pos);
    }
    vector<u16> verts_Pzz = {};
    vector<u16> verts_Nzz = {};
    vector<u16> verts_zPz = {};
    vector<u16> verts_zNz = {};
    vector<u16> verts_zzP = {};
    vector<u16> verts_zzN = {};
    // printl(ogt_mesh->index_count);
    for (u32 i = 0; i < ogt_mesh->index_count; i++) {
        u32 index = ogt_mesh->indices[i];
        u32 provoking_index = ogt_mesh->indices[ (i / 3) * 3];
        vec<3, signed char, defaultp> norm = verts[provoking_index].norm;
        //where is my clang-tidy disable specific warning #pragma?..
        if (norm == vec<3, signed char, defaultp> (+1, 0, 0)) { verts_Pzz.push_back (index); }
        else if (norm == vec<3, signed char, defaultp> (-1, 0, 0)) { verts_Nzz.push_back (index); }
        else if (norm == vec<3, signed char, defaultp> (0, +1, 0)) { verts_zPz.push_back (index); }
        else if (norm == vec<3, signed char, defaultp> (0, -1, 0)) { verts_zNz.push_back (index); }
        else if (norm == vec<3, signed char, defaultp> (0, 0, +1)) { verts_zzP.push_back (index); }
        else if (norm == vec<3, signed char, defaultp> (0, 0, -1)) { verts_zzN.push_back (index); }
        else {
            DEBUG_LOG ((int)norm.x);
            DEBUG_LOG ((int)norm.y);
            DEBUG_LOG ((int)norm.z);
            crash (unrecognized normal);
        }
    }
// TRACE();
    assert (circ_verts.size() == ogt_mesh->vertex_count);
    mesh->triangles.vertexes = render.createElemBuffers<PackedVoxelCircuit> (circ_verts.data(), circ_verts.size(), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
// TRACE();
    assert (verts_Pzz.size() != 0);  
    assert (verts_Nzz.size() != 0); 
    assert (verts_zPz.size() != 0); 
    assert (verts_zNz.size() != 0); 
    assert (verts_zzP.size() != 0); 
    assert (verts_zzN.size() != 0); 
    mesh->triangles.Pzz.indexes = render.createElemBuffers<u16> (verts_Pzz.data(), verts_Pzz.size(), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
    mesh->triangles.Nzz.indexes = render.createElemBuffers<u16> (verts_Nzz.data(), verts_Nzz.size(), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
    mesh->triangles.zPz.indexes = render.createElemBuffers<u16> (verts_zPz.data(), verts_zPz.size(), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
    mesh->triangles.zNz.indexes = render.createElemBuffers<u16> (verts_zNz.data(), verts_zNz.size(), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
    mesh->triangles.zzP.indexes = render.createElemBuffers<u16> (verts_zzP.data(), verts_zzP.size(), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
    mesh->triangles.zzN.indexes = render.createElemBuffers<u16> (verts_zzN.data(), verts_zzN.size(), VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT);
// TRACE();
    mesh->triangles.Pzz.icount = verts_Pzz.size();
    mesh->triangles.Nzz.icount = verts_Nzz.size();
    mesh->triangles.zPz.icount = verts_zPz.size();
    mesh->triangles.zNz.icount = verts_zNz.size();
    mesh->triangles.zzP.icount = verts_zzP.size();
    mesh->triangles.zzN.icount = verts_zzN.size();
    mesh->shift = vec3 (0);
    mesh->rot = quat_identity<float, defaultp>();
    ogt::ogt_mesh_destroy (&ctx, (ogt::ogt_mesh*)ogt_mesh);
}
