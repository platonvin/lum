#pragma once

#include <vector>
#include <tuple>
#include <set>
#include <string.h>
#include <optional>
#include <iostream>
#include <stdio.h>
#include <limits>
// #include <clamp>
#include <fstream>
#include <Volk/volk.h>
#include <vulkan/vk_enum_string_helper.h>
#include <GLFW/glfw3.h>
// #include <algorithm>
#include <glm/glm.hpp>
#include "window.hpp"
#include <defines.hpp>

using namespace std;
using namespace glm;

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
        create_Swapchain();
        create_Image_Views();

        create_RenderPass_Graphical();
        create_RenderPass_RayGen();

        create_Command_Pool();
        create_Command_Buffers(graphicalCommandBuffers, MAX_FRAMES_IN_FLIGHT);
        create_Command_Buffers(   rayGenCommandBuffers, MAX_FRAMES_IN_FLIGHT);
        create_Command_Buffers(  computeCommandBuffers, MAX_FRAMES_IN_FLIGHT);

        create_samplers();
    
        create_Image_Storages(computeImages, computeImagesMemory, computeImageViews,
            VK_FORMAT_R8G8B8A8_UNORM,
            VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_ACCESS_SHADER_WRITE_BIT);
        create_Image_Storages(rayGenPosMatImages, rayGenPosMatImagesMemory, rayGenPosMatImageViews,
            VK_FORMAT_R32G32B32A32_SFLOAT,
            VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_ACCESS_SHADER_WRITE_BIT);
        create_Image_Storages(rayGenNormImages, rayGenNormImagesMemory, rayGenNormImageViews,
            VK_FORMAT_R32G32B32A32_SFLOAT,
            VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_ACCESS_SHADER_WRITE_BIT);

        vector<vector<VkImageView>> swapViews = {swapChainImageViews};
        create_N_Framebuffers(swapChainFramebuffers, swapViews, graphicalRenderPass, swapChainImages.size(), swapChainExtent.width, swapChainExtent.height);
        vector<vector<VkImageView>> rayVeiws = {rayGenPosMatImageViews, rayGenNormImageViews};
// printl(swapChainImages.size());
        create_N_Framebuffers(rayGenFramebuffers, rayVeiws, rayGenRenderPass, MAX_FRAMES_IN_FLIGHT, swapChainExtent.width, swapChainExtent.height);
        
        create_RayGen_VertexBuffers();
        create_Descriptor_Pool();
        create_Descriptor_Set_Layouts();
        allocate_Descriptors();
        setup_Compute_Descriptors();
        setup_Graphical_Descriptors();
        // setup_RayGen_Descriptors();
        create_Compute_Pipeline();
        create_Graphics_Pipeline();
        create_RayGen_Pipeline();
        create_Sync_Objects();
    }
    void cleanup(){

        for (int i=0; i<MAX_FRAMES_IN_FLIGHT; i++){
            vkDestroyImageView(device,      computeImageViews[i], NULL);
            vkDestroyImageView(device, rayGenPosMatImageViews[i], NULL);
            vkDestroyImageView(device,   rayGenNormImageViews[i], NULL);
            vkFreeMemory(device,      computeImagesMemory[i], NULL);
            vkFreeMemory(device, rayGenPosMatImagesMemory[i], NULL);
            vkFreeMemory(device,   rayGenNormImagesMemory[i], NULL);
            vkDestroyImage(device,      computeImages[i], NULL);
            vkDestroyImage(device, rayGenPosMatImages[i], NULL);
            vkDestroyImage(device,   rayGenNormImages[i], NULL);
            vkDestroySampler(device, computeImageSamplers[i], NULL);

            vkFreeMemory(device, rayGenVertexBuffersMemory[i], NULL);
            vkFreeMemory(device,  rayGenIndexBuffersMemory[i], NULL);
            vkDestroyBuffer(device, rayGenVertexBuffers[i], NULL);
            vkDestroyBuffer(device,  rayGenIndexBuffers[i], NULL);

            vkDestroyFramebuffer(device, rayGenFramebuffers[i], NULL);
        }

        vkDestroyDescriptorPool(device, descriptorPool, NULL);
        vkDestroyDescriptorSetLayout(device,   computeDescriptorSetLayout, NULL);
        vkDestroyDescriptorSetLayout(device, graphicalDescriptorSetLayout, NULL);
        for (int i=0; i < MAX_FRAMES_IN_FLIGHT; i++){
            vkDestroySemaphore(device,  imageAvailableSemaphores[i], NULL);
            vkDestroySemaphore(device,  renderFinishedSemaphores[i], NULL);
            vkDestroySemaphore(device, computeFinishedSemaphores[i], NULL);
            vkDestroySemaphore(device,  rayGenFinishedSemaphores[i], NULL);
            vkDestroyFence(device, graphicalInFlightFences[i], NULL);
            vkDestroyFence(device,   computeInFlightFences[i], NULL);
            vkDestroyFence(device,    rayGenInFlightFences[i], NULL);
        }
        vkDestroyCommandPool(device, commandPool, NULL);
        for (auto framebuffer : swapChainFramebuffers) {
            vkDestroyFramebuffer(device, framebuffer, NULL);
        }
        vkDestroyRenderPass(device, graphicalRenderPass, NULL);
        vkDestroyRenderPass(device, rayGenRenderPass, NULL);

        vkDestroyPipeline(device,  computePipeline, NULL);
        vkDestroyPipeline(device, graphicsPipeline, NULL);
        vkDestroyPipeline(device,   rayGenPipeline, NULL);

        vkDestroyPipelineLayout(device, computeLayout, NULL);
        vkDestroyPipelineLayout(device, graphicsLayout, NULL);
        vkDestroyPipelineLayout(device, rayGenPipelineLayout, NULL);

        vkDestroyShaderModule(device, rayGenVertShaderModule, NULL);
        vkDestroyShaderModule(device, rayGenFragShaderModule, NULL); 
        vkDestroyShaderModule(device, compShaderModule, NULL); 
        vkDestroyShaderModule(device, graphicsVertShaderModule, NULL);
        vkDestroyShaderModule(device, graphicsFragShaderModule, NULL); 
        for (auto imageView : swapChainImageViews) {
            vkDestroyImageView(device, imageView, NULL);
        }
        vkDestroySwapchainKHR(device, swapchain, NULL);
        vkDestroyDevice(device, NULL);
        //"unpicking" physical device is unnessesary :)
        vkDestroySurfaceKHR(instance, surface, NULL);
#ifndef VKNDEBUG
        vkDestroyDebugUtilsMessengerEXT(instance, debugMessenger, NULL);
#endif
        vkDestroyInstance(instance, NULL);
        glfwDestroyWindow(window.pointer);
    }
    void draw_Frame();
