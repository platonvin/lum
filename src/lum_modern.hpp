#pragma once
#ifndef __LUM_HPP__
#define __LUM_HPP__

#include "renderer/render.hpp"
#include "containers/arena.hpp"

// Lum is a lightweight wrapper around LumRenderer, with more stable and nicer API
// when properly compiled, it does not add any significant overhead

// even tho world is limited, it is not supposed to be the whole game world
// instead, treat it like "sliding window view" into your game world

// opaque ref-handlers that are supposed to be reused when possible (aka instancing):
typedef InternalMeshModel*      MeshModel; // non-world-grid-aligned voxel models
typedef BlockID_t               MeshBlock; // world-grid-aligned blocks of 16^3 voxels
typedef InternalMeshFoliage     MeshFoliage; // small non-voxel meshes, typically rendered in big groups. Mostly defined by vertex shader
typedef InternalMeshLiquid      MeshLiquid; // cascaded DFT for liquids like water
typedef InternalMeshVolumetric  MeshVolumetric; // Lambert law smoke
typedef InternalUiMesh*         MeshUi; // RmlUi interface thing

// queued requests to render different objects. They will be sorted and drawn
struct RenderRequest {
    float cam_dist{};
};

struct ModelRenderRequest : public RenderRequest {
    MeshModel mesh;
    MeshTransform trans;
};
struct BlockRenderRequest : public RenderRequest {
    MeshBlock block;
    ivec3 pos;
};
struct FoliageRenderRequest : public RenderRequest {
    MeshFoliage foliage;
    ivec3 pos;
};
struct LiquidRenderRequest : public RenderRequest {
    MeshLiquid liquid;
    ivec3 pos;
};
struct VolumetricRenderRequest : public RenderRequest {
    MeshVolumetric volumetric;
    ivec3 pos;
};

// yep its 13 kbytes
class Lum {
public:
    Lum(LumSettings settings) noexcept : Lum(settings, 4096, 64, 256) {}
    Lum(LumSettings settings, size_t mesh_storage_size, size_t foliage_storage_size, size_t block_palette_size) noexcept
        : mesh_models_storage(mesh_storage_size), mesh_foliage_storage(foliage_storage_size), block_palette(block_palette_size),
        curr_time(std::chrono::steady_clock::now()), settings(settings) 
        // {init();} // i do not like it being that impicit
        {}
    ~Lum() noexcept 
        // {cleanup();} // i do not like it being that impicit
        {}

    void init(LumSettings settings) noexcept;
    void loadWorld(const BlockID_t* world_data) noexcept;
    void loadWorld(const std::string& file) noexcept;

    void setWorldBlock(const ivec3& pos, MeshBlock block) noexcept;
    void setWorldBlock(int x, int y, int z, MeshBlock block) noexcept;

    [[nodiscard]] MeshBlock getWorldBlock(const ivec3& pos) const noexcept;
    [[nodiscard]] MeshBlock getWorldBlock(int x, int y, int z) const noexcept;

    void loadBlock(MeshBlock block, BlockWithMesh* block_data) noexcept;
    void loadBlock(MeshBlock block, const std::string& file) noexcept;

    void uploadBlockPaletteToGPU() noexcept;
    void uploadMaterialPaletteToGPU() noexcept;

    // loads palette no matter what
    void loadPalette(const std::string& file) noexcept;
    // loads palette if no palette loaded
    [[nodiscard]] MeshModel loadMesh(const std::string& file, bool try_extract_palette = true) noexcept;
    [[nodiscard]] MeshModel loadMesh(Voxel* mesh_data, int x_size, int y_size, int z_size) noexcept;
    // Only mesh can be freed
    void freeMesh(MeshModel& model) noexcept;

    [[nodiscard]] MeshFoliage loadFoliage(const std::string& vertex_shader_file, int vertices_per_foliage, int density) noexcept;
    [[nodiscard]] MeshVolumetric loadVolumetric(float max_density, float density_deviation, const u8vec3& color) noexcept;
    [[nodiscard]] MeshLiquid loadLiquid(MatID_t main_mat, MatID_t foam_mat) noexcept;

    void startFrame() noexcept;
        void drawWorld() noexcept;
        void drawParticles() noexcept;
        void drawMesh(const MeshModel& mesh, const MeshTransform& trans) noexcept;
        void drawFoliageBlock(const MeshFoliage& foliage, const ivec3& pos) noexcept;
        void drawLiquidBlock(const MeshLiquid& liquid, const ivec3& pos) noexcept;
        void drawVolumetricBlock(const MeshVolumetric& volumetric, const ivec3& pos) noexcept;
    void prepareFrame() noexcept;
    void submitFrame() noexcept;

    // "un init" function
    void cleanup() noexcept;
    [[nodiscard]] GLFWwindow* getGLFWptr() const noexcept;
    [[nodiscard]] bool shouldClose() const noexcept { return should_close; }
    // waits until all frames in flight are executed on gpu
    void waitIdle() noexcept;

public:
    bool should_close{false};
    LumRenderer render;
    LumSettings settings; // duplicated?
    std::array<Material, MATERIAL_PALETTE_SIZE> mat_palette;
    std::vector<BlockWithMesh> block_palette;
    // Arenas are not only faster to allocate and more coherent memory, but also auto-free is possible
    Arena<InternalMeshModel> mesh_models_storage;
    Arena<InternalMeshFoliage> mesh_foliage_storage;

    std::vector<BlockRenderRequest> block_que;
    std::vector<ModelRenderRequest> mesh_que;
    std::vector<FoliageRenderRequest> foliage_que;
    std::vector<LiquidRenderRequest> liquid_que;
    std::vector<VolumetricRenderRequest> volumetric_que;

    std::chrono::steady_clock::time_point curr_time;
    double delta_time{0};

    void updateTime() noexcept {
        auto now = std::chrono::steady_clock::now();
        delta_time = std::chrono::duration<double>(now - curr_time).count();
        curr_time = now;
    }
};

#endif // __LUM_HPP__