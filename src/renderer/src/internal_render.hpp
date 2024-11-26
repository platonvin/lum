#pragma once

#include <vector>

#define RMLUI_STATIC_LIB
#include <RmlUi/Core.h>

#include <lum-al/src/al.hpp>
#include <glm/gtx/quaternion.hpp>

#include "internal_types.hpp"

namespace LumInternal {
// there are some conflicts in namespaces so have to do it like this
using std::vector;
using std::cout;
using std::tuple;
using std::tie;
// using std::set;
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

#define POS_NORM_BIT_MASK 0b10000000
typedef struct VoxelVertex {
    vec<3, unsigned char, defaultp> pos;
    vec<3, signed char, defaultp> norm;
    MatID_t matID;
} VoxelVertex;

typedef struct PackedVoxelVertex {
    vec<3, unsigned char, defaultp> pos;
    MatID_t matID;
} PackedVoxelVertex;

typedef struct PackedVoxelQuad {
    vec<2, unsigned char, defaultp> size;
    vec<3, unsigned char, defaultp> pos;
    MatID_t matID;
} PackedVoxelQuad;

typedef struct PackedVoxelCircuit {
    vec<3, unsigned char, defaultp> pos;
} PackedVoxelCircuit;

typedef struct IndexedVertices {
    u32 offset, icount;
} IndexedVertices;

// typedef struct NonIndexedVertices {
//     ring<Buffer> vertices;
//     u32 vcount;
// } NonIndexedVertices;

typedef struct FaceBuffers {
    IndexedVertices Pzz, Nzz, zPz, zNz, zzP, zzN;
    Lumal::Buffer vertexes;
    Lumal::Buffer indices;
} FaceBuffers;

typedef struct InternalMeshModel {
    FaceBuffers triangles;
    //staged per frame in flight, so you can update it faster. But costs double the memory
    //3d image of voxels in this mesh, used to represent mesh to per-frame world voxel representation
    ring<Lumal::Image> voxels; 
    ivec3 size; // integer because in voxels
} InternalMeshModel;

typedef struct InternalMeshFoliage {
    // foliage is defined as shader, not as vertices
    // this might sound stupid, but otherwise it needs like 100 parameters
    // and shader would still be faster and more extendable
    // limitation is that all foliage has to be init-time defined
    std::string vertex_shader_file = {};
    Lumal::RasterPipe pipe = {};
    // Staged per frame in flight, so you can update it faster. But costs double the memory
    int vertices;
    // ivec3 size; // integer because in voxels
    int dencity; // total dencity^2 foliage objects rendered as mesh
} InternalMeshFoliage;

typedef struct InternalMeshLiquid {
    MatID_t main;
    MatID_t foam;
} InternalMeshLiquid;

typedef struct InternalMeshVolumetric {
    float max_dencity;
    float variation;
    u8vec3 color;
} InternalMeshVolumetric;

typedef struct InternalUiMesh {
    Lumal::Buffer vertexes;
    Lumal::Buffer indexes;
    u32 icount;
    Lumal::Image* image;
} InternalUiMesh;

typedef struct Block {
    BlockVoxels voxels;
    InternalMeshModel mesh;
} BlockWithMesh; //in palette

//just 3d array. Content invalid after allocate()
template <typename Type> class table3d {
  private:
    Type* memory = NULL;
    ivec3 _size = {};
  public:
    //makes content invalid
    void allocate (int x, int y, int z) {
        _size = ivec3 (x, y, z);
        this->memory = (Type*)calloc (x* y* z, sizeof (Type));
    }
    void allocate (ivec3 size) {
        this->allocate (size.x, size.y, size.z);
    }
    void deallocate() {
        free (memory);
    }

    void set (Type val) {
        for (int x = 0; x < _size.x; x++) {
            for (int y = 0; y < _size.y; y++) {
                for (int z = 0; z < _size.z; z++) {
                    this->memory [x + _size.x * y + (_size.x* _size.y)*z] = val;
                }
            }
        }
    }

    //syntactic sugar
    Type* data() {return this->memory;}
    uvec3 size() {return _size;}

    Type& operator() (int x, int y, int z) const {
        return this->memory [x + _size.x * y + (_size.x* _size.y) * z];
    }
    Type& operator() (ivec3 v) const {return (*this) (v.x, v.y, v.z);}

};

//forward declaration for pointer
class MyRenderInterface;

// typedef struct LumSpecificSettings
typedef struct LumSettings : Lumal::Settings, LumSpecificSettings {
    // wait for it
    // LumSettings(LumSpecificSettings s) : LumSpecificSettings(s) {};
    // LumSettings() = default;
} LumSettings;

struct LumInternalRenderer {
    Lumal::Renderer lumal;
    
