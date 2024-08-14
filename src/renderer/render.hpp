#pragma once

#include <climits>
#include <cmath>
#include <optional>
#include <vector>
#include <string.h>
#include <stdio.h>
#include <io.h>
#include <limits.h>
#include <stdio.h>
#include <stdio.h>

#include <glm/detail/qualifier.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_projection.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/vector_int3.hpp>
#include <glm/ext/vector_uint4.hpp>
#include <glm/trigonometric.hpp>
#include <glm/fwd.hpp>
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>
#include <Volk/volk.h>
#include <vulkan/vulkan.h>
#include <vulkan/vk_enum_string_helper.h>
#include <vma/vk_mem_alloc.h>
#include <GLFW/glfw3.h>

#define RMLUI_STATIC_LIB
#include <RmlUi/Core.h>

#include <defines.hpp>

using namespace glm;
using namespace std;

//Everything is X -> Y -> Z order in arrays (vectors)
const int BLOCK_SIZE = 16; // each block in common world is BLOCK_SIZE x BLOCK_SIZE x BLOCK_SIZE
const int MATERIAL_PALETTE_SIZE = 256; //0 always empty
const int RCACHE_RAYS_PER_PROBE = 32;
// on nvidia required 2d isntead of 1d cause VK_DEVICE_LOST on vkCmdCopy. FML
const int BLOCK_PALETTE_SIZE_X = 64;
const int BLOCK_PALETTE_SIZE_Y = 64;
const int BLOCK_PALETTE_SIZE =  (BLOCK_PALETTE_SIZE_X*BLOCK_PALETTE_SIZE_Y);

// #define FRAME_FORMAT VK_FORMAT_A2B10G10R10_UNORM_PACK32
// #define DEPTH_FORMAT VK_FORMAT_D32_SFLOAT_S8_UINT //somehow faster than VK_FORMAT_D24_UNORM_S8_UINT even on low-end 
// #define MATNORM_FORMAT VK_FORMAT_R8G8B8A8_SNORM
// #define RADIANCE_FORMAT VK_FORMAT_A2B10G10R10_UNORM_PACK32

#define DEPTH_LAYOUT VK_IMAGE_LAYOUT_GENERAL

// #define ACCUMULATE_HIGHRES
//should be (at least) 2 for temporal accumulation, but can be changed if rebind images
const int MAX_FRAMES_IN_FLIGHT = 2;

using namespace std;
using namespace glm;

typedef   u8 MatID_t;
typedef  i16 BlockID_t;

typedef struct Material {
    // glm::vec4<u8> casd;
    vec4 color; //colo   r AND transparancy
    f32 emmit; //emmits same color as reflect
    f32 rough;
} Material;
typedef u8 Voxel;

