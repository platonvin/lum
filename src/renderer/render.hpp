#pragma once

#include <Volk/volk.h>
#include <vulkan/vulkan.h>
#include <climits>
#include <cmath>
// #include <cstddef>
#include <string.h>
#include <glm/fwd.hpp>
#include <vector>
#include <tuple>

// #include <string.h>
#include <optional>
#include <stdio.h>

#include <vulkan/vk_enum_string_helper.h>
#include <vma/vk_mem_alloc.h>
// #define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

// #include <cassert>
#include <glm/detail/qualifier.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_projection.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/vector_int3.hpp>
#include <glm/ext/vector_uint4.hpp>
#include <glm/trigonometric.hpp>
#include <stdio.h>
#include <io.h>
// #include <vulkan/vulkan_core.h>
#include <set> 
#include <limits>
#include <fstream>

#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

#include <defines.hpp>

using namespace glm;
using namespace std;

//Everything is X -> Y -> Z order in arrays (vectors)
#define BLOCK_SIZE 16 // each block in common world is BLOCK_SIZE x BLOCK_SIZE x BLOCK_SIZE
#define MATERIAL_PALETTE_SIZE 256 //0 always empty

// on nvidia required 2d isntead of 1d cause VK_DEVICE_LOST on vkCmdCopy. FML
#define    BLOCK_PALETTE_SIZE_X 32
#define    BLOCK_PALETTE_SIZE_Y 32
#define    BLOCK_PALETTE_SIZE  (BLOCK_PALETTE_SIZE_X*BLOCK_PALETTE_SIZE_Y)

// #define STENCIL

// #define RAYTRACED_IMAGE_FORMAT VK_FORMAT_R16G16B16A16_UNORM
// #define RAYTRACED_IMAGE_FORMAT VK_FORMAT_R16G16B16A16_UNORM
#define RAYTRACED_IMAGE_FORMAT VK_FORMAT_R32G32B32A32_SFLOAT
#define  FINAL_IMAGE_FORMAT VK_FORMAT_R16G16B16A16_UNORM
#define OLD_UV_FORMAT VK_FORMAT_R32G32_SFLOAT
#define DEPTH_LAYOUT VK_IMAGE_LAYOUT_GENERAL
#ifdef STENCIL
#define DEPTH_FORMAT VK_FORMAT_D24_UNORM_S8_UINT 
#else
#define DEPTH_FORMAT VK_FORMAT_D32_SFLOAT
#endif

//should be 2 for temporal accumulation, but can be changed if rebind images
const int MAX_FRAMES_IN_FLIGHT = 2;

typedef   u8 MatID_t;
typedef  i16 BlockID_t;

typedef struct Material {
    // glm::vec4<u8> casd;
    vec4 color; //colo   r AND transparancy
    f32 emmit; //emmits same color as reflect
    f32 rough;
} Material;
typedef u8 Voxel;
typedef struct Vertex {
    vec3 pos;
    vec3 norm;
    MatID_t matID;
} Vertex;
// typedef struct MeshData {
//     vector<VkBuffer> vertices;
//     vector<VkBuffer>  indices;
//     vector<VmaAllocation> verticesAllocation;
//     vector<VmaAllocation>  indicesAllocation;
//     u32 icount;
// } MeshData;
// typedef struct VoxelData {
//     vector<VkBuffer> vertices;
//     vector<VkBuffer>  indices;
//     vector<VmaAllocation> verticesAllocation;
//     vector<VmaAllocation>  indicesAllocation;
//     u32 icount;
// } MeshData;
typedef struct Buffer {
    VkBuffer buffer;
    VmaAllocation alloc;
} Buffer;
typedef struct Image {
    VkImage image;
    VkImageView view;
    VmaAllocation alloc;
} Image;

typedef struct Mesh {
    //everything is Staged per frame in flight, so you can update it faster. But costs double the memory
    //VkBuffers for ray generation
    vector<Buffer> vertexes;
    vector<Buffer> indexes;
    u32 icount;
    vector<Image> voxels; //3d image of voxels in this mesh, used to represent mesh to per-frame world voxel representation

    mat4 transform; //used to transform from self coordinate system to world coordinate system
    mat4 old_transform; //for denoising
    
    ivec3 size;
    // Voxel* host_voxels; // single copy of voxel memory on cpu. If NULL then you store your data yourself
} Mesh;


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

using namespace std;
using namespace glm;

// extern VisualWorld world;
#define STRINGIZE(x) STRINGIZE2(x)
#define STRINGIZE2(x) #x
#define __LINE_STRING__ STRINGIZE(__LINE__)