  public:
    void init (LumSettings settings);
    void setupFoliageDescriptors();
    void setupDescriptors();
    void createImages();
    VkResult createSwapchainDependent();
    void createSwapchainDependentImages();
    VkResult cleanupSwapchainDependent();
    void createFoliagePipilines();
    void createPipilines();
    void cleanup();
    void createSamplers();

    // sets voxels and size. By default uses first .vox palette as main palette
    void load_mesh (InternalMeshModel* mesh, const char* vox_file, bool _make_vertices = true, bool extrude_palette = true, Material* mat_palette = nullptr);
    // sets voxels and size. By default uses first .vox palette as main palette
    void load_mesh (InternalMeshModel* mesh, Voxel* voxel_mem, int x_size, int y_size, int z_size, bool _make_vertices = true);
    // safe to call in game/rendering loop frames
    void free_mesh (InternalMeshModel* mesh);
    void make_vertices (InternalMeshModel* mesh, Voxel* Voxels, int x_size, int y_size, int z_size);
    void load_vertices (InternalMeshModel* mesh, VoxelVertex* vertices);
    // void extrude_palette(Material* material_palette);
    void load_block (BlockWithMesh* block, const char* vox_file);
    // safe to call in game/rendering loop frames
    void free_block (BlockWithMesh* block);

    void load_scene (const char* scene_file);
    void save_scene (const char* scene_file);
    void extract_palette(const char* scene_file, Material* mat_palette);

    table3d<BlockID_t> origin_world = {};
    table3d<BlockID_t> current_world = {};
    MyRenderInterface* ui_render_interface;
    // float fratio;
    uvec3 world_size = uvec3(48, 48, 16);
    u32 static_block_palette_size = 15;
    int maxParticleCount = 8128;

    bool hasPalette = false;
    vector<VkImageCopy> blockCopyQueue = {};
    vector<VkImageSubresourceRange> blockclearQueue = {};
    double deltaTime = 0;

    bool resized = false;

    dvec3 lightDir = normalize (vec3 (0.5, 0.5, -0.9));
    dmat4 lightTransform = glm::identity<mat4>();
    void update_light_transform();

    Camera camera = {};

    void gen_perlin_2d();
    void gen_perlin_3d();
    void start_frame();
        void start_compute();
            void start_blockify();
                void blockify_mesh(InternalMeshModel* mesh, MeshTransform* model_trans);
            void end_blockify();
            void blockify_custom(void* ptr); // just in case you have custom blockify algorithm. If using this, no startBlockify needed
            void exec_copies();
            void start_map();
                void map_mesh(InternalMeshModel* mesh, MeshTransform* model_trans);
            void end_map();
            void update_radiance();
            void updade_grass(vec2 windDirection);
            void updade_water();
            void recalculate_df();
            void recalculate_bit();
        void end_compute();
        void start_lightmap();
            void lightmap_start_blocks();
                void lightmap_block(InternalMeshModel* block_mesh, int block_id, ivec3 shift); void lightmap_block_face(i8vec3 normal, IndexedVertices& buff, int block_id);
            void lightmap_start_models();
                void lightmap_model(InternalMeshModel* mesh, MeshTransform* model_trans); void lightmap_model_face(vec3 normal, IndexedVertices& buff);
        void end_lightmap(); //ends somewhere after raygen but operates on separate cmd buf
            // void start_lightmap();
            void start_raygen();
            void raygen_start_blocks();
                void raygen_block(InternalMeshModel* block_mesh, int block_id, ivec3 shift); void draw_block_face(i8vec3 normal, IndexedVertices& buff, int block_id);
            void raygen_start_models();
                void raygen_model(InternalMeshModel* mesh, MeshTransform* model_trans); void draw_model_face(vec3 normal, IndexedVertices& buff);
            void update_particles();
            void raygen_map_particles();
            void raygen_start_grass();
                void raygen_map_grass(InternalMeshFoliage* grass, ivec3 pos);
            // void raygen_end_grass();
            void raygen_start_water();
                void raygen_map_water(InternalMeshLiquid water, ivec3 pos);
            // void raygen_end_water();
        void end_raygen();
            // void end_raygen_first();
            void diffuse();
            void ambient_occlusion();
            void start_2nd_spass();
                void glossy_raygen();
                void raygen_start_smoke();// special mesh?
                void raygen_map_smoke(InternalMeshVolumetric smoke, ivec3 pos);// special mesh?
                void smoke();
                void glossy();
                void tonemap();
                void start_ui();
                    //ui updates
                void end_ui();
            void end_2nd_spass();
        void present();
    void end_frame();

