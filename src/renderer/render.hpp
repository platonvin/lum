#pragma once

#include <vector>
#include <tuple>

#include <string.h>
#include <optional>
#include <stdio.h>

#include <Volk/volk.h>
#include <vulkan/vk_enum_string_helper.h>
#include <vma/vk_mem_alloc.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include "vulkan/vulkan_core.h"
#include "window.hpp"
#include "primitives.hpp"
// #include "visible_world.hpp"
#include <defines.hpp>

using namespace std;
using namespace glm;

// extern VisualWorld world;

#ifdef NDEBUG
#define VKNDEBUG
#endif

// #define VKNDEBUG

#ifdef NDEBUG
#define VK_CHECK(func)                                                                        \
do {                                                                                          \
    VkResult result = func;                                                                   \
    if (result != VK_SUCCESS) {                                                               \
        exit(result);                                                                         \
    }                                                                                         \
} while (0)
#else
#define VK_CHECK(func)                                                                        \
do {                                                                                          \
    VkResult result = func;                                                                   \
    if (result != VK_SUCCESS) {                                                               \
        fprintf(stderr, "LINE %d: Vulkan call failed: %s\n", __LINE__, string_VkResult(result));\
        exit(1);                                                                              \
    }                                                                                         \
} while (0)
#endif

// typedef struct FamilyIndex {
//     bool exists;
//     u32 index;
// } FamilyIndex;

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

const int MAX_FRAMES_IN_FLIGHT = 2;