#define POS_NORM_BIT_MASK 0b10000000
typedef struct VoxelVertex {
    vec<3, unsigned char, defaultp> pos;
    vec<3,   signed char, defaultp> norm;
    // vec<3, unsigned char, defaultp> pos_AND_normBits; //
    MatID_t matID;
} VoxelVertex;
typedef struct PackedVoxelVertex {
    vec<3, unsigned char, defaultp> pos;
    // unsigned char norm;
    MatID_t matID;
} PackedVoxelVertex;
typedef struct PackedVoxelQuad {
    vec<2, unsigned char, defaultp> size;
    vec<3, unsigned char, defaultp> pos;
    MatID_t matID;
    // unsigned char norm;
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
typedef struct Buffer {
    VkBuffer buffer;
    VmaAllocation alloc;
} Buffer;
typedef struct Image {
    VkImage image;
    VkImageView view;
    VmaAllocation alloc;
    // VkFormat format;
    VkImageLayout layout;
    VkImageAspectFlags aspect;
} Image;
enum MeshVertexTypes {
    // MESH_VERTEX_TYPE_
};
typedef struct IndexedVertices {
    vector<Buffer> indexes;
    u32 icount;
} IndexedVertices;
typedef struct NonIndexedVertices {
    vector<Buffer> vertices;
    u32 vcount;
} NonIndexedVertices;
typedef struct FaceBuffers {
    NonIndexedVertices Pzz, Nzz, zPz, zNz, zzP, zzN;
    // vector<Buffer> Pzz, Nzz, zPz, zNz, zzP, zzN;
} FaceBuffers;
typedef struct Mesh {
    //everything is Staged per frame in flight, so you can update it faster. But costs double the memory
    //VkBuffers for ray generation
    // vector<Buffer> vertexes;
    // vector<Buffer> indexes;
    FaceBuffers triangles;

    vector<Image> voxels; //3d image of voxels in this mesh, used to represent mesh to per-frame world voxel representation

    quat rot;
    quat old_rot;
    vec3 shift;
    vec3 old_shift;

    // mat4 transform; //used to transform from self coordinate system to world coordinate system
    // mat4 old_transform; //for denoising
    
    ivec3 size;
    // Voxel* host_voxels; // single copy of voxel memory on cpu. If NULL then you store your data yourself
} Mesh;
// typedef struct GrassState {
//     //2ch image stored sampled wind and physical contact impact 
//     Image marks; //RENAME ME PLEASE SOMEONE
// } GrassState;
// typedef struct WaterState {
//     //2ch image stored sampled waves height and (possibly) foam
//     Image waves;
// } WaterState;
// typedef struct GrassUpdateReqest {
//     float dTime;
//     vec2 windDirection, collsionPoint;
// } GrassUpdateReqest;
// typedef struct WaterUpdateReqest {
//     float dTime;
//     vec2 windDirection, collsionPoint;
// } WaterUpdateReqest;

typedef struct UiMesh {
    Buffer vertexes;
    Buffer indexes;
    u32 icount;
    Image* image;
} UiMesh;
typedef struct ImageDeletion {
    Image image;
    int life_counter;
} ImageDeletion;
typedef struct BufferDeletion {
    Buffer buffer;
    int life_counter;
} BufferDeletion;

//used for configuring raster pipeline
typedef struct AttrFormOffs{
    VkFormat format;
    uint32_t offset;
}AttrFormOffs;

enum BlendAttachment{
    NO_BLEND,
    DO_BLEND,
    BLEND_REPLACE_IF_GREATER, //basically max
    BLEND_REPLACE_IF_LESS, //basically max
};
enum DepthTesting{
    NO_DEPTH_TEST,
    FULL_DEPTH_TEST,
    READ_DEPTH_TEST,
    WRITE_DEPTH_TEST,
};
//it was not discard in fragment
enum Discard{
    NO_DISCARD,
    DO_DISCARD,
};
const VkStencilOpState NO_STENCIL = {};

typedef struct ShaderStage{
    const char* src;
    VkShaderStageFlagBits stage;
}ShaderStage;
//problem with such abstractions in vulkan is that they almost always have to be extended to a point where they make no sense
typedef struct RasterPipe { 
    VkPipeline line;
    VkPipelineLayout lineLayout;
    
    vector<VkDescriptorSet> sets;
    VkDescriptorSetLayout setLayout;

    VkRenderPass renderPass; //we dont really need to store it in here but why not
    i32 subpassId;
    // vksubn; //we dont really need to store it in here but why not
} RasterPipe;
typedef struct ComputePipe { 
    VkPipeline line;
    VkPipelineLayout lineLayout;
    
    vector<VkDescriptorSet> sets;
    VkDescriptorSetLayout setLayout;
} ComputePipe;

/*
we specify input images, output images
then mathing renderpass + pipelines is created

create subpass raster pipe
create render pass (subpipe1 subpipe2)

bind renderpass
bind subpass raster pipe
*/

//pipeline with extra info for subpass creation
typedef struct subPass { 
    VkPipeline pipe;
    VkPipelineLayout pipeLayout;
    
    VkDescriptorSetLayout dsetLayout;
    vector<VkDescriptorSet> descriptors;

    VkRenderPass rpass; //we dont really need to store it in here but why not
} subPass;

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
    void allocate(int x, int y, int z) {
        _size = ivec3(x,y,z);
        this->memory = (Type*)calloc(x*y*z, sizeof(Type));
    }
    void set(Type val) {
        for(int x=0; x<_size.x; x++){
        for(int y=0; y<_size.y; y++){
        for(int z=0; z<_size.z; z++){
            this->memory [x + _size.x*y + (_size.x*_size.y)*z] = val;
        }}}
        // memset(dst);
        // this->memory = (Type*)malloc(x*y*z);
    }
    //makes content invalid 
    void allocate(ivec3 size) {
        this->allocate(size.x, size.y, size.z);
    }
    Type* data() {
        return this->memory;
    }
    uvec3 size() {
        return _size;
    }
    Type& operator()(int x, int y, int z) {
        return this->memory [x + _size.x*y + (_size.x*_size.y)*z];
    }
    void deallocate(){
        free(memory);
    }
};
typedef struct Window{
    GLFWwindow* pointer;
    int width;
    int height;
} Window;