    template<class Vertex_T> vector<Lumal::Buffer> createElemBuffers (Vertex_T* vertices, u32 count, u32 buffer_usage = 0);
    template<class Vertex_T> vector<Lumal::Buffer> createElemBuffers (vector<Vertex_T> vertices, u32 buffer_usage = 0);

    ring<Lumal::Image> create_RayTrace_VoxelImages (Voxel* voxels, ivec3 size);
    void updateBlockPalette (BlockWithMesh* blockPalette);
    void updateMaterialPalette (Material* materialPalette);

    // VkExtent2D swapChainExtent;
    // VkExtent2D raytraceExtent;
    VkExtent2D lightmapExtent;

    Lumal::RasterPipe lightmapBlocksPipe = {};
    Lumal::RasterPipe lightmapModelsPipe = {};

    Lumal::RasterPipe raygenBlocksPipe = {};
    Lumal::RasterPipe raygenModelsPipe = {};
    VkDescriptorSetLayout raygenModelsPushLayout;
    Lumal::RasterPipe raygenParticlesPipe = {};
    Lumal::RasterPipe raygenGrassPipe = {};
    Lumal::RasterPipe raygenWaterPipe = {};

    Lumal::RasterPipe diffusePipe = {};
    Lumal::RasterPipe aoPipe = {};
    Lumal::RasterPipe fillStencilGlossyPipe = {};
    Lumal::RasterPipe fillStencilSmokePipe = {};
    Lumal::RasterPipe glossyPipe = {};
    Lumal::RasterPipe smokePipe = {};
    Lumal::RasterPipe tonemapPipe = {};
    Lumal::RasterPipe overlayPipe = {};

    Lumal::RenderPass lightmapRpass;
    Lumal::RenderPass gbufferRpass;
    Lumal::RenderPass shadeRpass; //for no downscaling

    ring<VkCommandBuffer> computeCommandBuffers;
    ring<VkCommandBuffer> lightmapCommandBuffers;
    ring<VkCommandBuffer> graphicsCommandBuffers;
    ring<VkCommandBuffer> copyCommandBuffers; //runtime copies for ui. Also does first frame resources

    ring<Lumal::Image> lightmap;
    ring<Lumal::Image> swapchainImages;
    ring<Lumal::Image> highresFrame;
    ring<Lumal::Image> highresDepthStencil;
    // Image highresStencils;
    ring<Lumal::Image> highresMatNorm;
    //downscaled version for memory coherence. TODO:TEST perfomance on tiled
    // ring<Image> lowresMatNorm;
    // ring<Image> lowresDepthStencil;
    ring<VkImageView> stencilViewForDS;
    ring<Lumal::Image> farDepth; //represents how much should smoke traversal for
    ring<Lumal::Image> nearDepth; //represents how much should smoke traversal for
    ring<Lumal::Image> maskFrame; //where lowres renders to. Blends with highres afterwards

    VkSampler nearestSampler = VK_NULL_HANDLE;
    VkSampler linearSampler = VK_NULL_HANDLE;
    VkSampler linearSampler_tiled = VK_NULL_HANDLE;
    VkSampler linearSampler_tiled_mirrored = VK_NULL_HANDLE;
    VkSampler overlaySampler = VK_NULL_HANDLE;
    VkSampler shadowSampler = VK_NULL_HANDLE;
    VkSampler unnormLinear = VK_NULL_HANDLE;
    VkSampler unnormNearest = VK_NULL_HANDLE;

    //is or might be in use when cpu is recording new one. Is pretty cheap, so just leave it
    ring<Lumal::Buffer> stagingWorld;
    // ring<void*> stagingWorldMapped;
    ring<Lumal::Image> world; //can i really use just one?

    ring<Lumal::Image> radianceCache;

    ring<Lumal::Image> originBlockPalette;
    ring<Lumal::Image> distancePalette;
    ring<Lumal::Image> bitPalette; //bitmask of originBlockPalette
    ring<Lumal::Image> materialPalette;

    ring<Lumal::Buffer> lightUniform;
    ring<Lumal::Buffer> uniform;
    ring<Lumal::Buffer> aoLutUniform;
    vector<i8vec4> radianceUpdates;
    vector<i8vec4> specialRadianceUpdates;
    ring<Lumal::Buffer> gpuRadianceUpdates;
    // ring<void*> stagingRadianceUpdatesMapped;
    ring<Lumal::Buffer> stagingRadianceUpdates;

    vector<Particle> particles;
    ring<Lumal::Buffer> gpuParticles; //multiple because cpu-related work
    // ring<void* > gpuParticlesMapped; //multiple because cpu-related work

    ring<Lumal::Image> perlinNoise2d; //full-world grass shift (~direction) texture sampled in grass
    ring<Lumal::Image> grassState; //full-world grass shift (~direction) texture sampled in grass
    ring<Lumal::Image> waterState; //~same but water

