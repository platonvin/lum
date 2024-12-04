#pragma once
#ifndef __LUM_RENDERER_HPP__
#define __LUM_RENDERER_HPP__

/*
C++ API for Lum
*/

// types in 
#include "../src/internal_types.hpp"
// #include <glm/ext/vector_int3.hpp>
// #include <glm/ext/vector_float4.hpp>
// #include "../../containers/promised_storage.hpp"
// #include "../../containers/size_predictor.hpp"
// #include "../../containers/arena.hpp"
#include <chrono>
#include <string>
#include <vector>

namespace Lum {

// fwd decl
class OpaqueRendererMembers;
    
// wrapper to have getter without actual declaration    
class MeshModelWithGetters {
public:
    void* ptr = nullptr;
    MeshModelWithGetters(void* ptr) : ptr(ptr) {} 
    MeshModelWithGetters() : ptr(nullptr) {} 
    glm::ivec3 getSize();
    operator void* ();
};

typedef MeshModelWithGetters MeshModel; // non-world-grid-aligned voxel models
typedef int16_t              MeshBlock; // world-grid-aligned blocks of 16^3 voxels
typedef void*                MeshFoliage; // small non-voxel meshes, typically rendered in big groups. Mostly defined by vertex shader
typedef void*                MeshLiquid; // cascaded DFT for liquids like water
typedef void*                MeshVolumetric; // Lambert law smoke
typedef void*                MeshUi; // RmlUi interface thing
typedef LumInternal::MeshTransform MeshTransform; // namespace typedef 
typedef LumInternal::Particle      Particle; // namespace typedef 
typedef LumInternal::Camera        Camera; // namespace typedef 
struct Settings {
    glm::vec3 world_size;
    int static_block_palette_size;
    int maxParticleCount;
    int timestampCount = 48;
    int fif = 2;
    bool vsync = false;
    bool fullscreen = false;
    bool debug = false;
    bool profile = true;
}; 

/*
    Lum::Renderer is a lightweight wrapper around LumInternalRenderer, with more stable and nicer API
    when properly compiled, it does not add any significant overhead

    even tho world is limited, it is not supposed to be the whole game world
    instead, treat it like "sliding window view" into your game world
*/
struct Renderer {
    Renderer() noexcept;
    Renderer(
            size_t block_palette_size, 
            size_t mesh_storage_size, 
            size_t foliage_storage_size, 
            size_t volumetric_storage_size, 
            size_t liquid_storage_size) noexcept ;
    ~Renderer() noexcept ;

    void init(Lum::Settings settings) noexcept;
    void loadWorld(const LumInternal::BlockID_t* world_data) noexcept;
    void loadWorld(const std::string& file) noexcept;

    void setWorldBlock(const glm::ivec3& pos, MeshBlock block) noexcept;
    void setWorldBlock(int x, int y, int z, MeshBlock block) noexcept;

    [[nodiscard]] MeshBlock getWorldBlock(const glm::ivec3& pos) const noexcept;
    [[nodiscard]] MeshBlock getWorldBlock(int x, int y, int z) const noexcept;

    void loadBlock(MeshBlock block, LumInternal::BlockVoxels* block_data) noexcept;
    void loadBlock(MeshBlock block, const std::string& file) noexcept;

    void uploadBlockPaletteToGPU() noexcept;
    void uploadMaterialPaletteToGPU() noexcept;

    // loads palette no matter what
    void loadPalette(const std::string& file) noexcept;
    // loads palette if no palette loaded
    [[nodiscard]] MeshModel loadModel(const std::string& file, bool try_extract_palette = true) noexcept;
    [[nodiscard]] MeshModel loadModel(LumInternal::Voxel* mesh_data, int x_size, int y_size, int z_size) noexcept;
    // Only mesh can be freed
    void freeMesh(MeshModel model) noexcept;

    [[nodiscard]] MeshFoliage loadFoliage(const std::string& vertex_shader_file, int vertices_per_foliage, int density) noexcept;
    [[nodiscard]] MeshVolumetric loadVolumetric(float max_density, float density_deviation, const glm::u8vec3& color) noexcept;
    [[nodiscard]] MeshLiquid loadLiquid(LumInternal::MatID_t main_mat, LumInternal::MatID_t foam_mat) noexcept;

    void startFrame() noexcept;
        void drawWorld() noexcept;
        void drawParticles() noexcept;
        void drawModel(const MeshModel& mesh, const MeshTransform& trans) noexcept;
        void drawFoliageBlock(const MeshFoliage& foliage, const glm::ivec3& pos) noexcept;
        void drawLiquidBlock(const MeshLiquid& liquid, const glm::ivec3& pos) noexcept;
        void drawVolumetricBlock(const MeshVolumetric& volumetric, const glm::ivec3& pos) noexcept;
    void prepareFrame() noexcept;
    void submitFrame() noexcept;
    void addParticle(Particle particle);

    // "un init" function
    void cleanup() noexcept;
    // waits until all frames in flight are executed on gpu
    void waitIdle() noexcept;

    // why setters and getters? For C99 API coherence
    // cast yourself. Made this way to avoid including glfw
    [[nodiscard]] void* getGLFWptr() const noexcept;
    [[nodiscard]] bool getShouldClose() const noexcept { return should_close; }
                  bool setShouldClose(bool sc) noexcept { should_close = sc; return should_close;}
    Camera& getCamera();
    glm::ivec3 getWorldSize();

    public:
        bool should_close{false};
        // This could be 80k lines of headers just to declare private members
        // but it's really not needed (and my LSP dies).
        OpaqueRendererMembers* opaque_members = nullptr;

    // queued requests to render different objects. They will be sorted by depth
    struct RenderRequest { float cam_dist; };
    struct ModelRenderRequest : RenderRequest {
        MeshModel mesh;
        MeshTransform trans;
    };
    struct BlockyRenderRequest     :       RenderRequest { glm::ivec3 pos; };
    struct BlockRenderRequest      : BlockyRenderRequest { MeshBlock block; };
    struct FoliageRenderRequest    : BlockyRenderRequest { MeshFoliage foliage; };
    struct LiquidRenderRequest     : BlockyRenderRequest { MeshLiquid liquid; };
    struct VolumetricRenderRequest : BlockyRenderRequest { MeshVolumetric volumetric; };

    std::vector<BlockRenderRequest> block_que = {};
    std::vector<ModelRenderRequest> mesh_que = {};
    std::vector<FoliageRenderRequest> foliage_que = {};
    std::vector<LiquidRenderRequest> liquid_que = {};
    std::vector<VolumetricRenderRequest> volumetric_que = {};

    // very simple CPU timer
    std::chrono::steady_clock::time_point curr_time;
    double delta_time{0};
    void updateTime() noexcept;
};

} // namespace Lum

#endif // __LUM_RENDERER_HPP__