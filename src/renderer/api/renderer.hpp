#pragma once
#ifndef __LUM_HPP__
#define __LUM_HPP__

/*
C++ API for Lum
*/

#include "../src/internal_render.hpp"
#include "../../containers/arena.hpp"

namespace Lum {
// opaque ref-handlers that are supposed to be reused when possible (aka instancing):
typedef LumInternal::InternalMeshModel*      MeshModel; // non-world-grid-aligned voxel models
typedef LumInternal::BlockID_t               MeshBlock; // world-grid-aligned blocks of 16^3 voxels
typedef LumInternal::InternalMeshFoliage*    MeshFoliage; // small non-voxel meshes, typically rendered in big groups. Mostly defined by vertex shader
typedef LumInternal::InternalMeshLiquid*     MeshLiquid; // cascaded DFT for liquids like water
typedef LumInternal::InternalMeshVolumetric* MeshVolumetric; // Lambert law smoke
typedef LumInternal::InternalUiMesh*         MeshUi; // RmlUi interface thing
typedef LumInternal::MeshTransform           MeshTransform; // namespace typedef 
typedef LumInternal::Particle                Particle; // namespace typedef 
typedef LumInternal::LumSettings             Settings; // namespace typedef 

/*
    Lum::Renderer is a lightweight wrapper around LumInternalRenderer, with more stable and nicer API
    when properly compiled, it does not add any significant overhead

    even tho world is limited, it is not supposed to be the whole game world
    instead, treat it like "sliding window view" into your game world
*/ // yep its 13 kbytes
class Renderer {
public:
    Renderer(LumInternal::LumSettings settings) noexcept : Renderer(settings, 1024, 1024, 64, 64, 64) {}
    Renderer(LumInternal::LumSettings settings, 
            size_t block_palette_size, 
            size_t mesh_storage_size, 
            size_t foliage_storage_size, 
            size_t volumetric_storage_size, 
            size_t liquid_storage_size) noexcept : 
        block_palette(block_palette_size),
        mesh_models_storage(mesh_storage_size), 
        mesh_foliage_storage(foliage_storage_size), 
        mesh_volumetric_storage(volumetric_storage_size), 
        mesh_liquid_storage(liquid_storage_size), 
        curr_time(std::chrono::steady_clock::now()), settings(settings) 
        // {init();} // i do not like it being that impicit
        {}
    ~Renderer() noexcept 
        // {cleanup();} // i do not like it being that impicit
        {}

    void init(LumInternal::LumSettings settings) noexcept;
    void loadWorld(const LumInternal::BlockID_t* world_data) noexcept;
    void loadWorld(const std::string& file) noexcept;

    void setWorldBlock(const ivec3& pos, MeshBlock block) noexcept;
    void setWorldBlock(int x, int y, int z, MeshBlock block) noexcept;

    [[nodiscard]] MeshBlock getWorldBlock(const ivec3& pos) const noexcept;
    [[nodiscard]] MeshBlock getWorldBlock(int x, int y, int z) const noexcept;

    void loadBlock(MeshBlock block, LumInternal::BlockWithMesh* block_data) noexcept;
    void loadBlock(MeshBlock block, const std::string& file) noexcept;

    void uploadBlockPaletteToGPU() noexcept;
    void uploadMaterialPaletteToGPU() noexcept;

    // loads palette no matter what
    void loadPalette(const std::string& file) noexcept;
    // loads palette if no palette loaded
    [[nodiscard]] MeshModel loadMesh(const std::string& file, bool try_extract_palette = true) noexcept;
    [[nodiscard]] MeshModel loadMesh(LumInternal::Voxel* mesh_data, int x_size, int y_size, int z_size) noexcept;
    // Only mesh can be freed
    void freeMesh(MeshModel model) noexcept;

    [[nodiscard]] MeshFoliage loadFoliage(const std::string& vertex_shader_file, int vertices_per_foliage, int density) noexcept;
    [[nodiscard]] MeshVolumetric loadVolumetric(float max_density, float density_deviation, const u8vec3& color) noexcept;
    [[nodiscard]] MeshLiquid loadLiquid(LumInternal::MatID_t main_mat, LumInternal::MatID_t foam_mat) noexcept;

    void startFrame() noexcept;
        void drawWorld() noexcept;
        void drawParticles() noexcept;
        void drawModel(const MeshModel& mesh, const MeshTransform& trans) noexcept;
        void drawFoliageBlock(const MeshFoliage& foliage, const ivec3& pos) noexcept;
        void drawLiquidBlock(const MeshLiquid& liquid, const ivec3& pos) noexcept;
        void drawVolumetricBlock(const MeshVolumetric& volumetric, const ivec3& pos) noexcept;
    void prepareFrame() noexcept;
    void submitFrame() noexcept;

    // "un init" function
    void cleanup() noexcept;
    // waits until all frames in flight are executed on gpu
    void waitIdle() noexcept;

    //why setters and getters? For C99 API coherence
    [[nodiscard]] GLFWwindow* getGLFWptr() const noexcept;
    [[nodiscard]] bool getShouldClose() const noexcept { return should_close; }
                  bool setShouldClose(bool sc) noexcept { should_close = sc; return should_close;}

    public:
        bool should_close{false};
        LumInternal::LumInternalRenderer render;
        LumInternal::LumSettings settings; // duplicated?
        std::array<LumInternal::Material, LumInternal::MATERIAL_PALETTE_SIZE> mat_palette;
        std::vector<LumInternal::BlockWithMesh> block_palette;
        // Arenas are not only faster to allocate and more coherent memory, but also auto-free is possible
        Arena<LumInternal::InternalMeshModel> mesh_models_storage;
        Arena<LumInternal::InternalMeshFoliage> mesh_foliage_storage;
        Arena<LumInternal::InternalMeshVolumetric> mesh_volumetric_storage;
        Arena<LumInternal::InternalMeshLiquid> mesh_liquid_storage;

    // queued requests to render different objects. They will be sorted by depth
    struct RenderRequest { float cam_dist {}; };
    struct ModelRenderRequest : RenderRequest {
        MeshModel mesh {};
        MeshTransform trans {};
    };
    struct BlockyRenderRequest     :       RenderRequest { ivec3 pos {}; };
    struct BlockRenderRequest      : BlockyRenderRequest { MeshBlock block {}; };
    struct FoliageRenderRequest    : BlockyRenderRequest { MeshFoliage foliage {}; };
    struct LiquidRenderRequest     : BlockyRenderRequest { MeshLiquid liquid {}; };
    struct VolumetricRenderRequest : BlockyRenderRequest { MeshVolumetric volumetric {}; };

    std::vector<BlockRenderRequest> block_que;
    std::vector<ModelRenderRequest> mesh_que;
    std::vector<FoliageRenderRequest> foliage_que;
    std::vector<LiquidRenderRequest> liquid_que;
    std::vector<VolumetricRenderRequest> volumetric_que;

    // very simple CPU timer
    std::chrono::steady_clock::time_point curr_time;
    double delta_time{0};
    void updateTime() noexcept {
        auto now = std::chrono::steady_clock::now();
        delta_time = std::chrono::duration<double>(now - curr_time).count();
        curr_time = now;
    }
};
// }

#endif // __LUM_HPP__

}