private:
    void recreate_Swapchain();
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
    void create_Image_Views();
    void create_RenderPass_Graphical();
    void create_RenderPass_RayGen();
    void create_Graphics_Pipeline(); 
    void create_RayGen_Pipeline();
    void create_Descriptor_Set_Layouts();
    void create_Descriptor_Pool();
    void allocate_Descriptors();
    void setup_Compute_Descriptors();
    void setup_Graphical_Descriptors();
    void create_samplers();
    // void update_Descriptors();
    void create_RayGen_VertexBuffers();
    void create_Image_Storages(vector<VkImage> &images, vector<VkDeviceMemory> &memory, vector<VkImageView> &views, 
        VkFormat format, VkImageUsageFlags usage, VkImageLayout layout, VkPipelineStageFlagBits pipeStage, VkAccessFlagBits access);
    void create_Compute_Pipeline(); 
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
    u32 find_Memory_Type(u32 typeFilter, VkMemoryPropertyFlags properties);
    VkCommandBuffer begin_Single_Time_Commands();
    void end_Single_Time_Commands(VkCommandBuffer commandBuffer);
    void transition_Image_Layout_Singletime(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout,
        VkPipelineStageFlags sourceStage, VkPipelineStageFlags destinationStage, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask);
    void transition_Image_Layout_Cmdb(VkCommandBuffer commandBuffer, VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout,
        VkPipelineStageFlags sourceStage, VkPipelineStageFlags destinationStage, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask);

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
    VkShaderModule compShaderModule;

    VkRenderPass rayGenRenderPass;
    VkPipelineLayout rayGenPipelineLayout;
    VkPipeline rayGenPipeline;

    VkRenderPass graphicalRenderPass;
    VkPipelineLayout graphicsLayout;
    VkPipeline graphicsPipeline;

    vector<VkFramebuffer> swapChainFramebuffers;
    vector<VkFramebuffer>    rayGenFramebuffers;

    vector<VkCommandBuffer> graphicalCommandBuffers;
    vector<VkCommandBuffer>    rayGenCommandBuffers;
    vector<VkCommandBuffer>   computeCommandBuffers;

    vector<VkSemaphore>  imageAvailableSemaphores;
    vector<VkSemaphore>  renderFinishedSemaphores;
    vector<VkSemaphore> computeFinishedSemaphores;
    vector<VkSemaphore>  rayGenFinishedSemaphores;
    vector<VkFence> graphicalInFlightFences;
    vector<VkFence> computeInFlightFences;
    vector<VkFence> rayGenInFlightFences;

    vector<VkBuffer> rayGenVertexBuffers;
    vector<VkBuffer> rayGenIndexBuffers;
    vector<VkDeviceMemory> rayGenVertexBuffersMemory;
    vector<VkDeviceMemory> rayGenIndexBuffersMemory;
    // VkBuffer rayGenVertexBuffer_Staging;
    
    
    vector<VkImage> rayGenPosMatImages;
    vector<VkImage>   rayGenNormImages;
    vector<VkImage>      computeImages;
    vector<VkImage>    swapChainImages;
    vector<VkDeviceMemory> rayGenPosMatImagesMemory;
    vector<VkDeviceMemory>   rayGenNormImagesMemory;
    vector<VkDeviceMemory>      computeImagesMemory;
    vector<VkImageView> rayGenPosMatImageViews;
    vector<VkImageView>   rayGenNormImageViews;
    vector<VkImageView>      computeImageViews;
    vector<VkImageView>    swapChainImageViews;
    vector<VkSampler>  computeImageSamplers;

    VkDescriptorSetLayout RayGenDescriptorSetLayout;
    VkDescriptorSetLayout computeDescriptorSetLayout;
    VkDescriptorSetLayout graphicalDescriptorSetLayout;
    VkDescriptorPool descriptorPool;
    
    //basically just MAX_FRAMES_IN_FLIGHT of same descriptors 
    vector<VkDescriptorSet> computeDescriptorSets;
    vector<VkDescriptorSet> graphicalDescriptorSets;

    VkPipelineLayout computeLayout;
    VkPipeline computePipeline;
    //wraps around MAX_FRAMES_IN_FLIGHT
    u32 currentFrame = 0;
private:
    VkDebugUtilsMessengerEXT debugMessenger;
};

// void a();1