#define crash(string) do {printf(KRED "l:%d %s\n" KEND, __LINE__, #string); exit(69);} while(0)

// #define NDEBUG
#ifdef NDEBUG
// #define VMA_NAME(allocation) 
#define VKNDEBUG
#define VK_CHECK(func) do{\
    VkResult result = func;\
    if (result != VK_SUCCESS) {\
        exit(result);\
    }} while (0)
#else
#define VK_CHECK(func) do{\
    VkResult result = (func);\
    if (result != VK_SUCCESS) {\
        fprintf(stderr, "LINE :%d Vulkan call failed: %s\n", __LINE__, string_VkResult(result));\
        exit(1);\
    }} while (0)
#define malloc(x)   ({void* res_ptr =(malloc)(x  ); assert(res_ptr!=NULL && __LINE_STRING__); res_ptr;})
#define calloc(x,y) ({void* res_ptr =(calloc)(x,y); assert(res_ptr!=NULL && __LINE_STRING__); res_ptr;})
#endif

#ifdef SET_ALLOC_NAMES
#define vmaCreateImage(allocator, pImageCreateInfo, pAllocationCreateInfo, pImage, pAllocation, pAllocationInfo) {\
    VkResult res = (vmaCreateImage)(allocator, pImageCreateInfo, pAllocationCreateInfo, pImage, pAllocation, pAllocationInfo);\
    vmaSetAllocationName(allocator,* pAllocation, #pAllocation __LINE_STRING__);\
    res;\
    }
#define vmaCreateBuffer(allocator, pImageCreateInfo, pAllocationCreateInfo, pImage, pAllocation, pAllocationInfo) {\
    VkResult res = (vmaCreateBuffer)(allocator, pImageCreateInfo, pAllocationCreateInfo, pImage, pAllocation, pAllocationInfo);\
    vmaSetAllocationName(allocator,* pAllocation, #pAllocation __LINE_STRING__);\
    res;\
    }
#endif

typedef struct Window{
    GLFWwindow* pointer;
    int width;
    int height;
} Window;