typedef struct QueueFamilyIndices {
    optional<u32> graphicalAndCompute;
    optional<u32> present;

public:
    bool isComplete(){
        return graphicalAndCompute.has_value() && present.has_value();
    }
} QueueFamilyIndices;

typedef struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    vector<VkSurfaceFormatKHR> formats;
    vector<VkPresentModeKHR> presentModes;

public:
    bool is_Suitable(){
        return (not formats.empty()) and (not presentModes.empty()); 
    }
} SwapChainSupportDetails;

//TODO:
enum denoise_targets{
    DENOISE_TARGET_LOWRES,
    DENOISE_TARGET_HIGHRES,
};
enum RelativeDescriptorPos {
    RD_NONE,    //what?
    RD_PREVIOUS,  //Relatove Descriptor position previous - for accumulators
    RD_CURRENT, //Relatove Descriptor position matching - common cpu-paired
    RD_FIRST,    //Relatove Descriptor position first    - for gpu-only
};

#define NO_SAMPLER ((VkSampler)(0))
#define NO_LAYOUT ((VkImageLayout)(0))
typedef struct DescriptorInfo {
    VkDescriptorType type;
    RelativeDescriptorPos relativePos;
    vector<Buffer> buffers;
    vector<Image> images;
    VkSampler imageSampler;
    VkImageLayout imageLayout; //ones that will be in use, not current
} DescriptorInfo;

typedef struct DelayedDescriptorSetup {
    VkDescriptorSetLayout* setLayout;
    vector<VkDescriptorSet>* sets; 
    vector<DescriptorInfo> descriptions;
    VkShaderStageFlags stages;
    VkDescriptorSetLayoutCreateFlags createFlags;
} DelayedDescriptorSetup;

//forward declaration for pointer
class MyRenderInterface;   

class Renderer {
public: 
    void init(int xSize=8, int ySize=8, int zSize=8, int staticBlockPaletteSize=128, int maxParticleCount=1024, float ratio = 1.5f, bool vsync=true, bool fullscreen = false);
    void setupDescriptors();
    void createImages();
    void createPipilines();
    void cleanup();

    // sets voxels and size. By default uses first .vox palette as main palette
    void load_mesh(Mesh* mesh, const char* vox_file,                                 bool _make_vertices=true, bool extrude_palette=true);
    // sets voxels and size. By default uses first .vox palette as main palette
    void load_mesh(Mesh* mesh, Voxel* voxel_mem, int x_size, int y_size, int z_size, bool _make_vertices=true);
    void free_mesh(Mesh* mesh);
    void make_vertices(Mesh* mesh, Voxel* Voxels, int x_size, int y_size, int z_size);
    void load_vertices(Mesh* mesh, VoxelVertex* vertices);
    // void extrude_palette(Material* material_palette);
    void load_block(Block** block, const char* vox_file);
    void free_block(Block** block);

    void load_scene(const char* scene_file);
    void save_scene(const char* scene_file);
    
    Material mat_palette[MATERIAL_PALETTE_SIZE];
    table3d<BlockID_t>  origin_world = {};
    table3d<BlockID_t> current_world = {};
    MyRenderInterface* ui_render_interface;
    int static_block_palette_size;
    ivec3 world_size;
    
    int   pre_denoiser_count = 0;
    int  post_denoiser_count = 0;
    int final_denoiser_count = 0;
    float _ratio;
    // float fratio;
private:
    bool hasPalette = false;
    int maxParticleCount;
    vector<VkImageCopy> blockCopyQueue = {};
    vector<VkImageSubresourceRange> blockclearQueue = {};
public:
    double deltaTime = 0;

    bool scaled = false;
    bool vsync = false;
    bool fullscreen = false;
    bool resized = false;

    dvec3 cameraPos     = vec3(60, 0, 194);
    dvec3 cameraPos_OLD = cameraPos;
    dvec3 cameraDir     = normalize(vec3(0.6, 1.0, -0.8));
    dvec3 cameraDir_OLD = normalize(vec3(0.6, 1.0, -0.8));
    dmat4 cameraTransform     = identity<dmat4>();
    dmat4 cameraTransform_OLD = identity<dmat4>();

