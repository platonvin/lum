#pragma once

// #include <optional>
#include <vector>

// #undef __STRICT_ANSI__
// #define GLM_ENABLE_EXPERIMENTAL
// #include <glm/glm.hpp>
// #include <glm/gtx/quaternion.hpp>
// #include <glm/ext/matrix_transform.hpp>

// #include <volk.h>
// #include <vulkan/vulkan.h>
// #include <vk_enum_string_helper.h> //idk why but it is neither shipped with Linux Vulkan SDK nor bundled in vulkan-sdk-components
// #include <vk_mem_alloc.h>
// #include <GLFW/glfw3.h>

#define RMLUI_STATIC_LIB
#include <RmlUi/Core.h>

#include "../../lum-al/src/al.hpp"

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

//Everything is X -> Y -> Z order in arrays (vectors)
const int BLOCK_SIZE = 16; // each block in common world is BLOCK_SIZE x BLOCK_SIZE x BLOCK_SIZE
const int MATERIAL_PALETTE_SIZE = 256; //0 always empty
const int RCACHE_RAYS_PER_PROBE = 32;
const int BLOCK_PALETTE_SIZE_X = 64;
const int BLOCK_PALETTE_SIZE_Y = 64;
const int BLOCK_PALETTE_SIZE = (BLOCK_PALETTE_SIZE_X* BLOCK_PALETTE_SIZE_Y);

const int MAX_FRAMES_IN_FLIGHT = 2;

typedef u8 MatID_t;
typedef i16 BlockID_t;
const BlockID_t SPECIAL_BLOCK = 32767;
typedef struct Material {
    vec4 color; //color AND transparancy
    f32 emmit; //emmits same color as reflect
    f32 rough;
} Material;

typedef u8 Voxel;

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

typedef struct Particle {
    vec3 pos;
    vec3 vel;
    float lifeTime;
    MatID_t matID;
} Particle;

typedef struct IndexedVertices {
    ring<Buffer> indexes;
    u32 icount;
} IndexedVertices;

typedef struct NonIndexedVertices {
    ring<Buffer> vertices;
    u32 vcount;
} NonIndexedVertices;

typedef struct FaceBuffers {
    IndexedVertices Pzz, Nzz, zPz, zNz, zzP, zzN;
    ring<Buffer> vertexes;
} FaceBuffers;

typedef struct Mesh {
    //everything is Staged per frame in flight, so you can update it faster. But costs double the memory
    FaceBuffers triangles;
    ring<Image> voxels; //3d image of voxels in this mesh, used to represent mesh to per-frame world voxel representation
    quat rot;
    vec3 shift;
    ivec3 size;
} Mesh;

typedef struct UiMesh {
    Buffer vertexes;
    Buffer indexes;
    u32 icount;
    Image* image;
} UiMesh;

typedef struct Block {
    Voxel voxels[BLOCK_SIZE][BLOCK_SIZE][BLOCK_SIZE];
    Mesh mesh;
} Block; //in palette

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

    Type& operator() (int x, int y, int z) {
        return this->memory [x + _size.x * y + (_size.x* _size.y) * z];
    }
    Type& operator() (ivec3 v) {return (*this) (v.x, v.y, v.z);}

};

typedef struct Camera {
    dvec3 cameraPos = vec3(60, 0, 194);
    dvec3 cameraDir = normalize(vec3(0.61, 1.0, -0.8));
    dmat4 cameraTransform = glm::identity<dmat4>();
    double pixelsInVoxel = 5.0;
    dvec2 originViewSize = dvec2(1920, 1080);
    dvec2 viewSize = originViewSize / pixelsInVoxel;
    dvec3 cameraRayDirPlane = normalize (dvec3 (cameraDir.x, cameraDir.y, 0));
    dvec3 horizline = normalize (cross (cameraRayDirPlane, dvec3 (0, 0, 1)));
    dvec3 vertiline = normalize (cross (cameraDir, horizline));

    void updateCamera();
} Camera;

//forward declaration for pointer
class MyRenderInterface;

struct LumRenderer {
    Renderer render;
    
  public:
    void init (Settings settings);
    void setupDescriptors();
    void createImages();
    VkResult createSwapchainDependent();
    void createSwapchainDependentImages();
    VkResult cleanupSwapchainDependent();
    void createPipilines();
    void cleanup();
    void createSamplers();

    // sets voxels and size. By default uses first .vox palette as main palette
    void load_mesh (Mesh* mesh, const char* vox_file, bool _make_vertices = true, bool extrude_palette = true);
    // sets voxels and size. By default uses first .vox palette as main palette
    void load_mesh (Mesh* mesh, Voxel* voxel_mem, int x_size, int y_size, int z_size, bool _make_vertices = true);
    void free_mesh (Mesh* mesh);
    void make_vertices (Mesh* mesh, Voxel* Voxels, int x_size, int y_size, int z_size);
    void load_vertices (Mesh* mesh, VoxelVertex* vertices);
    // void extrude_palette(Material* material_palette);
    void load_block (Block** block, const char* vox_file);
    void free_block (Block** block);