typedef struct QueueFamilyIndices {
    optional<u32> graphicalAndCompute;
    optional<u32> present;
    // optional<u32> compute;

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

class Renderer {

public: 
    void init(int x_size=8, int y_size=8, int z_size=8, int block_palette_size=128, int copy_queue_size=1024, vec2 ratio = vec2(1.0), bool vsync=true, bool fullscreen = false);
    void cleanup();

    // sets voxels and size. By default uses first .vox palette as main palette
    void load_mesh(Mesh* mesh, const char* vox_file,                                 bool _make_vertices=true, bool extrude_palette=true);
    // sets voxels and size. By default uses first .vox palette as main palette
    void load_mesh(Mesh* mesh, Voxel* voxel_mem, int x_size, int y_size, int z_size, bool _make_vertices=true);
    void free_mesh(Mesh* mesh);
    void make_vertices(Mesh* mesh, Voxel* Voxels, int x_size, int y_size, int z_size);
    void load_vertices(Mesh* mesh, Vertex* vertices);
    // void extrude_palette(Material* material_palette);
    void load_block(Block** block, const char* vox_file);
    void free_block(Block** block);
    
    Material mat_palette[MATERIAL_PALETTE_SIZE];
    table3d<BlockID_t>  origin_world = {};
    table3d<BlockID_t> current_world = {};
private:
    ivec3 world_size;
    bool has_palette = false;
    vec2 _ratio;
    vector<VkImageCopy> copy_queue = {};
public:
    bool is_scaled = false;
    bool is_vsync = false;
    bool is_fullscreen = false;

    bool is_resized = false;

    vec3     camera_pos = vec3(60, 0, 50);
    vec3 old_camera_pos = vec3(60, 0, 50);
    vec3     camera_dir = normalize(vec3(0.1, 1.0, -0.5));
    vec3 old_camera_dir = normalize(vec3(0.1, 1.0, -0.5));

    mat4 current_trans = identity<mat4>();
    mat4     old_trans = identity<mat4>();
    // vec3 old_camera_pos = vec3(60, 0, 50);
    // vec3 old_camera_dir = normalize(vec3(0.1, 1.0, -0.5));

    void start_Frame();
        void startRaygen();
        void RaygenMesh(Mesh* mesh);
        void   endRaygen();
        void RaygenParticles();
        // Start of computeCommandBuffer
        void startCompute();
                void startBlockify();
                void blockifyMesh(Mesh* mesh);
                    void blockifyCustom(void* ptr); // just in case you have custom blockify algorithm. If using this, no startBlockify needed
                void   endBlockify();
                void blockifyParticles();
            void execCopies();
                void startMap();
                void mapMesh(Mesh* mesh);
                void   endMap();
                void mapParticles();
            void recalculate_df();
            void raytrace();
            void denoise(int iterations);
            void upscale();
        void   endCompute();
        // End of computeCommandBuffer
        void present();
        void   end_Present();
    void end_Frame();

    tuple<vector<Buffer>, vector<Buffer>> create_RayGen_VertexBuffers(Vertex* vertices, u32 vcount, u32* indices, u32 icount); 
    tuple<vector<Buffer>, vector<Buffer>> create_RayGen_VertexBuffers(vector<Vertex> vertices, vector<u32> indices); 
    vector<Image>  create_RayTrace_VoxelImages(u16* voxels, ivec3 size);
    void update_Block_Palette(Block** blockPalette);
    void update_Material_Palette(Material* materialPalette);
    void update_SingleChunk(BlockID_t* blocks);
private:
    //including fullscreen and scaled images
    void  cleanup_SwapchainDependent();
    //including fullscreen and scaled images
    void   create_SwapchainDependent();
    //including fullscreen and scaled images
    void  recreate_SwapchainDependent();

    void  cleanup_Images();


    void create_Allocator();
    
    void create_Window();
    void create_Instance();
    void setup_Debug_Messenger();
    void create_Surface();

    SwapChainSupportDetails query_Swapchain_Support(VkPhysicalDevice);
    QueueFamilyIndices find_Queue_Families(VkPhysicalDevice);
    bool check_Format_Support(VkPhysicalDevice device, VkFormat format, VkFormatFeatureFlags features);
    bool is_PhysicalDevice_Suitable(VkPhysicalDevice);
    //call get_PhysicalDevice_Extensions first
    bool check_PhysicalDevice_Extension_Support(VkPhysicalDevice);
    void pick_Physical_Device();
    void create_Logical_Device();
    void create_Swapchain();
    VkSurfaceFormatKHR choose_Swap_SurfaceFormat(vector<VkSurfaceFormatKHR> availableFormats);
    VkPresentModeKHR choose_Swap_PresentMode(vector<VkPresentModeKHR> availablePresentModes);
    VkExtent2D choose_Swap_Extent(VkSurfaceCapabilitiesKHR capabilities);
    void create_Swapchain_Image_Views();
    void create_RenderPass_Graphical();
    void create_RenderPass_RayGen();
    void create_Graphics_Pipeline(); 
    void create_RayGen_Pipeline();
    void create_Descriptor_Set_Layouts();
    void create_Descriptor_Pool();
    void allocate_Descriptors();

    //not possible without loosing perfomance
    // void setup_descriptors_helper(vector<VkDescriptorSet>* descriptor_sets, vector<Buffers> buffers, vector<Images> images);

    void setup_Blockify_Descriptors();
    void setup_Copy_Descriptors();
    void setup_Map_Descriptors();
    void setup_Df_Descriptors();
    void setup_Raytrace_Descriptors();
    void setup_Denoise_Descriptors();
    void setup_Upscale_Descriptors();
    void setup_Graphical_Descriptors();
    void setup_RayGen_Descriptors();
    void create_samplers();
    // void update_Descriptors();

    void delete_Images(vector<Image>* images);
    void delete_Images(Image* images);
    void create_Image_Storages(vector<Image>* images, 
    VkImageType type, VkFormat format, VkImageUsageFlags usage, VmaMemoryUsage vma_usage, VmaAllocationCreateFlags vma_flags, VkImageAspectFlags aspect, VkImageLayout layout, VkPipelineStageFlags pipeStage, VkAccessFlags access, 
    uvec3 size);
    void create_Image_Storages(Image* image, 
    VkImageType type, VkFormat format, VkImageUsageFlags usage, VmaMemoryUsage vma_usage, VmaAllocationCreateFlags vma_flags, VkImageAspectFlags aspect, VkImageLayout layout, VkPipelineStageFlags pipeStage, VkAccessFlags access, 
    uvec3 size);
// #ifdef STENCIL 
    void create_Image_Storages_DepthStencil(vector<VkImage>* images, vector<VmaAllocation>* allocs, vector<VkImageView>* depthViews, vector<VkImageView>* stencilViews,
    VkImageType type, VkFormat format, VkImageUsageFlags usage, VmaMemoryUsage vma_usage, VmaAllocationCreateFlags vma_flags, VkImageAspectFlags aspect, VkImageLayout layout, VkPipelineStageFlags pipeStage, VkAccessFlags access, 
    uvec3 size);
// #endif

    // void destroy_images
    void create_Buffer_Storages(vector<Buffer>* buffers, VkBufferUsageFlags usage, u32 size, bool host = false);
    void create_compute_pipelines_helper(const char* name, VkDescriptorSetLayout  descriptorSetLayout, VkPipelineLayout* pipelineLayout, VkPipeline* pipeline, u32 push_size);
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

    void create_Buffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer* buffer, VkDeviceMemory* bufferMemory);
    void copy_Buffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size); 
    void copy_Buffer(VkBuffer srcBuffer, VkImage dstImage, uvec3 size, VkImageLayout layout);
    u32 find_Memory_Type(u32 typeFilter, VkMemoryPropertyFlags properties);
    VkCommandBuffer begin_Single_Time_Commands();
    void end_Single_Time_Commands(VkCommandBuffer commandBuffer);
    void transition_Image_Layout_Singletime(VkImage image, VkFormat format, VkImageAspectFlags aspect,VkImageLayout oldLayout, VkImageLayout newLayout,
        VkPipelineStageFlags sourceStage, VkPipelineStageFlags destinationStage, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask);
    void transition_Image_Layout_Cmdb(VkCommandBuffer commandBuffer, VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout,
        VkPipelineStageFlags sourceStage, VkPipelineStageFlags destinationStage, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask);
    void copy_Whole_Image(VkExtent3D extent, VkCommandBuffer cmdbuf, VkImage src, VkImage dst);
    
    void get_Instance_Extensions();
    // void get_PhysicalDevice_Extensions();