class Renderer {
public:
    void init(){
    // void init(VisualWorld &_world){
        // _world = world;
        create_Window();

        VK_CHECK(volkInitialize());

        get_Layers();
        get_Instance_Extensions();
        get_PhysicalDevice_Extensions();

        create_Instance();
        volkLoadInstance(instance);   
        
#ifndef VKNDEBUG
        setup_Debug_Messenger();
#endif

        create_Surface();
        pick_Physical_Device();
        create_Logical_Device();

        //AMD VMA 
        create_Allocator();
        
        create_Swapchain();
        create_Swapchain_Image_Views();

        create_RenderPass_Graphical();
        create_RenderPass_RayGen();

        create_Command_Pool();
        create_Command_Buffers(graphicalCommandBuffers, MAX_FRAMES_IN_FLIGHT);
        create_Command_Buffers(   rayGenCommandBuffers, MAX_FRAMES_IN_FLIGHT);
        create_Command_Buffers(  computeCommandBuffers, MAX_FRAMES_IN_FLIGHT);

        create_samplers();
    
        create_Image_Storages(raytracedImages, raytracedImageAllocations, raytracedImageViews,
            VK_IMAGE_TYPE_2D,
            VK_FORMAT_R8G8B8A8_UNORM,
            VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_IMAGE_ASPECT_COLOR_BIT,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_ACCESS_SHADER_WRITE_BIT,
            {swapChainExtent.height, swapChainExtent.width, 1});
        create_Image_Storages(rayGenPosMatImages, rayGenPosMatImageAllocations, rayGenPosMatImageViews,
            VK_IMAGE_TYPE_2D,
            VK_FORMAT_R32G32B32A32_SFLOAT,
            VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_IMAGE_ASPECT_COLOR_BIT,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_ACCESS_SHADER_WRITE_BIT,
            {swapChainExtent.height, swapChainExtent.width, 1});
        create_Image_Storages(rayGenNormImages, rayGenNormImageAllocations, rayGenNormImageViews,
            VK_IMAGE_TYPE_2D,
            VK_FORMAT_R32G32B32A32_SFLOAT,
            VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_IMAGE_ASPECT_COLOR_BIT,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_ACCESS_SHADER_WRITE_BIT,
            {swapChainExtent.height, swapChainExtent.width, 1});
        create_Image_Storages(rayGenDepthImages, rayGenDepthImageAllocations, rayGenDepthImageViews,
            VK_IMAGE_TYPE_2D,
            VK_FORMAT_D32_SFLOAT,
            VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
            VK_IMAGE_ASPECT_DEPTH_BIT,
            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
            VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
            VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
            {swapChainExtent.height, swapChainExtent.width, 1});
        create_Image_Storages(originBlocksImages, originBlocksImageAllocations, originBlocksImageViews,
            VK_IMAGE_TYPE_3D,
            VK_FORMAT_R32_SINT,
            VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
            VK_IMAGE_ASPECT_COLOR_BIT,
            VK_IMAGE_LAYOUT_GENERAL,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT,
            {8, 8, 8}); //TODO: dynamic
        create_Image_Storages(raytraceBlocksImages, raytraceBlocksImageAllocations, raytraceBlocksImageViews,
            VK_IMAGE_TYPE_3D,
            VK_FORMAT_R32_SINT,
            VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
            VK_IMAGE_ASPECT_COLOR_BIT,
            VK_IMAGE_LAYOUT_GENERAL,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT,
            {8, 8, 8}); //TODO: dynamic
        create_Image_Storages(originBlockPaletteImages, originBlockPaletteImageAllocations, originBlockPaletteImageViews,
            VK_IMAGE_TYPE_3D,
            VK_FORMAT_R8_UINT,
            VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
            VK_IMAGE_ASPECT_COLOR_BIT,
            VK_IMAGE_LAYOUT_GENERAL,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
            {16, 16, 16*BLOCK_PALETTE_SIZE}); //TODO: dynamic
        create_Image_Storages(raytraceBlockPaletteImages, raytraceBlockPaletteImageAllocations, raytraceBlockPaletteImageViews,
            VK_IMAGE_TYPE_3D,
            VK_FORMAT_R8_UINT,
            VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
            VK_IMAGE_ASPECT_COLOR_BIT,
            VK_IMAGE_LAYOUT_GENERAL,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
            {16, 16, 16*BLOCK_PALETTE_SIZE}); //TODO: dynamic
        create_Image_Storages(raytraceVoxelPaletteImages, raytraceVoxelPaletteImageAllocations, raytraceVoxelPaletteImageViews,
            VK_IMAGE_TYPE_2D,
            VK_FORMAT_R32_SFLOAT, //try R32G32
            VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_IMAGE_ASPECT_COLOR_BIT,
            VK_IMAGE_LAYOUT_GENERAL,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_MEMORY_READ_BIT ,
            {256, 6, 1}); //TODO: dynamic, text formats, pack Material into smth other than 6 ortogonal floats :)

        // create_Buffer_Storages(vector<VkBuffer> &buffers, vector<VmaAllocation> &allocs, VkBufferUsageFlags usage, u32 size)
        create_Buffer_Storages(paletteCounterBuffers, paletteCounterBufferAllocations,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            sizeof(int));
        create_Buffer_Storages(copyCounterBuffers, copyCounterBufferAllocations,
            VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
            sizeof(int)*(3+1024));
        
        vector<vector<VkImageView>> swapViews = {swapChainImageViews};
        create_N_Framebuffers(swapChainFramebuffers, swapViews, graphicalRenderPass, swapChainImages.size(), swapChainExtent.width, swapChainExtent.height);
// printl(swapChainImages.size());
        vector<vector<VkImageView>> rayGenVeiws = {rayGenPosMatImageViews, rayGenNormImageViews, rayGenDepthImageViews};
        create_N_Framebuffers(rayGenFramebuffers, rayGenVeiws, rayGenRenderPass, MAX_FRAMES_IN_FLIGHT, swapChainExtent.width, swapChainExtent.height);
        
        create_Descriptor_Set_Layouts();
        create_Descriptor_Pool();
        allocate_Descriptors();
        setup_Blockify_Descriptors();
        setup_Copy_Descriptors();
        //setup for map?
        setup_Raytrace_Descriptors();
        setup_Graphical_Descriptors();
        // setup_RayGen_Descriptors();
        create_Blockify_Pipeline();
        create_Copy_Pipeline();
        create_Map_Pipeline();
        create_Raytrace_Pipeline();
        create_Graphics_Pipeline();
        create_RayGen_Pipeline();
        create_Sync_Objects();
    }
    void cleanup(){
        for (int i=0; i<MAX_FRAMES_IN_FLIGHT; i++){
            vkDestroyImageView(device,             raytracedImageViews[i], NULL);
            vkDestroyImageView(device,            rayGenNormImageViews[i], NULL);
            vkDestroyImageView(device,           rayGenDepthImageViews[i], NULL);
            vkDestroyImageView(device,          rayGenPosMatImageViews[i], NULL);
            vkDestroyImageView(device,          originBlocksImageViews[i], NULL);
            vkDestroyImageView(device,        raytraceBlocksImageViews[i], NULL);
            vkDestroyImageView(device,    originBlockPaletteImageViews[i], NULL);
            vkDestroyImageView(device,  raytraceBlockPaletteImageViews[i], NULL);
            vkDestroyImageView(device,  raytraceVoxelPaletteImageViews[i], NULL);
            vmaDestroyImage(VMAllocator,            raytracedImages[i],            raytracedImageAllocations[i]);
            vmaDestroyImage(VMAllocator,           rayGenNormImages[i],           rayGenNormImageAllocations[i]);
            vmaDestroyImage(VMAllocator,          rayGenDepthImages[i],          rayGenDepthImageAllocations[i]);
            vmaDestroyImage(VMAllocator,         rayGenPosMatImages[i],         rayGenPosMatImageAllocations[i]);
            vmaDestroyImage(VMAllocator,         originBlocksImages[i],         originBlocksImageAllocations[i]);
            vmaDestroyImage(VMAllocator,       raytraceBlocksImages[i],       raytraceBlocksImageAllocations[i]);
            vmaDestroyImage(VMAllocator,   originBlockPaletteImages[i]  , originBlockPaletteImageAllocations[i]);
            vmaDestroyImage(VMAllocator, raytraceBlockPaletteImages[i], raytraceBlockPaletteImageAllocations[i]);
            vmaDestroyImage(VMAllocator, raytraceVoxelPaletteImages[i], raytraceVoxelPaletteImageAllocations[i]);
            vkDestroySampler(device, raytracedImageSamplers[i], NULL);
            
            vmaDestroyBuffer(VMAllocator, paletteCounterBuffers[i], paletteCounterBufferAllocations[i]);
            vmaDestroyBuffer(VMAllocator, copyCounterBuffers[i], copyCounterBufferAllocations[i]);

            vkDestroyFramebuffer(device, rayGenFramebuffers[i], NULL);
        }

        vkDestroyDescriptorPool(device, descriptorPool, NULL);
        vkDestroyDescriptorSetLayout(device,   raytraceDescriptorSetLayout, NULL);
        vkDestroyDescriptorSetLayout(device,   blockifyDescriptorSetLayout, NULL);
        vkDestroyDescriptorSetLayout(device,   copyDescriptorSetLayout, NULL);
        vkDestroyDescriptorSetLayout(device,   mapDescriptorSetLayout, NULL);
        vkDestroyDescriptorSetLayout(device, graphicalDescriptorSetLayout, NULL);
        for (int i=0; i < MAX_FRAMES_IN_FLIGHT; i++){
            vkDestroySemaphore(device,  imageAvailableSemaphores[i], NULL);
            vkDestroySemaphore(device,  renderFinishedSemaphores[i], NULL);
            // vkDestroySemaphore(device, blockifyFinishedSemaphores[i], NULL);
            // vkDestroySemaphore(device, copyFinishedSemaphores[i], NULL);
            // vkDestroySemaphore(device, mapFinishedSemaphores[i], NULL);
            vkDestroySemaphore(device, raytraceFinishedSemaphores[i], NULL);
            vkDestroySemaphore(device,  rayGenFinishedSemaphores[i], NULL);
            vkDestroyFence(device, graphicalInFlightFences[i], NULL);
            // vkDestroyFence(device,   blockifyInFlightFences[i], NULL);
            // vkDestroyFence(device,   copyInFlightFences[i], NULL);
            // vkDestroyFence(device,   mapInFlightFences[i], NULL);
            vkDestroyFence(device,   raytraceInFlightFences[i], NULL);
            vkDestroyFence(device,    rayGenInFlightFences[i], NULL);
        }
        vkDestroyCommandPool(device, commandPool, NULL);
        for (auto framebuffer : swapChainFramebuffers) {
            vkDestroyFramebuffer(device, framebuffer, NULL);
        }
        vkDestroyRenderPass(device, graphicalRenderPass, NULL);
        vkDestroyRenderPass(device, rayGenRenderPass, NULL);

        vkDestroyPipeline(device,  raytracePipeline, NULL);
        vkDestroyPipeline(device, graphicsPipeline, NULL);
        vkDestroyPipeline(device,   rayGenPipeline, NULL);

        vkDestroyPipelineLayout(device, raytraceLayout, NULL);
        vkDestroyPipelineLayout(device, graphicsLayout, NULL);
        vkDestroyPipelineLayout(device, rayGenPipelineLayout, NULL);

        vkDestroyShaderModule(device, rayGenVertShaderModule, NULL);
        vkDestroyShaderModule(device, rayGenFragShaderModule, NULL); 
        vkDestroyShaderModule(device, raytraceShaderModule, NULL); 
        vkDestroyShaderModule(device, graphicsVertShaderModule, NULL);
        vkDestroyShaderModule(device, graphicsFragShaderModule, NULL); 
        for (auto imageView : swapChainImageViews) {
            vkDestroyImageView(device, imageView, NULL);
        }
        vkDestroySwapchainKHR(device, swapchain, NULL);
        //do before destroyDevice
        vmaDestroyAllocator(VMAllocator);
        vkDestroyDevice(device, NULL);
        //"unpicking" physical device is unnessesary :)
        vkDestroySurfaceKHR(instance, surface, NULL);
#ifndef VKNDEBUG
        vkDestroyDebugUtilsMessengerEXT(instance, debugMessenger, NULL);
#endif
        vkDestroyInstance(instance, NULL);
        glfwDestroyWindow(window.pointer);
    }

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