    void update_camera();

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
        void start_raygen();
            void raygen_mesh(Mesh* mesh, int block_id); void draw_face_helper(vec3 normal, NonIndexedVertices& buff, int block_id);
            void update_particles();
            void raygen_map_particles();
            void raygen_start_grass();
                void raygen_map_grass(vec4 shift, int size);
            void raygen_end_grass();
                void raygen_start_water();
                    void raygen_map_water(vec4 shift, int size);
                void raygen_end_water();
            // void end_raygen_first();
            void diffuse();
            void start_2nd_spass();
                void smoke_raygen();// special mesh?
                void smoke();
                void glossy();
            void end_2nd_spass();
        void end_raygen();
            void collect_glossy();
            void start_ui();
            void end_ui();
        void present();
    void end_frame();
        // void inter();
            // void raytrace();
            // void denoise(int iterations, int denoising_radius, denoise_targets target);
            // void accumulate();
            // void upscale();

    template<class Vertex_T> vector<Buffer> create_elemBuffers(Vertex_T* vertices, u32 count, u32 buffer_usage = 0); 
    template<class Vertex_T> vector<Buffer> create_elemBuffers(vector<Vertex_T> vertices, u32 buffer_usage = 0); 

    vector<Image>  create_RayTrace_VoxelImages(Voxel* voxels, ivec3 size);
    void update_Block_Palette(Block** blockPalette);
    void update_Material_Palette(Material* materialPalette);
    void update_SingleChunk(BlockID_t* blocks);

    void cmdPipelineBarrier(VkCommandBuffer commandBuffer, 
        VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, 
        VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask,
        Buffer buffer);
    void cmdPipelineBarrier(VkCommandBuffer commandBuffer, 
        VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, 
        VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask,
        Image image);    
    void cmdPipelineBarrier(VkCommandBuffer commandBuffer, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask);
    //used as just barrier and can also transfer image layout
    void cmdTransLayoutBarrier(VkCommandBuffer commandBuffer, VkImageLayout targetLayout,
        VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask, 
        Image* image);
    void cmdTransLayoutBarrier(VkCommandBuffer commandBuffer, VkImageLayout srcLayout, VkImageLayout targetLayout,
        VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask, 
        Image* image);
private:
    void  cleanupSwapchainDependent();
    void   createSwapchainDependent();
    void recreateSwapchainDependent();

    void  cleanupImages();

    void createAllocator();
    void createWindow();
    void createInstance();
    void setupDebug_Messenger();
    void createSurface();
    SwapChainSupportDetails querySwapchainSupport(VkPhysicalDevice);
    QueueFamilyIndices findQueueFamilies(VkPhysicalDevice);
    bool checkFormatSupport(VkPhysicalDevice device, VkFormat format, VkFormatFeatureFlags features);
    bool isPhysicalDeviceSuitable(VkPhysicalDevice);
    //call get_PhysicalDevice_Extensions first
    bool checkPhysicalDeviceExtensionSupport(VkPhysicalDevice);
    void pickPhysicalDevice();
    void createLogicalDevice();
    void createSwapchain();
    VkSurfaceFormatKHR chooseSwapSurfaceFormat(vector<VkSurfaceFormatKHR> availableFormats);
    VkPresentModeKHR chooseSwapPresentMode(vector<VkPresentModeKHR> availablePresentModes);
    VkExtent2D chooseSwapExtent(VkSurfaceCapabilitiesKHR capabilities);
    void createSwapchainImageViews();
    void createRenderPass1();
    void createRenderPass2(); //2=3. 
    void createRenderPass3(); //3=2. Cool, right? borrowed from dreambeard
    void createRenderPassAlt();

    VkFormat findSupportedFormat(vector<VkFormat> candidates, VkImageType type, VkImageTiling tiling, VkImageUsageFlags usage);
    
    void create_Raster_Pipeline(RasterPipe* pipe, vector<ShaderStage> shader_stages, vector<AttrFormOffs> attr_desc, 
        u32 stride, VkVertexInputRate input_rate, VkPrimitiveTopology topology,
        VkExtent2D extent, vector<BlendAttachment> blends, u32 push_size, DepthTesting depthTest, VkCullModeFlags culling, Discard discard, VkStencilOpState stencil);
    void destroy_Raster_Pipeline(RasterPipe* pipe);

    void create_Compute_Pipeline(ComputePipe* pipe, VkDescriptorSetLayout extra_dynamic_layout, const char* src, u32 push_size, VkPipelineCreateFlags create_flags);
    void destroy_Compute_Pipeline(ComputePipe* pipe);