public:
    

    Window window;
    VkInstance instance;
    //picked one. Maybe will be multiple in future 
    VkPhysicalDevice physicalDevice; 
    VkDevice device;
    VkSurfaceKHR surface;

    QueueFamilyIndices FamilyIndices;
    VkQueue graphicsQueue;
    VkQueue presentQueue;
    VkQueue computeQueue;
    VkCommandPool commandPool;

    VkSwapchainKHR swapchain;
    VkFormat   swapChainImageFormat;
    VkExtent2D swapChainExtent;
    VkExtent2D  raytraceExtent;

    VkShaderModule graphicsVertShaderModule;
    VkShaderModule graphicsFragShaderModule;

    VkShaderModule rayGenVertShaderModule;
    VkShaderModule rayGenFragShaderModule;
    // VkShaderModule   blockifyShaderModule;
    // VkShaderModule       copyShaderModule;
    // VkShaderModule        mapShaderModule;
    // VkShaderModule         dfShaderModule;
    // VkShaderModule   raytraceShaderModule;

//rasterization pipeline things
    VkRenderPass    rayGenRenderPass;
    VkRenderPass graphicalRenderPass;
    VkPipelineLayout rayGenPipelineLayout;
    VkPipelineLayout       graphicsLayout;
    VkPipeline   rayGenPipeline;
    VkPipeline graphicsPipeline;


    vector<VkFramebuffer> swapChainFramebuffers;
    vector<VkFramebuffer>    rayGenFramebuffers;

    vector<VkCommandBuffer>    rayGenCommandBuffers;
    //It is so because they are too similar and easy to record (TODO: make concurent)
    vector<VkCommandBuffer>   computeCommandBuffers; 
    vector<VkCommandBuffer> graphicalCommandBuffers;

    vector<VkSemaphore>   imageAvailableSemaphores;
    vector<VkSemaphore>   renderFinishedSemaphores;
    vector<VkSemaphore> raytraceFinishedSemaphores;
    vector<VkSemaphore>   rayGenFinishedSemaphores;
    vector<VkFence> graphicalInFlightFences;
    vector<VkFence>  raytraceInFlightFences;
    vector<VkFence>    rayGenInFlightFences;    
    
    //g buffer of prev_pixel pos, matid and normal
    // vector<VkImage>           rayGenNormImages;
    // vector<VkImage>           rayGenPosDiffImages;
    //TODO: no copy if supported

    // vector<Image> depth ; //copy to this
    // vector<Image> depth_downscaled;

    //if scaled, we render to matnorm and downscale
    //if not scaled, we render to  matnorm_downscaled and just use it with nearest sampler
    
    // vector<Image> oldUv;
    // vector<Image> oldUv_downscaled;
    // vector<Image> oldUv_downscaled;
    // vector<VkSampler> oldUvSampler;

    // vector<Image> gBuffer;
    // vector<Image> gBuffer_downscaled;
    // vector<Image> step_count;
    // vector<Image> raytraced_frame;
    // vector<Image>  denoised_frame;
    // vector<Image>  upscaled_frame;