    tuple<Buffer, Buffer> create_RayGen_VertexBuffers(vector<Vertex> vertices, vector<u32> indices);
    Image  create_RayTrace_VoxelImages(MatID_t* voxels, ivec3 size);
    void update_Block_Palette(Block* blockPalette);
    void update_Voxel_Palette(Material* materialPalette);
    void update_Blocks(BlockID_t* blocks);
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
    void create_samplers();
    // void update_Descriptors();

    void create_Image_Storages(vector<VkImage> &images, vector<VmaAllocation> &allocs, vector<VkImageView> &views, 
    VkImageType type, VkFormat format, VkImageUsageFlags usage, VkImageAspectFlags aspect, VkImageLayout layout, VkPipelineStageFlagBits pipeStage, VkAccessFlags access, 
    uvec3 hwd);
    void create_Buffer_Storages(vector<VkBuffer> &buffers, vector<VmaAllocation> &allocs, VkBufferUsageFlags usage, u32 size);
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
    void copy_Buffer(VkBuffer srcBuffer, VkImage dstImage, VkDeviceSize size, uvec3 hwd, VkImageLayout layout);
    u32 find_Memory_Type(u32 typeFilter, VkMemoryPropertyFlags properties);
    VkCommandBuffer begin_Single_Time_Commands();
    void end_Single_Time_Commands(VkCommandBuffer commandBuffer);
    void transition_Image_Layout_Singletime(VkImage image, VkFormat format, VkImageAspectFlags aspect,VkImageLayout oldLayout, VkImageLayout newLayout,
        VkPipelineStageFlags sourceStage, VkPipelineStageFlags destinationStage, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask);
    void transition_Image_Layout_Cmdb(VkCommandBuffer commandBuffer, VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout,
        VkPipelineStageFlags sourceStage, VkPipelineStageFlags destinationStage, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask);
    void copy_Whole_Image(VkExtent3D extent, VkCommandBuffer cmdbuf, VkImage src, VkImage dst);
    