    void load_scene (const char* scene_file);
    void save_scene (const char* scene_file);

    Material mat_palette[MATERIAL_PALETTE_SIZE];
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
                void blockify_mesh(Mesh* mesh);
            void end_blockify();
            void blockify_custom(void* ptr); // just in case you have custom blockify algorithm. If using this, no startBlockify needed
            void exec_copies();
            void start_map();
                void map_mesh(Mesh* mesh);
            void end_map();
            void update_radiance();
            void updade_grass(vec2 windDirection);
            void updade_water();
            void recalculate_df();
            void recalculate_bit();
        void end_compute();
        void start_lightmap();
            void lightmap_start_blocks();
                void lightmap_block(Mesh* block_mesh, int block_id, ivec3 shift); void lightmap_block_face(i8vec3 normal, IndexedVertices& buff, int block_id);
            void lightmap_start_models();
                void lightmap_model(Mesh* mesh); void lightmap_model_face(vec3 normal, IndexedVertices& buff);
        void end_lightmap(); //ends somewhere after raygen but operates on separate cmd buf
            // void start_lightmap();
            void start_raygen();
            void raygen_start_blocks();
                void raygen_block(Mesh* block_mesh, int block_id, ivec3 shift); void draw_block_face(i8vec3 normal, IndexedVertices& buff, int block_id);
            void raygen_start_models();
                void raygen_model(Mesh* mesh); void draw_model_face(vec3 normal, IndexedVertices& buff);
            void update_particles();
            void raygen_map_particles();
            void raygen_start_grass();
                void raygen_map_grass(vec4 shift, int size);
            // void raygen_end_grass();
            void raygen_start_water();
                void raygen_map_water(vec4 shift, int size);
            // void raygen_end_water();
        void end_raygen();
            // void end_raygen_first();
            void diffuse();
            void ambient_occlusion();
            void start_2nd_spass();
                void glossy_raygen();
                void smoke_raygen();// special mesh?
                void smoke();
                void glossy();
                void tonemap();
                void start_ui();
                    //ui updates
                void end_ui();
            void end_2nd_spass();
        void present();
    void end_frame();

    template<class Vertex_T> vector<Buffer> createElemBuffers (Vertex_T* vertices, u32 count, u32 buffer_usage = 0);
    template<class Vertex_T> vector<Buffer> createElemBuffers (vector<Vertex_T> vertices, u32 buffer_usage = 0);

    ring<Image> create_RayTrace_VoxelImages (Voxel* voxels, ivec3 size);
    void updateBlockPalette (Block** blockPalette);
    void updateMaterialPalette (Material* materialPalette);

    // VkExtent2D swapChainExtent;
    // VkExtent2D raytraceExtent;
    VkExtent2D lightmapExtent;

    RasterPipe lightmapBlocksPipe;
    RasterPipe lightmapModelsPipe;

    RasterPipe raygenBlocksPipe;
    RasterPipe raygenModelsPipe;
    VkDescriptorSetLayout raygenModelsPushLayout;
    RasterPipe raygenParticlesPipe;
    RasterPipe raygenGrassPipe;
    RasterPipe raygenWaterPipe;

    RasterPipe diffusePipe;
    RasterPipe aoPipe;
    RasterPipe fillStencilGlossyPipe;
    RasterPipe fillStencilSmokePipe;
    RasterPipe glossyPipe;
    RasterPipe smokePipe;
    RasterPipe tonemapPipe;
    RasterPipe overlayPipe;

    RenderPass lightmapRpass;
    RenderPass gbufferRpass;
    RenderPass shadeRpass; //for no downscaling

    ring<VkCommandBuffer> computeCommandBuffers;
    ring<VkCommandBuffer> lightmapCommandBuffers;
    ring<VkCommandBuffer> graphicsCommandBuffers;
    ring<VkCommandBuffer> copyCommandBuffers; //runtime copies for ui. Also does first frame resources

    ring<Image> lightmap;
    ring<Image> swapchainImages;
    ring<Image> highresFrame;
    ring<Image> highresDepthStencil;
    // Image highresStencils;
    ring<Image> highresMatNorm;
    //downscaled version for memory coherence. TODO:TEST perfomance on tiled
    // ring<Image> lowresMatNorm;
    // ring<Image> lowresDepthStencil;
    ring<VkImageView> stencilViewForDS;
    ring<Image> farDepth; //represents how much should smoke traversal for
    ring<Image> nearDepth; //represents how much should smoke traversal for
    ring<Image> maskFrame; //where lowres renders to. Blends with highres afterwards

