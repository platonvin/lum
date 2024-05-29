#pragma once

// #include "pch/render_pch.hpp"
#pragma once

#include <vector>
#include <tuple>

// #include <string.h>
#include <optional>
#include <stdio.h>

#include <Volk/volk.h>
#include <vulkan/vk_enum_string_helper.h>
#include <vma/vk_mem_alloc.h>
// #define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include <cassert>
#include <cmath>
#include <cstddef>
#include <cstring>
#include <glm/detail/qualifier.hpp>
#include <glm/ext/matrix_clip_space.hpp>
#include <glm/ext/matrix_projection.hpp>
#include <glm/ext/matrix_transform.hpp>
#include <glm/ext/vector_int3.hpp>
#include <glm/ext/vector_uint4.hpp>
#include <glm/trigonometric.hpp>
#include <io.h>
#include <vulkan/vulkan_core.h>
#include <set>
#include <iostream>
#include <limits>
#include <fstream>


#include <vulkan/vulkan_core.h>
#include <cstring>
#include <vector>

#include <vulkan/vulkan.h>
#include <VMA/vk_mem_alloc.h>
#include <glm/glm.hpp>
#include <glm/gtx/quaternion.hpp>

// #include <vulkan/vulkan_core.h>
// #include "primitives.hpp"
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

typedef   u8 MatID_t;
typedef  i32 BlockID_t;

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
    vector<VkBuffer> buf;
    vector<VmaAllocation> alloc;
} Buffers;
typedef struct Image {
    vector<VkImage> image;
    vector<VkImageView> view;
    vector<VmaAllocation> alloc;
} Images;

typedef struct Mesh {
    //everything is Staged per frame in flight, so you can update it faster. But costs double the memory
    //VkBuffers for ray generation
    Buffers vertexes;
    Buffers indexes;
    u32 icount;
    Images voxels; //3d image of voxels in this mesh, used to represent mesh to per-frame world voxel representation

    mat4 transform; //used to transform from self coordinate system to world coordinate system
    mat4 old_transform; //for denoising
    
    ivec3 size;
    // Voxel* host_voxels; // single copy of voxel memory on cpu. If NULL then you store your data yourself
} Mesh;


typedef struct Block {
    Voxel voxels[BLOCK_SIZE][BLOCK_SIZE][BLOCK_SIZE];
} Block;