    void createDescriptorPool();
    void deferDescriptorsetup(VkDescriptorSetLayout* dsetLayout, vector<VkDescriptorSet>* descriptors, vector<DescriptorInfo> description, VkShaderStageFlags stages, VkDescriptorSetLayoutCreateFlags createFlags = 0);
    void setupDescriptor(VkDescriptorSetLayout* dsetLayout, vector<VkDescriptorSet>* descriptors, vector<DescriptorInfo> description, VkShaderStageFlags stages);
    vector<DelayedDescriptorSetup> delayed_descriptor_setups;
    void flushDescriptorSetup();

    void createSamplers();

    void deleteImages(vector<Image>* images);
    void deleteImages(Image* images);
    void create_Image_Storages(vector<Image>* images, 
    VkImageType type, VkFormat format, VkImageUsageFlags usage, VmaMemoryUsage vma_usage, VmaAllocationCreateFlags vma_flags, VkImageAspectFlags aspect, 
    uvec3 size, int mipmaps = 1, VkSampleCountFlagBits sample_count = VK_SAMPLE_COUNT_1_BIT);
    void create_Image_Storages(Image* image, 
    VkImageType type, VkFormat format, VkImageUsageFlags usage, VmaMemoryUsage vma_usage, VmaAllocationCreateFlags vma_flags, VkImageAspectFlags aspect, 
    uvec3 size, int mipmaps = 1, VkSampleCountFlagBits sample_count = VK_SAMPLE_COUNT_1_BIT);
// #ifdef STENCIL 
    void create_Image_Storages_DepthStencil(vector<VkImage>* images, vector<VmaAllocation>* allocs, vector<VkImageView>* depthViews, vector<VkImageView>* stencilViews,
    VkImageType type, VkFormat format, VkImageUsageFlags usage, VmaMemoryUsage vma_usage, VmaAllocationCreateFlags vma_flags, VkImageAspectFlags aspect, VkImageLayout layout, VkPipelineStageFlags pipeStage, VkAccessFlags access, 
    uvec3 size);
// #endif
    // public: void create_grass_state(GrassState* state);
    // public: void create_water_state(WaterState* state);
// private:
    void generateMipmaps(VkCommandBuffer commandBuffer, VkImage image, int32_t texWidth, int32_t texHeight, uint32_t mipLevels, VkImageAspectFlags aspect);
    // void destroy_images
    void create_Buffer_Storages(vector<Buffer>* buffers, VkBufferUsageFlags usage, u32 size, bool host = false);
    void create_Buffer_Storages(Buffer* buffer, VkBufferUsageFlags usage, u32 size, bool host = false);
    void create_compute_pipelines_helper(const char* name, VkDescriptorSetLayout  descriptorSetLayout, VkPipelineLayout* pipelineLayout, VkPipeline* pipeline, u32 push_size, u32 flags = 0);
    void create_compute_pipelines();
    // void create_Blockify_Pipeline();
    // void create_Copy_Pipeline();
    // void create_Map_Pipeline();
    // void create_Df_Pipeline();
    // void create_Raytrace_Pipeline(); 
    VkShaderModule create_Shader_Module(vector<char>* code);
    //creates framebuffers that point to attachments view specified views
    void create_N_Framebuffers(vector<VkFramebuffer>* framebuffers, vector<vector<VkImageView>>* views, VkRenderPass renderPass, u32 N, u32 Width, u32 Height);
    void create_Command_Pool();
    void create_Command_Buffers(vector<VkCommandBuffer>* commandBuffers, u32 size);
    void record_Command_Buffer_Graphical(VkCommandBuffer commandBuffer, u32 imageIndex);
    void record_Command_Buffer_RayGen(VkCommandBuffer commandBuffer);
    void record_Command_Buffer_Compute  (VkCommandBuffer commandBuffer);
    void create_Sync_Objects();

public:
    void create_Buffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer* buffer, VkDeviceMemory* bufferMemory);
    void copy_Buffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size); 
    void copy_Buffer(VkBuffer srcBuffer, Image* image, uvec3 size);
    VkCommandBuffer begin_Single_Time_Commands();
    void end_Single_Time_Commands(VkCommandBuffer commandBuffer);
    void transition_Image_Layout_Singletime(Image* image, VkImageLayout newLayout, int mipmaps = 1);
    void transition_Image_Layout_Cmdb(VkCommandBuffer commandBuffer, VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout,
        VkPipelineStageFlags sourceStage, VkPipelineStageFlags destinationStage, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask);
    void copy_Whole_Image(VkExtent3D extent, VkCommandBuffer cmdbuf, VkImage src, VkImage dst);
    
    void process_ui_deletion_queue();

    void get_Instance_Extensions();
    // void get_PhysicalDevice_Extensions();

    u32 STORAGE_BUFFER_DESCRIPTOR_COUNT = 0;
    u32 STORAGE_IMAGE_DESCRIPTOR_COUNT = 0;
    u32 COMBINED_IMAGE_SAMPLER_DESCRIPTOR_COUNT = 0;
    u32 UNIFORM_BUFFER_DESCRIPTOR_COUNT = 0; 
    u32 INPUT_ATTACHMENT_DESCRIPTOR_COUNT = 0;
    u32 descriptor_sets_count = 0;
    void count_Descriptor(const VkDescriptorType type);
    void create_DescriptorSetLayout(vector<VkDescriptorType> descriptorTypes, VkShaderStageFlags stageFlags, VkDescriptorSetLayout* layout, VkDescriptorSetLayoutCreateFlags flags = 0);