    VkSampler nearestSampler;
    VkSampler linearSampler;
    VkSampler linearSampler_tiled;
    VkSampler linearSampler_tiled_mirrored;
    VkSampler overlaySampler;
    VkSampler shadowSampler;
    VkSampler unnormLinear;
    VkSampler unnormNearest;

    //is or might be in use when cpu is recording new one. Is pretty cheap, so just leave it
    ring<Buffer> stagingWorld;
    // ring<void*> stagingWorldMapped;
    ring<Image> world; //can i really use just one?

    ring<Image> radianceCache;

    ring<Image> originBlockPalette;
    ring<Image> distancePalette;
    ring<Image> bitPalette; //bitmask of originBlockPalette
    ring<Image> materialPalette;

    ring<Buffer> lightUniform;
    ring<Buffer> uniform;
    ring<Buffer> aoLutUniform;
    vector<i8vec4> radianceUpdates;
    vector<i8vec4> specialRadianceUpdates;
    ring<Buffer> gpuRadianceUpdates;
    // ring<void*> stagingRadianceUpdatesMapped;
    ring<Buffer> stagingRadianceUpdates;

    vector<Particle> particles;
    ring<Buffer> gpuParticles; //multiple because cpu-related work
    // ring<void* > gpuParticlesMapped; //multiple because cpu-related work

    ring<Image> perlinNoise2d; //full-world grass shift (~direction) texture sampled in grass
    ring<Image> grassState; //full-world grass shift (~direction) texture sampled in grass
    ring<Image> waterState; //~same but water

    ring<Image> perlinNoise3d; //4 channels of different tileable noise for volumetrics

    VkDescriptorPool descriptorPool;
    ComputePipe raytracePipe;
    ComputePipe radiancePipe;
    ComputePipe mapPipe;
    VkDescriptorSetLayout mapPushLayout;
    ComputePipe updateGrassPipe;
    ComputePipe updateWaterPipe;
    ComputePipe genPerlin2dPipe; //generate noise for grass
    ComputePipe genPerlin3dPipe; //generate noise for grass
    ComputePipe dfxPipe;
    ComputePipe dfyPipe;
    ComputePipe dfzPipe;
    ComputePipe bitmaskPipe;

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


class MyRenderInterface : public Rml::RenderInterface {
  public:
    MyRenderInterface() {}

    ~MyRenderInterface() {}; //override 
    // Called by RmlUi when it wants to render geometry that the application does not wish to optimise
    void RenderGeometry (Rml::Vertex* vertices,
                         int num_vertices,
                         int* indices,
                         int num_indices,
                         Rml::TextureHandle texture,
                         const Rml::Vector2f& translation); //override;

    // Called by RmlUi when it wants to compile geometry it believes will be static for the forseeable future.
    Rml::CompiledGeometryHandle CompileGeometry (Rml::Vertex* vertices, int num_vertices, int* indices, int num_indices, Rml::TextureHandle texture); //override;

    // Called by RmlUi when it wants to render application-compiled geometry.
    void RenderCompiledGeometry (Rml::CompiledGeometryHandle geometry, const Rml::Vector2f& translation); //override;

    // Called by RmlUi when it wants to release application-compiled geometry.
    void ReleaseCompiledGeometry (Rml::CompiledGeometryHandle geometry); //override;

    // Called by RmlUi when it wants to enable or disable scissoring to clip content.
    void EnableScissorRegion (bool enable); //override;

    // Called by RmlUi when it wants to change the scissor region.
    void SetScissorRegion (int x, int y, int width, int height); //override;

    // Called by RmlUi when a texture is required by the library.
    bool LoadTexture (Rml::TextureHandle& texture_handle,
                      Rml::Vector2i& texture_dimensions,
                      const Rml::String& source); //override;

    // Called by RmlUi when a texture is required to be built from an internally-generated sequence of pixels.
    bool GenerateTexture (Rml::TextureHandle& texture_handle,
                          const Rml::byte* source,
                          const Rml::Vector2i& source_dimensions); //override;

    // Called by RmlUi when a loaded texture is no longer required.
    void ReleaseTexture (Rml::TextureHandle texture_handle); //override;

    // Called by RmlUi when it wants the renderer to use a new transform matrix.
    // If no transform applies to the current element, nullptr is submitted. Then it expects the renderer to use
    // an glm::identity matrix or otherwise omit the multiplication with the transform.
    void SetTransform (const Rml::Matrix4f* transform); //override;

    // Rml::Context* GetContext(); //override {};
    LumRenderer* render;
    Image* default_image = NULL;

    mat4 current_transform = glm::identity<mat4>();
    // bool has_scissors = true;
    VkRect2D last_scissors = {{0, 0}, {1, 1}};
};