#ifdef STENCIL
    vector<VkImage> depthStencilImages; //used for depth testing
    vector<VmaAllocation> depthStencilAllocs; //used for depth testing
    vector<VkImageView>   depthViews; //used for depth testing
    vector<VkImageView> stencilViews; //used for depth testing
#else
    vector<Image> depthBuffer; //used for depth testing
#endif
           Image  matNorm; //render always to one, other stored downscaled
    vector<Image> matNorm_downscaled;
           Image oldUv;
           Image step_count;
    vector<Image>          frame;
           Image  upscaled_frame;
    vector<Image> swapchain_images;
    VkSampler  nearestSampler;
    VkSampler  linearSampler;

    
    //is or might be in use when cpu is recording new one. Is pretty cheap, so just leave it
    vector<Buffer>      staging_world;
    vector<void*>       staging_world_mapped;

           Image                world; //can i really use just one?
           
    vector<Image> origin_block_palette;
           Image        block_palette;
    vector<Image> material_palette;
    
    vector<Buffer>         uniform;
    
    // vector<Buffer> 
    // vector<Buffer>       depthStaging; //atomic
    // vector<VmaAllocation>        paletteCounterBufferAllocations;

    // vector<Buffer> copy_queue;
    // vector<VkBuffer>          copyCounterBuffers; //atomic
    // vector<VmaAllocation>           copyCounterBufferAllocations;

    // vector<Buffer> uniform;
    // vector<Buffer> staging_uniform;
    // vector<void*>  staging_uniform_mapped;
    // vector<VmaAllocation>           RayGenUniformBufferAllocations;

    //g buffer of prev_pixel pos, matid and normal
    // vector<VkImageView>           rayGenNormImageViews;
    // vector<VkImageView>           rayGenPosDiffImageViews;
    // vector<VkImageView>         originBlocksImageViews;
    // vector<VkImageView>   originVoxelPaletteImageViews; //unused - voxel mat palette does not change

    //buffers dont need view
    

    VkDescriptorSetLayout    RayGenDescriptorSetLayout;
    vector<VkDescriptorSet>    RayGenDescriptorSets;

    VkDescriptorSetLayout  raytraceDescriptorSetLayout;
    vector<VkDescriptorSet>  raytraceDescriptorSets;

    VkDescriptorSetLayout  denoiseDescriptorSetLayout;
    vector<VkDescriptorSet>  denoiseDescriptorSets;

    VkDescriptorSetLayout  upscaleDescriptorSetLayout;
    vector<VkDescriptorSet>  upscaleDescriptorSets;

    // VkDescriptorSetLayout  blockifyDescriptorSetLayout;
    // vector<VkDescriptorSet>  blockifyDescriptorSets;

    // VkDescriptorSetLayout      copyDescriptorSetLayout;
    // vector<VkDescriptorSet>      copyDescriptorSets;

    VkDescriptorSetLayout       mapDescriptorSetLayout;
    vector<VkDescriptorSet>       mapDescriptorSets;

    // VkDescriptorSetLayout        dfDescriptorSetLayout;
    // vector<VkDescriptorSet>        dfDescriptorSets;

    VkDescriptorSetLayout graphicalDescriptorSetLayout;
    vector<VkDescriptorSet> graphicalDescriptorSets;

    VkDescriptorPool descriptorPool;
    
    //basically just MAX_FRAMES_IN_FLIGHT of same descriptors
    //raygen does not need this, for raygen its inside renderpass thing
// upscale denoise
//compute pipeline things
    VkPipelineLayout raytraceLayout;
    VkPipelineLayout denoiseLayout;
    VkPipelineLayout upscaleLayout;
    // VkPipelineLayout blockifyLayout;
    // VkPipelineLayout     copyLayout;
    VkPipelineLayout      mapLayout;
    // VkPipelineLayout       dfxLayout;
    // VkPipelineLayout       dfyLayout;
    // VkPipelineLayout       dfzLayout;
    VkPipeline raytracePipeline;
    VkPipeline denoisePipeline;
    VkPipeline upscalePipeline;
    // VkPipeline blockifyPipeline;
    // VkPipeline     copyPipeline;
    VkPipeline      mapPipeline;
    // VkPipeline       dfxPipeline;
    // VkPipeline       dfyPipeline;
    // VkPipeline       dfzPipeline;

    VmaAllocator VMAllocator; 
private:
    u32 imageIndex = 0;

    //wraps around MAX_FRAMES_IN_FLIGHT
    int itime = 0;
    u32  currentFrame = 0;
    u32 previousFrame = 0;
    u32     nextFrame = 0;
    int palette_counter = 0;
    // VisualWorld* _world = world;
    VkDebugUtilsMessengerEXT debugMessenger;
};

// void a();1