public:
    

    Window window;
    VkInstance instance;
    //picked one. Maybe will be multiple in future 
    VkPhysicalDevice physicalDevice; 
    VkDevice device;
    VkSurfaceKHR surface;

    QueueFamilyIndices familyIndices;
    VkQueue graphicsQueue;
    VkQueue presentQueue;
    // VkQueue computeQueue;
    VkCommandPool commandPool;

    VkSwapchainKHR swapchain;
    VkFormat   swapChainImageFormat;
    VkExtent2D swapChainExtent;
    VkExtent2D  raytraceExtent;

    // RasterPipe lightmapPipe;

    RasterPipe raygenBlocksPipe;
    VkDescriptorSetLayout blocksPushLayout;
    RasterPipe raygenParticlesPipe;
    RasterPipe raygenGrassPipe;
    RasterPipe raygenWaterPipe;
    RasterPipe diffusePipe;

    RasterPipe fillStencilGlossyPipe;
    RasterPipe fillStencilSmokePipe;
    RasterPipe glossyPipe;
    RasterPipe smokePipe;

    RasterPipe blurPipe; // i can use it for both mirror and glow

    RasterPipe overlayPipe;

    VkRenderPass       lightmapRpass;
    VkRenderPass raygen2diffuseRpass;
    VkRenderPass   smoke2glossyRpass;
    VkRenderPass   blur2presentRpass;
    VkRenderPass   altRpass; //for no downscaling

    vector<VkFramebuffer>  rayGenFramebuffers; //for 1st rpass
    // vector<VkFramebuffer>  glossyFramebuffers; //for 3rd rpass. Size of 1
    // vector<VkFramebuffer> overlayFramebuffers; //for 2st rpass.
    vector<VkFramebuffer> altFramebuffers; //for no downscaling

    vector<VkCommandBuffer>  computeCommandBuffers; 
    vector<VkCommandBuffer> graphicsCommandBuffers;
    vector<VkCommandBuffer>     copyCommandBuffers; //runtime copies for ui. Also does first frame resources

    vector<VkSemaphore> imageAvailableSemaphores;
    vector<VkSemaphore> renderFinishedSemaphores; //to sync renering with presenting

    vector<VkFence>     frameInFlightFences;    

    vector<Image> swapchainImages;
           Image highresFrames;
           Image highresDepthStencil;
           Image highresStencils;
           Image highresMatNorms; 
           //downscaled version for memory coherence. TODO:TEST perfomance on tiled 
           Image lowresMatNorm;
           Image lowresDepthStencil;
               VkImageView stencilViewForDS;
           Image farDepth; //represents how much should smoke traversal for
           Image nearDepth; //represents how much should smoke traversal for
           Image maskFrame; //where lowres renders to. Blends with highres afterwards
    

    VkSampler  nearestSampler;
    VkSampler   linearSampler;
    VkSampler   linearSampler_tiled;
    VkSampler  overlaySampler;

    VkSampler  unnormLinear;
    VkSampler  unnormNearest;

    //is or might be in use when cpu is recording new one. Is pretty cheap, so just leave it
    vector<Buffer>      stagingWorld;
    vector<void*>       stagingWorldMapped;
           Image               world; //can i really use just one?

           Image radianceCache;

    vector<Image> originBlockPalette;
           Image     distancePalette;
           Image          bitPalette; //bitmask of originBlockPalette
    vector<Image> materialPalette;
    
    vector<Buffer>         uniform;
    vector<ivec4>         radianceUpdates;
    Buffer             gpuRadianceUpdates;
    vector<void*>  stagingRadianceUpdatesMapped;
    vector<Buffer> stagingRadianceUpdates;

    vector<Particle>     particles;
    vector<Buffer>   gpuParticles; //multiple because cpu-related work
    vector<void* >   gpuParticlesMapped; //multiple because cpu-related work

    Image perlinNoise2d; //full-world grass shift (~direction) texture sampled in grass
    Image grassState; //full-world grass shift (~direction) texture sampled in grass
    Image waterState; //~same but water

    Image perlinNoise3d; //4 channels of different tileable noise for volumetrics

    vector<ImageDeletion>  imageDeletionQueue;  //cpu side  image abstractions deletion queue. Exists for delayed copies
    vector<BufferDeletion> bufferDeletionQueue; //cpu side buffer abstractions deletion queue. Exists for delayed copies


    VkDescriptorPool descriptorPool;
    ComputePipe   raytracePipe;
    ComputePipe   radiancePipe;
    ComputePipe        mapPipe;
    VkDescriptorSetLayout mapPushLayout;
    ComputePipe        updateGrassPipe;
    ComputePipe        updateWaterPipe;
    ComputePipe        genPerlin2dPipe; //generate noise for grass 
    ComputePipe        genPerlin3dPipe; //generate noise for grass 
    // ComputePipe    diffusePipe;
    // ComputePipe     glossyPipe;
    // ComputePipe    denoisePipe;
    // ComputePipe    denoisePipeLowres;
    // ComputePipe accumulatePipe;
    // ComputePipe    upscalePipe;
    // ComputePipe     mipmapPipe;
    // ComputePipe blockifyPipe;
    // ComputePipe     copyPipe;
    ComputePipe dfxPipe;
    ComputePipe dfyPipe;
    ComputePipe dfzPipe;
    ComputePipe bitmaskPipe;

    VmaAllocator VMAllocator; 
    u32 imageIndex = 0;

    //wraps around MAX_FRAMES_IN_FLIGHT
    u32  currentFrame = 0;
    u32 previousFrame = MAX_FRAMES_IN_FLIGHT-1;
    u32     nextFrame = 1;
