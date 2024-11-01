#include "render.hpp"
#include "defines/macros.hpp"
#include <glm/gtx/string_cast.hpp>
#include <glm/gtx/quaternion.hpp>
#include <fstream>

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

tuple<int, int> get_block_xy (int N);

#ifdef DISTANCE_FIELD
struct bp_tex {u8 r; u8 g;};

#else
struct bp_tex {u8 r;};

#endif
//block palette on cpu side is expected to be array of pointers to blocks
void LumInternal::LumInternalRenderer::updateBlockPalette (BlockWithMesh* blockPalette) {
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
                    // if (blockPalette[N] == NULL) {
                    //     blockPaletteLinear (x + 16 * block_x, y + 16 * block_y, z).r = 0;
                    // } else 
                    {
                        if (N < static_block_palette_size) {
                            blockPaletteLinear (x + 16 * block_x, y + 16 * block_y, z).r = (u16) blockPalette[N].voxels[x][y][z];
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
void LumInternal::LumInternalRenderer::updateMaterialPalette (Material* materialPalette) {
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
// std::string find_shader(const std::string& shader_name) {
//     std::string paths[] = {
//         "../shaders/compiled/" + shader_name,
//         "../../shaders/compiled/" + shader_name
//     };

//     for (const auto& path : paths) {
//         std::ifstream file(path);
//         if (file.good()) {
//             return path;
//         }
//     }

//     throw std::runtime_error("Shader file not found: " + shader_name);
// }

// try to find shader in multiple places just in case if lum is launched incorrectly
std::string find_asset(const std::string& asset_name) {
    std::string paths[] = {
        asset_name,
        "../" + asset_name // when in bin/
    };

    for (const auto& path : paths) {
        std::ifstream file(path);
        if (file.good()) {
            return path;
        }
    }

    assert(false && ("Asset file not found: " + asset_name).c_str());
    std::unreachable();
}

std::vector<char> readFileBuffer(const std::string& asset_name) {
    std::string path = find_asset(asset_name);
    
    std::ifstream file(path, std::ios::binary | std::ios::ate);
    if (!file) {
        return {};
    }

    std::streamsize size = file.tellg();
    file.seekg(0, std::ios::beg);

    std::vector<char> buffer(size);
    if (!file.read(buffer.data(), size)) {
        return {};
    }

    return buffer;
}

void updateFileBuffer(const std::string& asset_name, const std::vector<char>& content) {
    std::string path = find_asset(asset_name);

    std::ofstream file(path, std::ios::binary | std::ios::trunc);
    if (!file) {
        crash(Failed to open file for writing);
    }

    if (!file.write(content.data(), content.size())) {
        crash(Failed to write content to file);
    }
}

// namespace ogt { //this library for some reason uses ogt_ in cases when it will never intersect with others BUT DOES NOT WHEN IT FOR SURE WILL
#include <ogt_vox.hpp>
#include <ogt_voxel_meshify.hpp>

void LumInternal::LumInternalRenderer::load_scene (const char* vox_file) {
    auto buffer = readFileBuffer(vox_file);

    if (buffer.size() < sizeof(ivec3)) {
        origin_world.set(0);
        return;
    }

    ivec3* header = (ivec3*)buffer.data();
    ivec3 stored_world_size = *header;
    BlockID_t* stored_world = (BlockID_t*)(buffer.data() + sizeof(ivec3));

    assert((char*)stored_world < buffer.data() + buffer.size());
    
    DEBUG_LOG(world_size.x);
    DEBUG_LOG(world_size.y);
    DEBUG_LOG(world_size.z);
    DEBUG_LOG(static_block_palette_size);

    ivec3 size2read = glm::clamp(stored_world_size, ivec3(0), ivec3(world_size));

    for (int xx = 0; xx < size2read.x; xx++) {
        for (int yy = 0; yy < size2read.y; yy++) {
            for (int zz = 0; zz < size2read.z; zz++) {
                size_t index = xx + stored_world_size.x * yy + (stored_world_size.x * stored_world_size.y) * zz;
                assert(index < (buffer.size() - sizeof(ivec3)) / sizeof(BlockID_t));
                
                BlockID_t loaded_block = stored_world[index];
                origin_world(xx, yy, zz) = glm::clamp(loaded_block, BlockID_t(0), BlockID_t(static_block_palette_size));
            }
        }
    }

    return;
}

void LumInternal::LumInternalRenderer::save_scene (const char* vox_file) {
    assert(world_size.x > 0 && world_size.y > 0 && world_size.z > 0);
    
    std::vector<char> content;
    size_t buffer_size = sizeof(ivec3) + (world_size.x * world_size.y * world_size.z * sizeof(BlockID_t));
    content.resize(buffer_size);
    std::memcpy(content.data(), &world_size, sizeof(ivec3));
    std::memcpy(content.data() + sizeof(ivec3), origin_world.data(), world_size.x * world_size.y * world_size.z * sizeof(BlockID_t));

    updateFileBuffer(vox_file, content);
}

static void extract_palette_mem(const ogt::vox_scene* scene, LumInternal::Material* mat_palette) {
    assert(mat_palette != NULL);
    for (int i = 0; i < LumInternal::MATERIAL_PALETTE_SIZE; i++) {
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
    /*
        roughness
        IOR
        Specular
        Metallic

        Emmission
        Power - radiant flux
        Ldr
    */
}

void LumInternal::LumInternalRenderer::extract_palette(const char* scene_file, Material* mat_palette) {
    auto buffer = readFileBuffer(scene_file);
    assert(not buffer.empty());

    const ogt::vox_scene* scene = ogt::vox_read_scene((u8*)buffer.data(), buffer.size());
    assert(scene != NULL);
    assert(scene->num_models > 0);

    hasPalette = true;
    extract_palette_mem(scene, mat_palette);
    
    ogt::vox_destroy_scene(scene);
}

//size limited by 256^3
void LumInternal::LumInternalRenderer::load_mesh(InternalMeshModel* mesh, const char* vox_file, bool _make_vertices, bool extrude_palette, Material* mat_palette) {
    auto buffer = readFileBuffer(vox_file);
    assert(not buffer.empty());
    const ogt::vox_scene* scene = ogt::vox_read_scene((u8*)buffer.data(), buffer.size());
 
    assert(scene != NULL);
    assert(scene->num_models == 1);
    
    load_mesh(mesh, (Voxel*)scene->models[0]->voxel_data, scene->models[0]->size_x, scene->models[0]->size_y, scene->models[0]->size_z, _make_vertices);
    
    if (extrude_palette && !hasPalette) {
        assert(mat_palette != NULL);
        hasPalette = true;
        extract_palette_mem(scene, mat_palette);
    }
    
    ogt::vox_destroy_scene(scene);
}

// pointer to block_ptr, so block_ptr can be modified 
void LumInternal::LumInternalRenderer::load_block(BlockWithMesh* block, const char* vox_file) {
    auto buffer = readFileBuffer(vox_file);
    assert(not buffer.empty());
    const ogt::vox_scene* scene = ogt::vox_read_scene((u8*)buffer.data(), buffer.size());

    assert(scene != NULL);
    assert(scene->num_models == 1);
    // if ((block) == NULL) {
    //     *block = new Block;
    TRACE()
    // }
    (block)->mesh.size = ivec3(scene->models[0]->size_x, scene->models[0]->size_y, scene->models[0]->size_z);
    TRACE()
    assert(scene->models[0]->size_x > 0 && scene->models[0]->size_y > 0 && scene->models[0]->size_z > 0);
    TRACE()
    make_vertices(&((block)->mesh), (Voxel*)scene->models[0]->voxel_data, scene->models[0]->size_x, scene->models[0]->size_y, scene->models[0]->size_z);
    TRACE()
    for (int x = 0; x < scene->models[0]->size_x; x++) {
        for (int y = 0; y < scene->models[0]->size_y; y++) {
            for (int z = 0; z < scene->models[0]->size_z; z++) {
                (block)->voxels[x][y][z] = (u16)scene->models[0]->voxel_data[x + y * scene->models[0]->size_x + z * scene->models[0]->size_x * scene->models[0]->size_y];
            }
        }
    }
    TRACE()
    ogt::vox_destroy_scene(scene);
}

// #define free_helper(dir) \
// if(not (block)->mesh.triangles.dir.indexes.empty()) vmaDestroyBuffer(render.VMAllocator, (block)->mesh.triangles.dir.indexes[i].buffer, (block)->mesh.triangles.dir.indexes[i].alloc);
#define free_helper(__dir) \
if(block->mesh.triangles.__dir.indexes.buffer) {\
    BufferDeletion bd = {};\
        bd.buffer = block->mesh.triangles.__dir.indexes;\
        bd.life_counter = render.settings.fif; /* delete after fif+1 frames*/\
    render.bufferDeletionQueue.push_back(bd);\
    block->mesh.triangles.__dir.indexes.buffer = VK_NULL_HANDLE;\
}

void LumInternal::LumInternalRenderer::free_block(BlockWithMesh* block) {
    assert(block != NULL);
    // assert(*block != NULL);
    
    // free_helper(Pzz);
    // free_helper(Nzz);
    // free_helper(zPz);
    // free_helper(zNz);
    // free_helper(zzP);
    // free_helper(zzN);
    if(block->mesh.triangles.indices.buffer) {\
        Lumal::BufferDeletion bd = {};\
            bd.buffer = block->mesh.triangles.indices;\
            bd.life_counter = render.settings.fif; /* delete after fif+1 frames*/\
        render.bufferDeletionQueue.push_back(bd);\
        block->mesh.triangles.indices.buffer = VK_NULL_HANDLE;\
    }
    if((block)->mesh.triangles.vertexes.buffer){
        vmaDestroyBuffer(render.VMAllocator, (block)->mesh.triangles.vertexes.buffer, (block)->mesh.triangles.vertexes.alloc);
        (block)->mesh.triangles.vertexes.buffer = VK_NULL_HANDLE;
    }
    // for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        // if (!(block)->mesh.triangles.vertexes.empty()) {
        // }
    // }
    // delete *block;
    // *block = NULL;
}

void LumInternal::LumInternalRenderer::load_mesh(InternalMeshModel* mesh, Voxel* Voxels, int x_size, int y_size, int z_size, bool _make_vertices) {
    assert(mesh != NULL);
    assert(Voxels != NULL);
    assert(x_size > 0 && y_size > 0 && z_size > 0);
    mesh->size = ivec3(x_size, y_size, z_size);
    if (_make_vertices) {
        make_vertices(mesh, Voxels, x_size, y_size, z_size);
    }
    mesh->voxels = create_RayTrace_VoxelImages(Voxels, mesh->size);
}

//frees only gpu side stuff, not mesh ptr
#undef  free_helper
#define free_helper(__dir) \
if(mesh->triangles.__dir.indexes.buffer) {\
    BufferDeletion bd = {};\
        bd.buffer = mesh->triangles.__dir.indexes;\
        bd.life_counter = render.settings.fif; /* delete after fif+1 frames*/\
    render.bufferDeletionQueue.push_back(bd);\
}

void LumInternal::LumInternalRenderer::free_mesh(InternalMeshModel* mesh) {
    assert(mesh != NULL);
    // free_helper(Pzz);
    // free_helper(Nzz);
    // free_helper(zPz);
    // free_helper(zNz);
    // free_helper(zzP);
    // free_helper(zzN);
    if(mesh->triangles.indices.buffer) {\
        Lumal::BufferDeletion bd = {};\
            bd.buffer = mesh->triangles.indices;\
            bd.life_counter = render.settings.fif; /* delete after fif+1 frames*/\
        render.bufferDeletionQueue.push_back(bd);\
    }
    Lumal::BufferDeletion
        bd = {};
        bd.buffer = mesh->triangles.vertexes;
        bd.life_counter = render.settings.fif; // delete after fif+1 frames
    render.bufferDeletionQueue.push_back(bd);
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        // if (!mesh->triangles.vertexes.empty()) {
            // vmaDestroyBuffer(render.VMAllocator, mesh->triangles.vertexes[i].buffer, mesh->triangles.vertexes[i].alloc);
        // }
        if (!mesh->voxels.empty()) {
            // vmaDestroyImage(render.VMAllocator, mesh->voxels[i].image, mesh->voxels[i].alloc);
            // vkDestroyImageView(render.device, mesh->voxels[i].view, NULL);
            Lumal::ImageDeletion
                id = {};
                id.image = mesh->voxels[i];
                id.life_counter = render.settings.fif; // delete after fif+1 frames
            render.imageDeletionQueue.push_back(id);
        }
    }
}
#undef free_helper

bool operator== (const LumInternal::PackedVoxelVertex& one, const LumInternal::PackedVoxelVertex& other) {
    return
        (one.pos.x == other.pos.x) &&
        (one.pos.y == other.pos.y) &&
        (one.pos.z == other.pos.z)
        ;
}

bool operator!= (const LumInternal::PackedVoxelVertex& one, const LumInternal::PackedVoxelVertex& other) {
    return ! (one == other);
}

bool operator< (const LumInternal::PackedVoxelVertex& one, const LumInternal::PackedVoxelVertex& other) {
    return one != other;
}

LumInternal::PackedVoxelQuad pack_quad (const array<LumInternal::PackedVoxelVertex, 6> vertices, uvec3 norm) {
    LumInternal::PackedVoxelQuad quad = {};
    vector<LumInternal::PackedVoxelVertex> uniq;
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

void repack_helper (vector<LumInternal::PackedVoxelVertex>& vs, vector<LumInternal::PackedVoxelQuad>& qs, uvec3 norm) {
    assert ((vs.size() % 6) == 0);
    for (int i = 0; i < vs.size() / 6; i++) {
        qs.push_back (pack_quad ({
            vs[i + 0], vs[i + 1], vs[i + 2],
            vs[i + 3], vs[i + 4], vs[i + 5],
        }, norm));
    }
}

//defenetly not an example of highly optimized code
void LumInternal::LumInternalRenderer::make_vertices (InternalMeshModel* mesh, Voxel* Voxels, int x_size, int y_size, int z_size) {
    ogt::ogt_voxel_meshify_context ctx = {};

    const int totalVoxels = x_size * y_size * z_size;
    std::vector<Voxel> contour(totalVoxels);
    for (int i = 0; i < totalVoxels; ++i) {
        contour[i] = (Voxels[i] == 0) ? 0 : 1;
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

    auto classify_normal = [&](const vec3& norm) {
        if (norm == vec3(1, 0, 0)) return &verts_Pzz;
        if (norm == vec3(-1, 0, 0)) return &verts_Nzz;
        if (norm == vec3(0, 1, 0)) return &verts_zPz;
        if (norm == vec3(0, -1, 0)) return &verts_zNz;
        if (norm == vec3(0, 0, 1)) return &verts_zzP;
        if (norm == vec3(0, 0, -1)) return &verts_zzN;
        return (std::vector<u16>*)(nullptr); // error
    };

    for (u32 i = 0; i < ogt_mesh->index_count; ++i) {
        u32 index = ogt_mesh->indices[i];
        u32 provoking_index = ogt_mesh->indices[(i / 3) * 3];
        const auto& norm = ogt_mesh->vertices[provoking_index].normal;

        auto* target_vector = classify_normal(norm);
        if (target_vector) target_vector->push_back(index);
        else crash("Unrecognized normal");
    }
// TRACE();
    assert (circ_verts.size() == ogt_mesh->vertex_count);
    // mesh->triangles.vertexes = render.createElemBuffer<PackedVoxelCircuit> (circ_verts.data(), circ_verts.size(), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);
// TRACE();
    assert (verts_Pzz.size() != 0);  
    assert (verts_Nzz.size() != 0); 
    assert (verts_zPz.size() != 0); 
    assert (verts_zNz.size() != 0); 
    assert (verts_zzP.size() != 0); 
    assert (verts_zzN.size() != 0);
    
    std::vector<u16> all_indices;
    auto offset_and_insert = [&all_indices](auto& vector, IndexedVertices& section) {
        section.offset = all_indices.size();
        all_indices.insert(all_indices.end(), vector.begin(), vector.end());
        section.icount = vector.size();
    };

    offset_and_insert(verts_Pzz, mesh->triangles.Pzz);
    offset_and_insert(verts_Nzz, mesh->triangles.Nzz);
    offset_and_insert(verts_zPz, mesh->triangles.zPz);
    offset_and_insert(verts_zNz, mesh->triangles.zNz);
    offset_and_insert(verts_zzP, mesh->triangles.zzP);
    offset_and_insert(verts_zzN, mesh->triangles.zzN);

    mesh->triangles.vertexes = render.createElemBuffer<PackedVoxelCircuit>(
        circ_verts.data(), 
        circ_verts.size(),
        VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT);

    mesh->triangles.indices = render.createElemBuffer<u16>(
        all_indices.data(), 
        all_indices.size(),
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT);

// TRACE();
    ogt::ogt_mesh_destroy (&ctx, (ogt::ogt_mesh*)ogt_mesh);
}