    ring<Lumal::Image> perlinNoise3d; //4 channels of different tileable noise for volumetrics

    VkDescriptorPool descriptorPool;
    Lumal::ComputePipe raytracePipe = {};
    Lumal::ComputePipe radiancePipe = {};
    Lumal::ComputePipe mapPipe = {};
    VkDescriptorSetLayout mapPushLayout;
    Lumal::ComputePipe updateGrassPipe = {};
    Lumal::ComputePipe updateWaterPipe = {};
    Lumal::ComputePipe genPerlin2dPipe = {}; //generate noise for grass
    Lumal::ComputePipe genPerlin3dPipe = {}; //generate noise for grass
    Lumal::ComputePipe dfxPipe = {};
    Lumal::ComputePipe dfyPipe = {};
    Lumal::ComputePipe dfzPipe = {};
    Lumal::ComputePipe bitmaskPipe = {};

    vector<InternalMeshFoliage*> registered_foliage = {};

  private:
    int paletteCounter = 0;

    float timeTakenByRadiance = 0.0;
    int magicSize = 2;

  private:
    const VkFormat FRAME_FORMAT = VK_FORMAT_R16G16B16A16_UNORM;
    const VkFormat LIGHTMAPS_FORMAT = VK_FORMAT_D16_UNORM;
    VkFormat DEPTH_FORMAT = VK_FORMAT_UNDEFINED;
    const VkFormat DEPTH_FORMAT_PREFERED = VK_FORMAT_D24_UNORM_S8_UINT;
    const VkFormat DEPTH_FORMAT_SPARE = VK_FORMAT_D32_SFLOAT_S8_UINT; //TODO somehow faster than VK_FORMAT_D24_UNORM_S8_UINT on low-end

    const VkFormat MATNORM_FORMAT = VK_FORMAT_R8G8B8A8_UINT;
    const VkFormat RADIANCE_FORMAT = VK_FORMAT_A2B10G10R10_UNORM_PACK32;
    const VkFormat SECONDARY_DEPTH_FORMAT = VK_FORMAT_R16_SFLOAT;
};


// class MyRenderInterface : public Rml::RenderInterface {
class MyRenderInterface {
  public:
    MyRenderInterface() {}

    ~MyRenderInterface() {}; //override 
    // Called by RmlUi when it wants to render geometry that the application does not wish to optimise
    // void RenderGeometry (Rml::Vertex* vertices,
    //                      int num_vertices,
    //                      int* indices,
    //                      int num_indices,
    //                      Rml::TextureHandle texture,
    //                      const Rml::Vector2f& translation); //override;

    // // Called by RmlUi when it wants to compile geometry it believes will be static for the forseeable future.
    // Rml::CompiledGeometryHandle CompileGeometry (Rml::Vertex* vertices, int num_vertices, int* indices, int num_indices, Rml::TextureHandle texture); //override;

    // // Called by RmlUi when it wants to render application-compiled geometry.
    // void RenderCompiledGeometry (Rml::CompiledGeometryHandle geometry, const Rml::Vector2f& translation); //override;

    // // Called by RmlUi when it wants to release application-compiled geometry.
    // void ReleaseCompiledGeometry (Rml::CompiledGeometryHandle geometry); //override;

    // // Called by RmlUi when it wants to enable or disable scissoring to clip content.
    // void EnableScissorRegion (bool enable); //override;

    // // Called by RmlUi when it wants to change the scissor region.
    // void SetScissorRegion (int x, int y, int width, int height); //override;

    // // Called by RmlUi when a texture is required by the library.
    // bool LoadTexture (Rml::TextureHandle& texture_handle,
    //                   Rml::Vector2i& texture_dimensions,
    //                   const Rml::String& source); //override;

    // // Called by RmlUi when a texture is required to be built from an internally-generated sequence of pixels.
    // bool GenerateTexture (Rml::TextureHandle& texture_handle,
    //                       const Rml::byte* source,
    //                       const Rml::Vector2i& source_dimensions); //override;

    // // Called by RmlUi when a loaded texture is no longer required.
    // void ReleaseTexture (Rml::TextureHandle texture_handle); //override;

    // // Called by RmlUi when it wants the renderer to use a new transform matrix.
    // // If no transform applies to the current element, nullptr is submitted. Then it expects the renderer to use
    // // an glm::identity matrix or otherwise omit the multiplication with the transform.
    // void SetTransform (const Rml::Matrix4f* transform); //override;

    // Rml::Context* GetContext(); //override {};
    LumInternalRenderer* render;
    Lumal::Image* default_image = NULL;

    mat4 current_transform = glm::identity<mat4>();
    // bool has_scissors = true;
    VkRect2D last_scissors = {{0, 0}, {1, 1}};
};
}