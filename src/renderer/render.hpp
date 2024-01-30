#pragma once

#include <vector>
#include <tuple>
#include <set>
#include <string.h>
#include <optional>
#include <iostream>
#include <stdio.h>
#include <limits>
#include <algorithm>
#include <fstream>
#include <Volk/volk.h>
#include <vulkan/vk_enum_string_helper.h>
#include <GLFW/glfw3.h>
#include "window.hpp"

using namespace std;

#define KEND  "\x1B[0m"
#define KRED  "\x1B[31m"
#define KGRN  "\x1B[32m"
#define KYEL  "\x1B[33m"
#define KBLU  "\x1B[34m"
#define KMAG  "\x1B[35m"
#define KCYN  "\x1B[36m"
#define KWHT  "\x1B[37m"

#define print printf("L:%d %s\n", __LINE__, __FUNCTION__);

#define VK_CHECK(func)                                                                        \
do {                                                                                          \
    VkResult result = func;                                                                   \
    if (result != VK_SUCCESS) {                                                               \
        fprintf(stderr, "LINE %d: Vulkan call failed: %s\n", __LINE__, string_VkResult(result));\
        exit(1);                                                                              \
    }                                                                                         \
} while (0)

// typedef struct FamilyIndex {
//     bool exists;
//     uint32_t index;
// } FamilyIndex;

typedef struct QueueFamilyIndices {
    optional<uint32_t> graphicalAndCompute;
    optional<uint32_t> present;
    // optional<uint32_t> compute;

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

        setup_Debug_Messenger();

        create_Surface();
        pick_Physical_Device();
        create_Logical_Device();
        create_Swapchain();
        create_Image_Views();

        create_Render_Pass();
        create_Graphics_Pipeline();

        create_Image_Storages();
        create_Descriptor_Pool();
        create_Descriptor_Set_Layout();
        allocate_Descriptors();
        create_Compute_Pipeline();

        create_Framebuffers();
        create_Command_Pool();
        create_Command_Buffers();
        create_Sync_Objects();
    }
    void cleanup(){

        for (int i=0; i<MAX_FRAMES_IN_FLIGHT; i++){
            vkDestroyImageView(device, computeImageViews[i], NULL);
            vkFreeMemory(device, imagesMemory[i], NULL);
            vkDestroyImage(device, computeImages[i], NULL);
        }

        vkDestroyDescriptorPool(device, descriptorPool, NULL);
        vkDestroyDescriptorSetLayout(device, descriptorSetLayout, NULL);
        vkDestroyShaderModule(device, compShaderModule, NULL); 
        vkDestroyPipeline(device, computePipeline, NULL);
        vkDestroyPipelineLayout(device, computeLayout, NULL);
        for (int i=0; i < MAX_FRAMES_IN_FLIGHT; i++){
            vkDestroySemaphore(device, imageAvailableSemaphores[i], NULL);
            vkDestroySemaphore(device, renderFinishedSemaphores[i], NULL);
            vkDestroyFence(device, inFlightFences[i], NULL);
        }
        vkDestroyCommandPool(device, commandPool, NULL);
        for (auto framebuffer : swapChainFramebuffers) {
            vkDestroyFramebuffer(device, framebuffer, NULL);
        }
        vkDestroyRenderPass(device, renderPass, NULL);
        vkDestroyPipeline(device, graphicsPipeline, NULL);
        vkDestroyPipelineLayout(device, graphicsLayout, NULL);
        vkDestroyShaderModule(device, vertShaderModule, NULL);
        vkDestroyShaderModule(device, fragShaderModule, NULL); 
        for (auto imageView : swapChainImageViews) {
            vkDestroyImageView(device, imageView, NULL);
        }
        vkDestroySwapchainKHR(device, swapchain, NULL);
        vkDestroyDevice(device, NULL);
        //"unpicking" physical device is unnessesary :)
        vkDestroySurfaceKHR(instance, surface, NULL);
        vkDestroyDebugUtilsMessengerEXT(instance, debugMessenger, NULL);
        vkDestroyInstance(instance, NULL);
        glfwDestroyWindow(window.pointer);
    }
    void draw_Frame();
private:
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
    void create_Render_Pass();
    void create_Graphics_Pipeline(); 
    void create_Descriptor_Set_Layout();
    void create_Descriptor_Pool();
    void allocate_Descriptors();
    // void update_Descriptors();
    void create_Image_Storages();
    void create_Compute_Pipeline(); 
    VkShaderModule create_Shader_Module(vector<char>& code);

    void create_Framebuffers();
    void create_Command_Pool();
    void create_Command_Buffers();
    void record_Command_Buffer(VkCommandBuffer commandBuffer, uint32_t imageIndex);
    void record_Command_Buffer_Compute(VkCommandBuffer commandBuffer, uint32_t imageIndex);
    void create_Sync_Objects();

    void create_Buffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory);
    uint32_t find_Memory_Type(uint32_t typeFilter, VkMemoryPropertyFlags properties);

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

    VkSwapchainKHR swapchain;
    VkFormat   swapChainImageFormat;
    VkExtent2D swapChainExtent;
    vector<VkImage    > swapChainImages;
    vector<VkImageView> swapChainImageViews;

    VkShaderModule vertShaderModule;
    VkShaderModule fragShaderModule;
    VkShaderModule compShaderModule;

    VkRenderPass renderPass;
    VkPipelineLayout graphicsLayout;
    VkPipeline graphicsPipeline;

    vector<VkFramebuffer> swapChainFramebuffers;

    VkCommandPool commandPool;
    vector<VkCommandBuffer> commandBuffers;

    vector<VkSemaphore> imageAvailableSemaphores;
    vector<VkSemaphore> renderFinishedSemaphores;
    vector<VkFence> inFlightFences;

    vector<VkImage    > computeImages;
    vector<VkDeviceMemory> imagesMemory;
    
    vector<VkImageView> computeImageViews;
    VkDescriptorSetLayout descriptorSetLayout;
    VkDescriptorPool descriptorPool;
    //basically just MAX_FRAMES_IN_FLIGHT of same descriptors 
    vector<VkDescriptorSet> descriptorSets;
    VkPipelineLayout computeLayout;
    VkPipeline computePipeline;
    //wraps around MAX_FRAMES_IN_FLIGHT
    uint32_t currentFrame = 0;
private:
    VkDebugUtilsMessengerEXT debugMessenger;
};

void a();