//just 3d array. Content invalid after allocate()
template <typename Type> class table3d {
private:
    Type* memory = NULL;
    ivec3 _size = {};
public:
    //makes content invalid 
    void allocate(int x, int y, int z) {
        _size = ivec3(x,y,z);
        this->memory = (Type*)malloc(sizeof(Type)*x*y*z);
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
};

using namespace std;
using namespace glm;

// extern VisualWorld world;
#define STRINGIZE(x) STRINGIZE2(x)
#define STRINGIZE2(x) #x
#define __LINE_STRING__ STRINGIZE(__LINE__)

#define crash(string) do {printf(KRED "l:%d %s\n" KEND, __LINE__, string); exit(69);} while(0)

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
    vmaSetAllocationName(allocator, *pAllocation, #pAllocation __LINE_STRING__);\
    res;\
    }
#define vmaCreateBuffer(allocator, pImageCreateInfo, pAllocationCreateInfo, pImage, pAllocation, pAllocationInfo) {\
    VkResult res = (vmaCreateBuffer)(allocator, pImageCreateInfo, pAllocationCreateInfo, pImage, pAllocation, pAllocationInfo);\
    vmaSetAllocationName(allocator, *pAllocation, #pAllocation __LINE_STRING__);\
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
const int MAX_FRAMES_IN_FLIGHT = 2;

class Renderer {

public: 
    void init(int x_size=8, int y_size=8, int z_size=8, int block_palette_size=128, int copy_queue_size=1024);
    void cleanup();

    // sets voxels and size. By default uses first .vox palette as main palette
    void load_mesh(Mesh* mesh, const char* vox_file,                                 bool _make_vertices=true, bool extrude_palette=true);
    // sets voxels and size. By default uses first .vox palette as main palette
    void load_mesh(Mesh* mesh, Voxel* voxel_mem, int x_size, int y_size, int z_size, bool _make_vertices=true);
    void free_mesh(Mesh* mesh);
    void make_vertices(Mesh* mesh, Voxel* Voxels, int x_size, int y_size, int z_size);
    void load_vertices(Mesh* mesh, Vertex* vertices);
    // void extrude_palette(Material* material_palette);
    Material mat_palette[MATERIAL_PALETTE_SIZE];
private:
    ivec3(world_size);
    bool _has_palette = false;
public:
    void start_Frame();
        void startRaygen();
        void RaygenMesh(Mesh &mesh);
        void   endRaygen();
        // Start of computeCommandBuffer
        void startCompute();
                void startBlockify();
                void blockifyMesh(Mesh &mesh);
                void   endBlockify();
            void execCopies();
                void startMap();
                void mapMesh(Mesh &mesh);
                void   endMap();
            void raytrace();
        void   endCompute();
        // End of computeCommandBuffer
        void present();
        void   end_Present();
    void end_Frame();

    tuple<Buffers, Buffers> create_RayGen_VertexBuffers(Vertex* vertices, u32 vcount, u32* indices, u32 icount); 
    tuple<Buffers, Buffers> create_RayGen_VertexBuffers(vector<Vertex> vertices, vector<u32> indices); 
    Images  create_RayTrace_VoxelImages(MatID_t* voxels, ivec3 size);
    void update_Block_Palette(Block* blockPalette);
    void update_Material_Palette(Material* materialPalette);
    void update_SingleChunk(BlockID_t* blocks);
private:
    void recreate_Swapchain();

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
    void setup_Blockify_Descriptors();
    void setup_Copy_Descriptors();
    void setup_Map_Descriptors();
    void setup_Raytrace_Descriptors();
    void setup_Graphical_Descriptors();
    void setup_RayGen_Descriptors();
    void create_samplers();
    // void update_Descriptors();

    void create_Image_Storages(vector<VkImage> &images, vector<VmaAllocation> &allocs, vector<VkImageView> &views, 
    VkImageType type, VkFormat format, VkImageUsageFlags usage, VkImageAspectFlags aspect, VkImageLayout layout, VkPipelineStageFlagBits pipeStage, VkAccessFlags access, 
    uvec3 size);
    void create_Buffer_Storages(vector<VkBuffer> &buffers, vector<VmaAllocation> &allocs, VkBufferUsageFlags usage, u32 size, VkMemoryPropertyFlags required_flags = 0);
    void create_Blockify_Pipeline();
    void create_Copy_Pipeline();
    void create_Map_Pipeline();
    void create_Raytrace_Pipeline(); 
    VkShaderModule create_Shader_Module(vector<char>& code);
    //creates framebuffers that point to attachments view specified views
    void create_N_Framebuffers(vector<VkFramebuffer> &framebuffers, vector<vector<VkImageView>> &views, VkRenderPass renderPass, u32 N, u32 Width, u32 Height);
    void create_Command_Pool();
    void create_Command_Buffers(vector<VkCommandBuffer>& commandBuffers, u32 size);
    void record_Command_Buffer_Graphical(VkCommandBuffer commandBuffer, u32 imageIndex);
    void record_Command_Buffer_RayGen(VkCommandBuffer commandBuffer);
    void record_Command_Buffer_Compute  (VkCommandBuffer commandBuffer);
    void create_Sync_Objects();

    void create_Buffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
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

    VkShaderModule graphicsVertShaderModule;
    VkShaderModule graphicsFragShaderModule;

    VkShaderModule rayGenVertShaderModule;
    VkShaderModule rayGenFragShaderModule;
    VkShaderModule   blockifyShaderModule;
    VkShaderModule       copyShaderModule;
    VkShaderModule        mapShaderModule;
    VkShaderModule   raytraceShaderModule;

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
    vector<VkCommandBuffer>   computeCommandBuffers; //also used for blockify, copy and map. 
    vector<VkCommandBuffer> graphicalCommandBuffers;

    vector<VkSemaphore>   imageAvailableSemaphores;
    vector<VkSemaphore>   renderFinishedSemaphores;
    // vector<VkSemaphore> blockifyFinishedSemaphores;
    // vector<VkSemaphore> copyFinishedSemaphores;
    // vector<VkSemaphore> mapFinishedSemaphores;
    vector<VkSemaphore> raytraceFinishedSemaphores;
    vector<VkSemaphore>   rayGenFinishedSemaphores;
    vector<VkFence> graphicalInFlightFences;
    // vector<VkFence>  blockifyInFlightFences;
    // vector<VkFence>      copyInFlightFences;
    // vector<VkFence>       mapInFlightFences;
    vector<VkFence>  raytraceInFlightFences;
    vector<VkFence>    rayGenInFlightFences;    
    
    vector<VkImage>         rayGenPosMatImages;
    vector<VkImage>           rayGenNormImages;
    vector<VkImage>           rayGenPosDiffImages;
    vector<VkImage>          rayGenDepthImages;
    vector<VkImage>       raytraceBlocksImages;
    vector<VkImage>          originWorldImages; //TODO: make multiple chunks be translated to main world
    vector<VkImage> raytraceBlockPaletteImages;
    vector<VkImage>   originBlockPaletteImages;
    vector<VkImage> raytraceVoxelPaletteImages;
    vector<VkBuffer>       paletteCounterBuffers; //atomic
    vector<VkBuffer>          copyCounterBuffers; //atomic

    vector<VkBuffer> RayGenUniformBuffers;
    // vector<VkImage>   originVoxelPaletteImages; //unused - voxel mat palette does not change
    vector<VkImage>            raytracedImages;
    vector<VkImage>            swapChainImages;
    vector<VmaAllocation>          rayGenPosMatImageAllocations;
    vector<VmaAllocation>            rayGenNormImageAllocations;
    vector<VmaAllocation>            rayGenPosDiffImageAllocations;
    vector<VmaAllocation>           rayGenDepthImageAllocations;
    vector<VmaAllocation>        raytraceBlocksImageAllocations;
    vector<VmaAllocation>           originWorldImageAllocations;
    vector<VmaAllocation>  raytraceBlockPaletteImageAllocations;
    vector<VmaAllocation>    originBlockPaletteImageAllocations;
    vector<VmaAllocation>  raytraceVoxelPaletteImageAllocations;
    // vector<VmaAllocation>    originVoxelPaletteImageAllocations; //unused - voxel mat palette does not change
    vector<VmaAllocation>             raytracedImageAllocations;
    vector<VmaAllocation>        paletteCounterBufferAllocations;
    vector<VmaAllocation>           copyCounterBufferAllocations;
    vector<VmaAllocation>           RayGenUniformBufferAllocations;
    vector<VkImageView>         rayGenPosMatImageViews;
    vector<VkImageView>           rayGenNormImageViews;
    vector<VkImageView>           rayGenPosDiffImageViews;
    vector<VkImageView>          rayGenDepthImageViews;
    vector<VkImageView>       raytraceBlocksImageViews;
    vector<VkImageView>         originBlocksImageViews;
    vector<VkImageView> raytraceBlockPaletteImageViews;
    vector<VkImageView>   originBlockPaletteImageViews;
    vector<VkImageView> raytraceVoxelPaletteImageViews;
    // vector<VkImageView>   originVoxelPaletteImageViews; //unused - voxel mat palette does not change
    vector<VkImageView>            raytracedImageViews;
    vector<VkImageView>            swapChainImageViews;
    //buffers dont need view
    vector<VkSampler>  raytracedImageSamplers;
    
    vector<void*> RayGenUniformMapped;

    VkDescriptorSetLayout    RayGenDescriptorSetLayout;
    VkDescriptorSetLayout  raytraceDescriptorSetLayout;
    VkDescriptorSetLayout  blockifyDescriptorSetLayout;
    VkDescriptorSetLayout      copyDescriptorSetLayout;
    VkDescriptorSetLayout       mapDescriptorSetLayout;
    VkDescriptorSetLayout graphicalDescriptorSetLayout;
    VkDescriptorPool descriptorPool;
    
    //basically just MAX_FRAMES_IN_FLIGHT of same descriptors
    //raygen does not need this, for raygen its inside renderpass thing
    vector<VkDescriptorSet>  raytraceDescriptorSets;
    vector<VkDescriptorSet>  blockifyDescriptorSets;
    vector<VkDescriptorSet>      copyDescriptorSets;
    vector<VkDescriptorSet>       mapDescriptorSets;
    vector<VkDescriptorSet> graphicalDescriptorSets;
    vector<VkDescriptorSet>    RayGenDescriptorSets;

//compute pipeline things
    VkPipelineLayout raytraceLayout;
    VkPipelineLayout blockifyLayout;
    VkPipelineLayout     copyLayout;
    VkPipelineLayout      mapLayout;
    VkPipeline raytracePipeline;
    VkPipeline blockifyPipeline;
    VkPipeline     copyPipeline;
    VkPipeline      mapPipeline;

    VmaAllocator VMAllocator; 
private:
    u32 imageIndex = 0;

    //wraps around MAX_FRAMES_IN_FLIGHT
    u32 currentFrame = 0;
    // VisualWorld &_world = world;
    VkDebugUtilsMessengerEXT debugMessenger;
};

// void a();1