private:
    int iFrame = 0;
    int paletteCounter = 0;
    #ifndef VKNDEBUG
        VkDebugUtilsMessengerEXT debugMessenger;
    #endif
    vector<VkQueryPool> queryPoolTimestamps;
public:
    int currentTimestamp = 0;
    // #ifdef MEASURE_PERFOMANCE
    // #else
    // #endif
    int timestampCount = 0;
    bool measureAll = true;
    // int timestampCount = 2;
    vector<uint64_t> timestamps = {};
    vector<double> ftimestamps = {};
    vector<double> average_ftimestamps = {};
    vector<const char*> timestampNames = {};
    VkPhysicalDeviceProperties physicalDeviceProperties;
    float timeTakenByRadiance = 0.0;
    int magicSize = 2;

private:
    const VkFormat FRAME_FORMAT =  VK_FORMAT_R16G16B16A16_UNORM;
    VkFormat DEPTH_FORMAT = VK_FORMAT_UNDEFINED;
    const VkFormat DEPTH_FORMAT_PREFERED =  VK_FORMAT_D24_UNORM_S8_UINT;
    const VkFormat DEPTH_FORMAT_SPARE =  VK_FORMAT_D32_SFLOAT_S8_UINT; //TODO somehow faster than VK_FORMAT_D24_UNORM_S8_UINT on low-end
    // const VkFormat DEPTH_FORMAT_PREFERED =  VK_FORMAT_D32_SFLOAT_S8_UINT;
    // const VkFormat DEPTH_FORMAT_SPARE =  VK_FORMAT_D24_UNORM_S8_UINT; //TODO somehow faster than VK_FORMAT_D24_UNORM_S8_UINT on low-end

    const VkFormat MATNORM_FORMAT =  VK_FORMAT_R8G8B8A8_UINT;
    const VkFormat RADIANCE_FORMAT =  VK_FORMAT_A2B10G10R10_UNORM_PACK32;
    const VkFormat SECONDARY_DEPTH_FORMAT =  VK_FORMAT_R16_SFLOAT; 
};

class MyRenderInterface : public Rml::RenderInterface{   
public:
    MyRenderInterface(){}
    ~MyRenderInterface(){}

    // Called by RmlUi when it wants to render geometry that the application does not wish to optimise
    void RenderGeometry(Rml::Vertex* vertices,
                            int num_vertices,
                            int* indices,
                            int num_indices,
                            Rml::TextureHandle texture,
                            const Rml::Vector2f& translation) override;
                            