    void get_Layers();
    void get_Instance_Extensions();
    void get_PhysicalDevice_Extensions();
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
    vector<VkImage>          rayGenDepthImages;
    vector<VkImage>       raytraceBlocksImages;
    vector<VkImage>         originBlocksImages;
    vector<VkImage> raytraceBlockPaletteImages;
    vector<VkImage>   originBlockPaletteImages;
    vector<VkImage> raytraceVoxelPaletteImages;
    vector<VkBuffer>       paletteCounterBuffers; //atomic
    vector<VkBuffer>          copyCounterBuffers; //atomic
    // vector<VkImage>   originVoxelPaletteImages; //unused - voxel mat palette does not change
    vector<VkImage>            raytracedImages;
    vector<VkImage>            swapChainImages;
    vector<VmaAllocation>          rayGenPosMatImageAllocations;
    vector<VmaAllocation>            rayGenNormImageAllocations;
    vector<VmaAllocation>           rayGenDepthImageAllocations;
    vector<VmaAllocation>        raytraceBlocksImageAllocations;
    vector<VmaAllocation>          originBlocksImageAllocations;
    vector<VmaAllocation>  raytraceBlockPaletteImageAllocations;
    vector<VmaAllocation>    originBlockPaletteImageAllocations;
    vector<VmaAllocation>  raytraceVoxelPaletteImageAllocations;
    // vector<VmaAllocation>    originVoxelPaletteImageAllocations; //unused - voxel mat palette does not change
    vector<VmaAllocation>             raytracedImageAllocations;
    vector<VmaAllocation>        paletteCounterBufferAllocations;
    vector<VmaAllocation>           copyCounterBufferAllocations;
    vector<VkImageView>         rayGenPosMatImageViews;
    vector<VkImageView>           rayGenNormImageViews;
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