    // Called by RmlUi when it wants to compile geometry it believes will be static for the forseeable future.
    Rml::CompiledGeometryHandle CompileGeometry(Rml::Vertex* vertices, int num_vertices, int* indices, int num_indices, Rml::TextureHandle texture) override;

    // Called by RmlUi when it wants to render application-compiled geometry.
    void RenderCompiledGeometry(Rml::CompiledGeometryHandle geometry, const Rml::Vector2f& translation) override;

    // Called by RmlUi when it wants to release application-compiled geometry.
    void ReleaseCompiledGeometry(Rml::CompiledGeometryHandle geometry) override;

    // Called by RmlUi when it wants to enable or disable scissoring to clip content.
    void EnableScissorRegion(bool enable) override;

    // Called by RmlUi when it wants to change the scissor region.
    void SetScissorRegion(int x, int y, int width, int height) override;

    // Called by RmlUi when a texture is required by the library.
    bool LoadTexture(Rml::TextureHandle& texture_handle,
                            Rml::Vector2i& texture_dimensions,
                            const Rml::String& source) override;

    // Called by RmlUi when a texture is required to be built from an internally-generated sequence of pixels.
    bool GenerateTexture(Rml::TextureHandle& texture_handle,
                                const Rml::byte* source,
                                const Rml::Vector2i& source_dimensions) override;

    // Called by RmlUi when a loaded texture is no longer required.
    void ReleaseTexture(Rml::TextureHandle texture_handle) override;

    // Called by RmlUi when it wants the renderer to use a new transform matrix.
    // If no transform applies to the current element, nullptr is submitted. Then it expects the renderer to use
    // an identity matrix or otherwise omit the multiplication with the transform.
    void SetTransform(const Rml::Matrix4f* transform) override;
    
    Renderer* render;
    Image* default_image = NULL;

    mat4 current_transform = identity<mat4>();
    // bool has_scissors = true;
    VkRect2D last_scissors = {{0,0}, {1,1}};
};

//TODO: need to find memory, verify flags
template<class Elem_T> vector<Buffer> Renderer::create_elemBuffers(vector<Elem_T> elements, u32 buffer_usage){
    return create_elemBuffers<Elem_T>(elements.data(), elements.size(), buffer_usage);
}

template<class Elem_T> vector<Buffer> Renderer::create_elemBuffers(Elem_T* elements, u32 count, u32 buffer_usage){
    VkDeviceSize bufferSize = sizeof(Elem_T)*count;
    // VkDeviceSize bufferSizeI = sizeof(u32   )*icount;

    vector<Buffer> elems (MAX_FRAMES_IN_FLIGHT);
    // vector<Buffer>  indexes (MAX_FRAMES_IN_FLIGHT);

    VkBufferCreateInfo stagingBufferInfo = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        stagingBufferInfo.size = bufferSize;
        stagingBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    VmaAllocationCreateInfo stagingAllocInfo = {};
        stagingAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        stagingAllocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
        stagingAllocInfo.requiredFlags = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    VkBuffer stagingBuffer = {};
    VmaAllocation stagingAllocation = {};
    vmaCreateBuffer(VMAllocator, &stagingBufferInfo, &stagingAllocInfo, &stagingBuffer, &stagingAllocation, NULL);

    void* data;
assert (VMAllocator);
assert (stagingAllocation);
assert (&data);
    vmaMapMemory(VMAllocator, stagingAllocation, &data);
        memcpy(data, elements, bufferSize);
    vmaUnmapMemory(VMAllocator, stagingAllocation);

    for(i32 i=0; i<MAX_FRAMES_IN_FLIGHT; i++){
        VkBufferCreateInfo bufferInfo = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
            bufferInfo.size = bufferSize;
            bufferInfo.usage = buffer_usage;
        VmaAllocationCreateInfo allocInfo = {};
            allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
        vmaCreateBuffer(VMAllocator, &bufferInfo, &allocInfo, &elems[i].buffer, &elems[i].alloc, NULL);
               
        copy_Buffer(stagingBuffer, elems[i].buffer, bufferSize);
        // copy_Buffer(stagingBufferI,  indexes[i].buffer, bufferSizeI);
    }

    vmaDestroyBuffer(VMAllocator, stagingBuffer, stagingAllocation);
    // vmaDestroyBuffer(VMAllocator, stagingBufferI, stagingAllocationI);
// 
    return elems;
}