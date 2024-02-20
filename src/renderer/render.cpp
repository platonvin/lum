#define VMA_IMPLEMENTATION
#define VMA_STATIC_VULKAN_FUNCTIONS 0
// #define VMA_DYNAMIC_VULKAN_FUNCTIONS 0
// #define VMA_VULKAN_VERSION 1003000
#include "render.hpp" 

using namespace std;
using namespace glm;

//they are global because its easier lol
const vector<const char*> debugInstanceLayers = {
#ifndef VKNDEBUG
    "VK_LAYER_KHRONOS_validation",
#endif
    "VK_LAYER_LUNARG_monitor",
};
vector<const char*> debugInstanceExtensions = {
#ifndef VKNDEBUG
    "VK_EXT_debug_utils",
#endif
};

//no debug device extensions
vector<const char*> instanceLayers = {};
vector<const char*> instanceExtensions = {};
vector<const char*>   deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    // "VK_KHR_shader_non_semantic_info"
};


void Renderer::get_Layers(){
    for(auto layer : debugInstanceLayers){
        instanceLayers.push_back(layer);
    }
    
    //actual verify part
    // for(auto layer : instanceLayers){
        
    // }
}

void Renderer::get_Instance_Extensions(){
    u32 glfwExtCount;
    const char** glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtCount);
    
    for(u32 i=0; i < glfwExtCount; i++){
        instanceExtensions.push_back(glfwExtensions[i]);
    }
    for(auto ext : debugInstanceExtensions){
        instanceExtensions.push_back(ext);
    }

    //actual verify part
    u32 supportedExtensionCount = 0;
    VK_CHECK(vkEnumerateInstanceExtensionProperties(NULL, &supportedExtensionCount, NULL));
    vector<VkExtensionProperties> supportedInstanceExtensions(supportedExtensionCount);
    VK_CHECK(vkEnumerateInstanceExtensionProperties(NULL, &supportedExtensionCount, supportedInstanceExtensions.data()));    

    for(auto ext : instanceExtensions){
        bool supported = false;
        for(auto sup : supportedInstanceExtensions){
            if(strcmp(ext, sup.extensionName) == 0) supported = true;
        }
        if(not supported) {
            cout << KRED << ext << " not supported" << KEND;
            exit(1);
        }
    }
}

void Renderer::get_PhysicalDevice_Extensions(){
    //empty - they are in global vector. Might change in future tho
}

void Renderer::create_Allocator(){
    VmaAllocatorCreateInfo allocatorInfo = {};
    VmaVulkanFunctions vulkanFunctions;
        vulkanFunctions.vkGetInstanceProcAddr = vkGetInstanceProcAddr;
        vulkanFunctions.vkGetDeviceProcAddr = vkGetDeviceProcAddr;
        vulkanFunctions.vkGetPhysicalDeviceProperties = vkGetPhysicalDeviceProperties;
        vulkanFunctions.vkGetPhysicalDeviceMemoryProperties = vkGetPhysicalDeviceMemoryProperties;
        vulkanFunctions.vkAllocateMemory = vkAllocateMemory;
        vulkanFunctions.vkFreeMemory = vkFreeMemory;
        vulkanFunctions.vkMapMemory = vkMapMemory;
        vulkanFunctions.vkUnmapMemory = vkUnmapMemory;
        vulkanFunctions.vkFlushMappedMemoryRanges = vkFlushMappedMemoryRanges;
        vulkanFunctions.vkInvalidateMappedMemoryRanges = vkInvalidateMappedMemoryRanges;
        vulkanFunctions.vkBindBufferMemory = vkBindBufferMemory;
        vulkanFunctions.vkBindImageMemory = vkBindImageMemory;
        vulkanFunctions.vkGetBufferMemoryRequirements = vkGetBufferMemoryRequirements;
        vulkanFunctions.vkGetImageMemoryRequirements = vkGetImageMemoryRequirements;
        vulkanFunctions.vkCreateBuffer = vkCreateBuffer;
        vulkanFunctions.vkDestroyBuffer = vkDestroyBuffer;
        vulkanFunctions.vkCreateImage = vkCreateImage;
        vulkanFunctions.vkDestroyImage = vkDestroyImage;
        vulkanFunctions.vkCmdCopyBuffer = vkCmdCopyBuffer;
        vulkanFunctions.vkGetBufferMemoryRequirements2KHR = vkGetBufferMemoryRequirements2KHR;
        vulkanFunctions.vkGetImageMemoryRequirements2KHR = vkGetImageMemoryRequirements2KHR;
        vulkanFunctions.vkBindBufferMemory2KHR = vkBindBufferMemory2KHR;
        vulkanFunctions.vkBindImageMemory2KHR = vkBindImageMemory2KHR;
        vulkanFunctions.vkGetPhysicalDeviceMemoryProperties2KHR = vkGetPhysicalDeviceMemoryProperties2KHR;
    allocatorInfo.pVulkanFunctions = (const VmaVulkanFunctions*) &vulkanFunctions;
    allocatorInfo.instance = instance;
    allocatorInfo.physicalDevice = physicalDevice;
    allocatorInfo.device = device;
    VK_CHECK(vmaCreateAllocator(&allocatorInfo, &VMAllocator));
}

void Renderer::create_Window(){
    int glfwRes = glfwInit();
    assert(glfwRes != NULL);
    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
    const GLFWvidmode* mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
    window.width  = mode->width  / 1;
    window.height = mode->height / 1;
    
    // window.pointer = glfwCreateWindow(window.width, window.height, "renderer_vk", glfwGetPrimaryMonitor(), 0);
    window.pointer = glfwCreateWindow(window.width, window.height, "renderer_vk", 0, 0);
    assert(window.pointer != NULL);
}

void Renderer::create_Instance(){
    VkApplicationInfo app_info = {};
        app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        app_info.pApplicationName = "Lol vulkan raytracer";
        app_info.applicationVersion = VK_MAKE_VERSION(1,0,0);
        app_info.pEngineName = "my vk engine";
        app_info.engineVersion = VK_MAKE_VERSION(1,0,0);
        app_info.apiVersion = VK_API_VERSION_1_3;

    vector<VkValidationFeatureEnableEXT> enabledFeatures = {VK_VALIDATION_FEATURE_ENABLE_BEST_PRACTICES_EXT};

    VkValidationFeaturesEXT features = {};
        features.sType = VK_STRUCTURE_TYPE_VALIDATION_FEATURES_EXT;
        features.enabledValidationFeatureCount = enabledFeatures.size();
        features.pEnabledValidationFeatures    = enabledFeatures.data();

    VkInstanceCreateInfo createInfo = {};
    #ifndef VKNDEBUG 
        // createInfo.pNext = &features; // according to spec
    #endif 
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &app_info;
        createInfo.enabledExtensionCount   = instanceExtensions.size();
        createInfo.ppEnabledExtensionNames = instanceExtensions.data();
        createInfo.enabledLayerCount   = debugInstanceLayers.size();
        createInfo.ppEnabledLayerNames = debugInstanceLayers.data();

    VK_CHECK(vkCreateInstance(&createInfo, NULL, &instance));
}

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData) {
    if(messageType == VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) printf(KCYN);
    if(messageType == VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) printf(KRED); 
    if(messageType == VK_DEBUG_UTILS_MESSAGE_SEVERITY_INFO_BIT_EXT) printf(KBLU); 
    printf("validation layer: %s\n\n" KEND, pCallbackData->pMessage);
    return VK_FALSE;
}

void Renderer::setup_Debug_Messenger(){
    VkDebugUtilsMessengerEXT debugMessenger;
    
    VkDebugUtilsMessengerCreateInfoEXT debugUtilsCreateInfo = {};
        debugUtilsCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        debugUtilsCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debugUtilsCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        debugUtilsCreateInfo.pfnUserCallback = debugCallback;

    VK_CHECK(vkCreateDebugUtilsMessengerEXT(instance, &debugUtilsCreateInfo, NULL, &debugMessenger));

    this->debugMessenger = debugMessenger;
}

void Renderer::create_Surface(){
    VK_CHECK(glfwCreateWindowSurface(instance, window.pointer, NULL, &surface));
}

QueueFamilyIndices Renderer::find_Queue_Families(VkPhysicalDevice device){
    QueueFamilyIndices indices;

    u32 queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, NULL);
    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    i32 i = 0;
    for (auto queueFamily : queueFamilies) {
        if ((queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) and (queueFamily.queueFlags & VK_QUEUE_COMPUTE_BIT)) {
            indices.graphicalAndCompute = i;
        }

        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);

        if (presentSupport) {
            indices.present = i;
        }

        if (indices.isComplete()) {
            break;
        }

        i++;
    }

    return indices;
}

bool Renderer::check_PhysicalDevice_Extension_Support(VkPhysicalDevice device){
    u32 extensionCount;
    vkEnumerateDeviceExtensionProperties(device, NULL, &extensionCount, NULL);
    vector<VkExtensionProperties> availableExtensions(extensionCount);
    vkEnumerateDeviceExtensionProperties(device, NULL, &extensionCount, availableExtensions.data());

    //for time complexity? FOR CODE COMPLEXITY HAHAHA
    //does not apply to current code

    vector<const char*> requiredExtensions = deviceExtensions;

    for (auto req : requiredExtensions) {
        bool supports = false;
        for (auto avail : availableExtensions) {
            if(strcmp(req, avail.extensionName) == 0) {
                supports = true;
            }
        }
        if (not supports) {
            // cout << KRED << req << "not supported\n" << KEND;
            return false;
        }
    }
    return true;
}

SwapChainSupportDetails Renderer::query_Swapchain_Support(VkPhysicalDevice device){
    SwapChainSupportDetails details;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

    u32 formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, NULL);
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &formatCount, details.formats.data());

    u32 presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, NULL);
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &presentModeCount, details.presentModes.data());

    return details;
}

bool Renderer::check_Format_Support(VkPhysicalDevice device, VkFormat format, VkFormatFeatureFlags features) {
    VkFormatProperties formatProps;
    vkGetPhysicalDeviceFormatProperties(device, format, &formatProps);
    printf("%X\n", features);
    printf("%X\n", formatProps.optimalTilingFeatures);
    printf("%d\n", formatProps.optimalTilingFeatures & features);
    return formatProps.optimalTilingFeatures & features;

    // if (formatProps.optimalTilingFeatures & features) {
    //     return true;
    // }
    // imageFormatProps.
    // return false;
    
    // // cout << res;
    // if (res == VK_SUCCESS) {
    //     return true;
    // }

}

bool Renderer::is_PhysicalDevice_Suitable(VkPhysicalDevice device) {
    QueueFamilyIndices indices = find_Queue_Families(device);
    SwapChainSupportDetails swapChainSupport;
    bool extensionsSupported = check_PhysicalDevice_Extension_Support(device);
    bool imageFormatSupport = check_Format_Support(
        device,
        VK_FORMAT_R8G8B8A8_UNORM,
        VK_FORMAT_FEATURE_STORAGE_IMAGE_BIT
    );
    
    if (extensionsSupported) {
        swapChainSupport  = query_Swapchain_Support(device);
    }

    return indices.isComplete() 
        && extensionsSupported 
        && swapChainSupport.is_Suitable() 
        && imageFormatSupport
    ;   
}

void Renderer::pick_Physical_Device(){
    u32 deviceCount;
    VK_CHECK(vkEnumeratePhysicalDevices(instance, &deviceCount, NULL));
    vector<VkPhysicalDevice> devices (deviceCount);
    VK_CHECK(vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data()));

    physicalDevice = VK_NULL_HANDLE;
    for (auto device : devices) {
        // cout << device << "\n";
        if (is_PhysicalDevice_Suitable(device)){
            physicalDevice = device;//could be moved out btw
            break;
        }
    }
    if(physicalDevice == VK_NULL_HANDLE){
        cout << KRED "no suitable physical device\n" KEND;
        exit(1);
    }
}

void Renderer::create_Logical_Device(){
    QueueFamilyIndices indices = find_Queue_Families(physicalDevice);

    vector<VkDeviceQueueCreateInfo> queueCreateInfos = {};
    set<u32> uniqueQueueFamilies = {indices.graphicalAndCompute.value(), indices.present.value()};

    float queuePriority = 1.0f; //random priority?
    for (u32 queueFamily : uniqueQueueFamilies) {
        VkDeviceQueueCreateInfo queueCreateInfo{};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = queueFamily;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;
        queueCreateInfos.push_back(queueCreateInfo);
    }

    VkPhysicalDeviceFeatures deviceFeatures = {};

    VkDeviceCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
        createInfo.queueCreateInfoCount = static_cast<u32>(queueCreateInfos.size());
        createInfo.pQueueCreateInfos = queueCreateInfos.data();
        createInfo.pEnabledFeatures = &deviceFeatures;
        createInfo.enabledExtensionCount = static_cast<u32>(deviceExtensions.size());
        createInfo.ppEnabledExtensionNames = deviceExtensions.data();
        createInfo.enabledLayerCount = static_cast<u32>(instanceLayers.size());
        createInfo.ppEnabledLayerNames = instanceLayers.data();

    VK_CHECK(vkCreateDevice(physicalDevice, &createInfo, NULL, &device));

    vkGetDeviceQueue(device, indices.graphicalAndCompute.value(), 0, &graphicsQueue);
    vkGetDeviceQueue(device, indices.graphicalAndCompute.value(), 0, &computeQueue);
    vkGetDeviceQueue(device, indices.present.value(), 0, &presentQueue);
}

VkSurfaceFormatKHR Renderer::choose_Swap_SurfaceFormat(vector<VkSurfaceFormatKHR> availableFormats) {
    for (auto format : availableFormats) {
        if (format.format == VK_FORMAT_R8G8B8A8_UNORM && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return format;
        }
    }
    cout << KYEL "Where is your VK_FORMAT_R8G8B8A8_UNORM VK_COLOR_SPACE_SRGB_NONLINEAR_KHR?\n" KEND;
    return availableFormats[0];
}

VkPresentModeKHR Renderer::choose_Swap_PresentMode(vector<VkPresentModeKHR> availablePresentModes) {
    for (auto mode : availablePresentModes) {
        if (mode == VK_PRESENT_MODE_FIFO_KHR) {
        // if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return mode;
        }
    }
    // cout << "fifo\n";
    return VK_PRESENT_MODE_FIFO_KHR;
}

VkExtent2D Renderer::choose_Swap_Extent(VkSurfaceCapabilitiesKHR capabilities) {
    if (capabilities.currentExtent.width != numeric_limits<u32>::max()) {
        return capabilities.currentExtent;
    } else {
        i32 width, height;
        glfwGetFramebufferSize(window.pointer, &width, &height);

        VkExtent2D actualExtent = {
            static_cast<u32>(width),
            static_cast<u32>(height)
        };

        actualExtent.width  = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

        return actualExtent;
    }
}

void Renderer::create_Swapchain(){
    SwapChainSupportDetails swapChainSupport = query_Swapchain_Support(physicalDevice);

    VkSurfaceFormatKHR surfaceFormat = choose_Swap_SurfaceFormat(swapChainSupport.formats);
    VkPresentModeKHR presentMode = choose_Swap_PresentMode(swapChainSupport.presentModes);
    VkExtent2D extent = choose_Swap_Extent(swapChainSupport.capabilities);

    u32 imageCount = swapChainSupport.capabilities.minImageCount + 1;
    if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }

    VkSwapchainCreateInfoKHR createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        createInfo.surface = surface;
        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.imageExtent = extent;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT;

    QueueFamilyIndices indices = find_Queue_Families(physicalDevice);
    u32 queueFamilyIndices[] = {indices.graphicalAndCompute.value(), indices.present.value()};

    if (indices.graphicalAndCompute != indices.present) {
        createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        createInfo.queueFamilyIndexCount = 2;
        createInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else {
        createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    }

        createInfo.preTransform = swapChainSupport.capabilities.currentTransform;
        createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
        createInfo.presentMode = presentMode;
        createInfo.clipped = VK_TRUE;
        createInfo.oldSwapchain = VK_NULL_HANDLE;

    VK_CHECK(vkCreateSwapchainKHR(device, &createInfo, NULL, &swapchain));

    vkGetSwapchainImagesKHR(device, swapchain, &imageCount, NULL);
    swapChainImages.resize(imageCount);
    // cout << imageCount << "\n";
    vkGetSwapchainImagesKHR(device, swapchain, &imageCount, swapChainImages.data());

    swapChainImageFormat = surfaceFormat.format;
    swapChainExtent = extent;
}
void Renderer::recreate_Swapchain(){
    // vkDeviceWaitIdle(device);

    // for (auto framebuffer : swapChainFramebuffers) {
    //     vkDestroyFramebuffer(device, framebuffer, NULL);}
    // for (auto imageView : swapChainImageViews) {
    //     vkDestroyImageView(device, imageView, NULL);}
    // vkDestroySwapchainKHR(device, swapchain, NULL);

    // for (int i=0; i<MAX_FRAMES_IN_FLIGHT; i++){
    //     vkDestroyImageView(device,      computeImageViews[i], NULL);
    //     vkDestroyImageView(device, rayGenPosMatImageViews[i], NULL);
    //     vkDestroyImageView(device,   rayGenNormImageViews[i], NULL);
    //     vkFreeMemory(device,      computeImagesMemory[i], NULL);
    //     vkFreeMemory(device, rayGenPosMatImagesMemory[i], NULL);
    //     vkFreeMemory(device,   rayGenNormImagesMemory[i], NULL);
    //     vkDestroyImage(device,      computeImages[i], NULL);
    //     vkDestroyImage(device, rayGenPosMatImages[i], NULL);
    //     vkDestroyImage(device,   rayGenNormImages[i], NULL);

    //     vkDestroyFramebuffer(device, rayGenFramebuffers[i], NULL);
    // }

    // create_Swapchain();
    // create_Image_Views();

    // create_Image_Storages(computeImages, computeImagesMemory, computeImageViews,
    //     VK_IMAGE_TYPE_2D,
    //     VK_FORMAT_R8G8B8A8_UNORM,
    //     VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
    //     VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    //     VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
    //     VK_ACCESS_SHADER_WRITE_BIT);
    // create_Image_Storages(rayGenPosMatImages, rayGenPosMatImagesMemory, rayGenPosMatImageViews,
    //     VK_IMAGE_TYPE_2D,
    //     VK_FORMAT_R32G32B32A32_SFLOAT,
    //     VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
    //     VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    //     VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
    //     VK_ACCESS_SHADER_WRITE_BIT);
    // create_Image_Storages(rayGenNormImages, rayGenNormImagesMemory, rayGenNormImageViews,
    //     VK_IMAGE_TYPE_2D,
    //     VK_FORMAT_R32G32B32A32_SFLOAT,
    //     VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
    //     VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
    //     VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
    //     VK_ACCESS_SHADER_WRITE_BIT);

    // vector<vector<VkImageView>> swapViews = {swapChainImageViews};
    // create_N_Framebuffers(swapChainFramebuffers, swapViews, graphicalRenderPass, swapChainImages.size(), swapChainExtent.width, swapChainExtent.height);

    // vector<vector<VkImageView>> rayVeiws = {rayGenPosMatImageViews, rayGenNormImageViews};
    // create_N_Framebuffers(rayGenFramebuffers, rayVeiws, rayGenRenderPass, MAX_FRAMES_IN_FLIGHT, swapChainExtent.width, swapChainExtent.height);

}

void Renderer::create_Image_Views(){
    swapChainImageViews.resize(swapChainImages.size());

    for(i32 i=0; i<swapChainImages.size(); i++){
        VkImageViewCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = swapChainImages[i];
        createInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        createInfo.format = swapChainImageFormat;
        createInfo.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;
        createInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        createInfo.subresourceRange.baseMipLevel = 0;
        createInfo.subresourceRange.levelCount = 1;
        createInfo.subresourceRange.baseArrayLayer = 0;
        createInfo.subresourceRange.layerCount = 1;

        VK_CHECK(vkCreateImageView(device, &createInfo, NULL, &swapChainImageViews[i]));
    }
}

vector<char> read_Shader(const char* filename) {
    ifstream file(filename, ios::ate | ios::binary);
    assert(file.is_open());

    u32 fileSize = file.tellg();
    vector<char> buffer(fileSize);

    file.seekg(0);
    file.read(buffer.data(), fileSize);

    file.close();
    return buffer;
}

VkShaderModule Renderer::create_Shader_Module(vector<char>& code) {
    VkShaderModule shaderModule;
    VkShaderModuleCreateInfo createInfo = {};

    createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    createInfo.codeSize = code.size();
    createInfo.pCode = reinterpret_cast<const u32*>(code.data());

    VK_CHECK(vkCreateShaderModule(device, &createInfo, NULL, &shaderModule));

    return shaderModule;
}

void Renderer::create_RenderPass_Graphical(){
    VkAttachmentDescription colorAttachment = {};
        colorAttachment.format = swapChainImageFormat;
        colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachment.loadOp  = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE; 
        colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

    VkAttachmentReference colorAttachmentRef = {};
        colorAttachmentRef.attachment = 0;
        colorAttachmentRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;

    VkSubpassDescription subpass = {};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.colorAttachmentCount = 1;
        subpass.pColorAttachments = &colorAttachmentRef;

    VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

    
    VkRenderPassCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        createInfo.attachmentCount = 1;
        createInfo.pAttachments = &colorAttachment;
        createInfo.subpassCount = 1;
        createInfo.pSubpasses = &subpass;
        createInfo.dependencyCount = 1;
        createInfo.pDependencies = &dependency;
    
    VK_CHECK(vkCreateRenderPass(device, &createInfo, NULL, &graphicalRenderPass));

}
void Renderer::create_RenderPass_RayGen(){
    VkAttachmentDescription colorAttachmentPosMat = {};
        colorAttachmentPosMat.format = VK_FORMAT_R32G32B32A32_SFLOAT;
        colorAttachmentPosMat.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachmentPosMat.loadOp  = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachmentPosMat.storeOp = VK_ATTACHMENT_STORE_OP_STORE; 
        colorAttachmentPosMat.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachmentPosMat.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachmentPosMat.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachmentPosMat.finalLayout = VK_IMAGE_LAYOUT_GENERAL;
    VkAttachmentDescription colorAttachmentNorm = {};
        colorAttachmentNorm.format = VK_FORMAT_R32G32B32A32_SFLOAT;
        colorAttachmentNorm.samples = VK_SAMPLE_COUNT_1_BIT;
        colorAttachmentNorm.loadOp  = VK_ATTACHMENT_LOAD_OP_CLEAR;
        colorAttachmentNorm.storeOp = VK_ATTACHMENT_STORE_OP_STORE; 
        colorAttachmentNorm.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        colorAttachmentNorm.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        colorAttachmentNorm.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        colorAttachmentNorm.finalLayout = VK_IMAGE_LAYOUT_GENERAL;
    VkAttachmentDescription depthAttachment = {};
        depthAttachment.format = VK_FORMAT_D32_SFLOAT;
        depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
        depthAttachment.loadOp  = VK_ATTACHMENT_LOAD_OP_CLEAR;
        depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE; 
        depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
        depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
        depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorAttachmentPosMatRef = {};
        colorAttachmentPosMatRef.attachment = 0;
        colorAttachmentPosMatRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    VkAttachmentReference colorAttachmentNormRef = {};
        colorAttachmentNormRef.attachment = 1;
        colorAttachmentNormRef.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    VkAttachmentReference depthAttachmentRef = {};
        depthAttachmentRef.attachment = 2;
        depthAttachmentRef.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    vector<VkAttachmentDescription> attachments = {colorAttachmentPosMat, colorAttachmentNorm, depthAttachment};
    vector<VkAttachmentReference>   colorAttachmentRefs = {colorAttachmentPosMatRef, colorAttachmentNormRef};

    VkSubpassDescription subpass = {};
        subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
        subpass.pDepthStencilAttachment = &depthAttachmentRef;
        subpass.colorAttachmentCount = colorAttachmentRefs.size();
        subpass.pColorAttachments = colorAttachmentRefs.data();

    VkSubpassDependency dependency{};
        dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
        dependency.dstSubpass = 0;
        dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.srcAccessMask = 0;
        dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT | VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
        dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

    
    VkRenderPassCreateInfo createInfo{};
        createInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
        createInfo.attachmentCount = attachments.size();
        createInfo.pAttachments    = attachments.data();
        createInfo.subpassCount = 1;
        createInfo.pSubpasses = &subpass;
        createInfo.dependencyCount = 1;
        createInfo.pDependencies = &dependency;
    
    VK_CHECK(vkCreateRenderPass(device, &createInfo, NULL, &rayGenRenderPass));

}

void Renderer::create_Graphics_Pipeline(){
    auto vertShaderCode = read_Shader("shaders/vert.spv");
    auto fragShaderCode = read_Shader("shaders/frag.spv");

    graphicsVertShaderModule = create_Shader_Module(vertShaderCode);
    graphicsFragShaderModule = create_Shader_Module(fragShaderCode);

    VkPipelineShaderStageCreateInfo vertShaderStageInfo = {};
        vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertShaderStageInfo.module = graphicsVertShaderModule;
        vertShaderStageInfo.pName = "main";
    VkPipelineShaderStageCreateInfo fragShaderStageInfo = {};
        fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragShaderStageInfo.module = graphicsFragShaderModule;
        fragShaderStageInfo.pName = "main";

    //common
    VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};

    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = 0;
        vertexInputInfo.pVertexBindingDescriptions = NULL; // Optional
        vertexInputInfo.vertexAttributeDescriptionCount = 0;
        vertexInputInfo.pVertexAttributeDescriptions = NULL; // Optional
    
    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width  = (float) swapChainExtent.width;
        viewport.height = (float) swapChainExtent.height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = swapChainExtent;  

    vector<VkDynamicState> dynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };
    VkPipelineDynamicStateCreateInfo dynamicState = {};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = dynamicStates.size();
        dynamicState.pDynamicStates = dynamicStates.data();

    VkPipelineViewportStateCreateInfo viewportState = {};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.scissorCount = 1;
    
    VkPipelineRasterizationStateCreateInfo rasterizer = {};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0f;
        rasterizer.depthBiasEnable = VK_FALSE;
        rasterizer.depthBiasConstantFactor = 0.0f; // Optional
        rasterizer.depthBiasClamp = 0.0f; // Optional
        rasterizer.depthBiasSlopeFactor = 0.0f; // Optional

    VkPipelineMultisampleStateCreateInfo multisampling = {};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        multisampling.minSampleShading = 1.0f; // Optional
        multisampling.pSampleMask = NULL; // Optional
        multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
        multisampling.alphaToOneEnable = VK_FALSE; // Optional
    
    VkPipelineColorBlendAttachmentState colorBlendAttachment = {};
        colorBlendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachment.blendEnable = VK_FALSE;
        colorBlendAttachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
        colorBlendAttachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
        colorBlendAttachment.colorBlendOp = VK_BLEND_OP_ADD; // Optional
        colorBlendAttachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
        colorBlendAttachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
        colorBlendAttachment.alphaBlendOp = VK_BLEND_OP_ADD; // Optional

    VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable = VK_FALSE;
        colorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional
        colorBlending.attachmentCount = 1;
        colorBlending.pAttachments = &colorBlendAttachment;
        colorBlending.blendConstants[0] = 0.0f; // Optional
        colorBlending.blendConstants[1] = 0.0f; // Optional
        colorBlending.blendConstants[2] = 0.0f; // Optional
        colorBlending.blendConstants[3] = 0.0f; // Optional

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 1;
        pipelineLayoutInfo.pSetLayouts = &graphicalDescriptorSetLayout;
        pipelineLayoutInfo.pushConstantRangeCount = 0; // Optional
        pipelineLayoutInfo.pPushConstantRanges = NULL; // Optional

    VK_CHECK(vkCreatePipelineLayout(device, &pipelineLayoutInfo, NULL, &graphicsLayout));

    VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = shaderStages;
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pDepthStencilState = NULL; // Optional
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = &dynamicState;
        
        pipelineInfo.layout = graphicsLayout;
        
        pipelineInfo.renderPass = graphicalRenderPass;
        pipelineInfo.subpass = 0;
        
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
        pipelineInfo.basePipelineIndex = -1; // Optional

        VK_CHECK(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, NULL, &graphicsPipeline));
}

void Renderer::create_RayGen_Pipeline(){
    auto vertShaderCode = read_Shader("shaders/rayGenVert.spv");
    auto fragShaderCode = read_Shader("shaders/rayGenFrag.spv");

    rayGenVertShaderModule = create_Shader_Module(vertShaderCode);
    rayGenFragShaderModule = create_Shader_Module(fragShaderCode);

    VkPipelineShaderStageCreateInfo vertShaderStageInfo = {};
        vertShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        vertShaderStageInfo.stage = VK_SHADER_STAGE_VERTEX_BIT;
        vertShaderStageInfo.module = rayGenVertShaderModule;
        vertShaderStageInfo.pName = "main";

    VkPipelineShaderStageCreateInfo fragShaderStageInfo = {};
        fragShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        fragShaderStageInfo.stage = VK_SHADER_STAGE_FRAGMENT_BIT;
        fragShaderStageInfo.module = rayGenFragShaderModule;
        fragShaderStageInfo.pName = "main";

    VkVertexInputBindingDescription bindingDescription{};
        bindingDescription.binding = 0;
        bindingDescription.stride = sizeof(Vertex);
        bindingDescription.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

    vector<VkVertexInputAttributeDescription> attributeDescriptions(3);
        attributeDescriptions[0].binding = 0;
        attributeDescriptions[0].location = 0;
        attributeDescriptions[0].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[0].offset = offsetof(Vertex, pos);
        attributeDescriptions[1].binding = 0;
        attributeDescriptions[1].location = 1;
        attributeDescriptions[1].format = VK_FORMAT_R32G32B32_SFLOAT;
        attributeDescriptions[1].offset = offsetof(Vertex, norm);
        attributeDescriptions[2].binding = 0;
        attributeDescriptions[2].location = 2;
        attributeDescriptions[2].format = VK_FORMAT_R8_UINT;
        attributeDescriptions[2].offset = offsetof(Vertex, matID);

    VkPipelineVertexInputStateCreateInfo vertexInputInfo = {};
        vertexInputInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
        vertexInputInfo.vertexBindingDescriptionCount = 1;
        vertexInputInfo.pVertexBindingDescriptions = &bindingDescription;
        vertexInputInfo.vertexAttributeDescriptionCount = attributeDescriptions.size();
        vertexInputInfo.pVertexAttributeDescriptions = attributeDescriptions.data();
    
    VkPipelineInputAssemblyStateCreateInfo inputAssembly = {};
        inputAssembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
        inputAssembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        inputAssembly.primitiveRestartEnable = VK_FALSE;

    VkViewport viewport{};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width  = (float) swapChainExtent.width;
        viewport.height = (float) swapChainExtent.height;
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;

    VkRect2D scissor{};
        scissor.offset = {0, 0};
        scissor.extent = swapChainExtent;  

    vector<VkDynamicState> dynamicStates = {
        VK_DYNAMIC_STATE_VIEWPORT,
        VK_DYNAMIC_STATE_SCISSOR
    };
    VkPipelineDynamicStateCreateInfo dynamicState = {};
        dynamicState.sType = VK_STRUCTURE_TYPE_PIPELINE_DYNAMIC_STATE_CREATE_INFO;
        dynamicState.dynamicStateCount = dynamicStates.size();
        dynamicState.pDynamicStates = dynamicStates.data();

    VkPipelineViewportStateCreateInfo viewportState = {};
        viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
        viewportState.viewportCount = 1;
        viewportState.scissorCount = 1;
    
    VkPipelineRasterizationStateCreateInfo rasterizer = {};
        rasterizer.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
        rasterizer.depthClampEnable = VK_FALSE;
        rasterizer.rasterizerDiscardEnable = VK_FALSE;
        rasterizer.polygonMode = VK_POLYGON_MODE_FILL;
        rasterizer.lineWidth = 1.0f;
        rasterizer.depthBiasEnable = VK_FALSE;
        rasterizer.depthBiasConstantFactor = 0.0f; // Optional
        rasterizer.depthBiasClamp = 0.0f; // Optional
        rasterizer.depthBiasSlopeFactor = 0.0f; // Optional

    VkPipelineMultisampleStateCreateInfo multisampling = {};
        multisampling.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
        multisampling.sampleShadingEnable = VK_FALSE;
        multisampling.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
        multisampling.minSampleShading = 1.0f; // Optional
        multisampling.pSampleMask = NULL; // Optional
        multisampling.alphaToCoverageEnable = VK_FALSE; // Optional
        multisampling.alphaToOneEnable = VK_FALSE; // Optional
    
VkPipelineColorBlendAttachmentState colorBlendAttachmentPos = {};
        colorBlendAttachmentPos.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachmentPos.blendEnable = VK_FALSE;
        colorBlendAttachmentPos.srcColorBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
        colorBlendAttachmentPos.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
        colorBlendAttachmentPos.colorBlendOp = VK_BLEND_OP_ADD; // Optional
        colorBlendAttachmentPos.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
        colorBlendAttachmentPos.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
        colorBlendAttachmentPos.alphaBlendOp = VK_BLEND_OP_ADD; // Optional
    VkPipelineColorBlendAttachmentState colorBlendAttachmentNorm = {};
        colorBlendAttachmentNorm.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
        colorBlendAttachmentNorm.blendEnable = VK_FALSE;
        colorBlendAttachmentNorm.srcColorBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
        colorBlendAttachmentNorm.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
        colorBlendAttachmentNorm.colorBlendOp = VK_BLEND_OP_ADD; // Optional
        colorBlendAttachmentNorm.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE; // Optional
        colorBlendAttachmentNorm.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO; // Optional
        colorBlendAttachmentNorm.alphaBlendOp = VK_BLEND_OP_ADD; // Optional
    vector<VkPipelineColorBlendAttachmentState> colorBlendAttachments = {colorBlendAttachmentPos, colorBlendAttachmentNorm};
// printl(colorBlendAttachments.size());
    VkPipelineColorBlendStateCreateInfo colorBlending{};
        colorBlending.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
        colorBlending.logicOpEnable = VK_FALSE;
        colorBlending.logicOp = VK_LOGIC_OP_COPY; // Optional
        colorBlending.attachmentCount = colorBlendAttachments.size();
        colorBlending.pAttachments = colorBlendAttachments.data();
        colorBlending.blendConstants[0] = 0.0f; // Optional
        colorBlending.blendConstants[1] = 0.0f; // Optional
        colorBlending.blendConstants[2] = 0.0f; // Optional
        colorBlending.blendConstants[3] = 0.0f; // Optional

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        pipelineLayoutInfo.setLayoutCount = 0;
        pipelineLayoutInfo.pSetLayouts = NULL;
        pipelineLayoutInfo.pushConstantRangeCount = 0; // Optional
        pipelineLayoutInfo.pPushConstantRanges = NULL; // Optional

    VK_CHECK(vkCreatePipelineLayout(device, &pipelineLayoutInfo, NULL, &rayGenPipelineLayout));

    VkPipelineDepthStencilStateCreateInfo depthStencil = {VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO};    
        depthStencil.depthTestEnable = VK_TRUE;
        depthStencil.depthWriteEnable = VK_TRUE;
        depthStencil.depthCompareOp = VK_COMPARE_OP_GREATER_OR_EQUAL;
        depthStencil.depthBoundsTestEnable = VK_FALSE;
        depthStencil.minDepthBounds = 0.0f; // Optional
        depthStencil.maxDepthBounds = 1.0f; // Optional
        depthStencil.stencilTestEnable = VK_FALSE;
        depthStencil.front = {}; // Optional
        depthStencil.back = {}; // Optional
    VkPipelineShaderStageCreateInfo shaderStages[] = {vertShaderStageInfo, fragShaderStageInfo};
    VkGraphicsPipelineCreateInfo pipelineInfo{};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
        pipelineInfo.stageCount = 2;
        pipelineInfo.pStages = shaderStages;
        pipelineInfo.pVertexInputState = &vertexInputInfo;
        pipelineInfo.pInputAssemblyState = &inputAssembly;
        pipelineInfo.pViewportState = &viewportState;
        pipelineInfo.pRasterizationState = &rasterizer;
        pipelineInfo.pMultisampleState = &multisampling;
        pipelineInfo.pDepthStencilState = &depthStencil; // Optional
        pipelineInfo.pColorBlendState = &colorBlending;
        pipelineInfo.pDynamicState = &dynamicState;
        
        pipelineInfo.layout = rayGenPipelineLayout;
        
        pipelineInfo.renderPass = rayGenRenderPass;
        pipelineInfo.subpass = 0; //first and only subpass in renderPass will use the pipeline
        
        pipelineInfo.basePipelineHandle = VK_NULL_HANDLE; // Optional
        pipelineInfo.basePipelineIndex = -1; // Optional

        VK_CHECK(vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, NULL, &rayGenPipeline));
}

void Renderer::create_N_Framebuffers(vector<VkFramebuffer> &framebuffers, vector<vector<VkImageView>> &views, VkRenderPass renderPass, u32 N, u32 Width, u32 Height){
    framebuffers.resize(N);

    for (u32 i = 0; i < N; i++) {
        vector<VkImageView> attachments = {};
        for (auto viewVec : views) {
            attachments.push_back(viewVec[i]);
        }

        VkFramebufferCreateInfo framebufferInfo = {};
            framebufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
            framebufferInfo.renderPass = renderPass;
            framebufferInfo.attachmentCount = attachments.size();
            framebufferInfo.pAttachments    = attachments.data();
            framebufferInfo.width  = Width;
            framebufferInfo.height = Height;
            framebufferInfo.layers = 1;

        VK_CHECK(vkCreateFramebuffer(device, &framebufferInfo, NULL, &framebuffers[i]));
    }
}

void Renderer::create_Command_Pool(){
    //TODO: could be saved after physical device choosen
    //btw TODO is ignored
    QueueFamilyIndices queueFamilyIndices = find_Queue_Families(physicalDevice);

    VkCommandPoolCreateInfo poolInfo = {};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = queueFamilyIndices.graphicalAndCompute.value();

    VK_CHECK(vkCreateCommandPool(device, &poolInfo, NULL, &commandPool));
}

void Renderer::create_Command_Buffers(vector<VkCommandBuffer>& commandBuffers, u32 size){
    commandBuffers.resize(size);

    VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = commandBuffers.size();
    
    VK_CHECK(vkAllocateCommandBuffers(device, &allocInfo, commandBuffers.data())); 
}

void Renderer::record_Command_Buffer_Graphical(VkCommandBuffer commandBuffer, u32 imageIndex){
    VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = 0; // Optional
        beginInfo.pInheritanceInfo = NULL; // Optional

    VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));

    transition_Image_Layout_Cmdb(commandBuffer, computeImages[currentFrame], VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0, VK_ACCESS_SHADER_READ_BIT
    );
    // transition_Image_Layout_Cmdb(commandBuffer, rayGenPosMatImages[currentFrame], VK_FORMAT_UNDEFINED, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    //     VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
    //     0, VK_ACCESS_SHADER_READ_BIT
    // );
    // VkCopyImageToImageInfoEXT info = {};
    // info.sType = VK_STRUCTURE_TYPE_COPY_IMAGE_TO_IMAGE_INFO_EXT;
    // info.
    // vkCopyImageToImageEXT(device, info)

    VkClearValue clearColor = {{{0.111f, 0.444f, 0.666f, 1.0f}}};
    VkRenderPassBeginInfo renderPassInfo = {};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = graphicalRenderPass;
        renderPassInfo.framebuffer = swapChainFramebuffers[imageIndex];
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = swapChainExtent;
        renderPassInfo.clearValueCount = 1;
        renderPassInfo.pClearValues = &clearColor;
    
    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsLayout, 0, 1, &graphicalDescriptorSets[currentFrame], 0, NULL);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

    VkViewport viewport = {};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width  = (float)(swapChainExtent.width );
        viewport.height = (float)(swapChainExtent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor = {};
        scissor.offset = {0, 0};
        scissor.extent = swapChainExtent;
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

    vkCmdDraw(commandBuffer, 3, 1, 0, 0);
    // vkCmdDraw(commandBuffer, 3, 1, 0, 0);

    vkCmdEndRenderPass(commandBuffer);

    VK_CHECK(vkEndCommandBuffer(commandBuffer));
}

// void Renderer::record_Command_Buffer_RayGen(VkCommandBuffer commandBuffer){
//     VkCommandBufferBeginInfo beginInfo = {};
//         beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
//         beginInfo.flags = 0; // Optional
//         beginInfo.pInheritanceInfo = NULL; // Optional

//     VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));

//     //TODO - change layout
//     // transition_Image_Layout_Cmdb(commandBuffer, computeImages[currentFrame], VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
//     //     VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
//     //     0, VK_ACCESS_SHADER_READ_BIT
//     // );graphic

//     VkClearValue clearColors[] = {{{0.111f, 0.666f, 0.111f, 1.0f}}, {{0.111f, 0.666f, 0.111f, 1.0f}}};
//     VkRenderPassBeginInfo renderPassInfo = {};
//         renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
//         renderPassInfo.renderPass = rayGenRenderPass;
//         renderPassInfo.framebuffer = rayGenFramebuffers[currentFrame];
//         renderPassInfo.renderArea.offset = {0, 0};
//         renderPassInfo.renderArea.extent = swapChainExtent;
//         renderPassInfo.clearValueCount = 2;
//         renderPassInfo.pClearValues = clearColors;
    
//     vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

//     vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, rayGenPipeline);

//     VkViewport viewport = {};
//         viewport.x = 0.0f;
//         viewport.y = 0.0f;
//         viewport.width  = (float)(swapChainExtent.width );
//         viewport.height = (float)(swapChainExtent.height);
//         viewport.minDepth = 0.0f;
//         viewport.maxDepth = 1.0f;
//     vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

//     VkRect2D scissor = {};
//         scissor.offset = {0, 0};
//         scissor.extent = swapChainExtent;
//     vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

//         VkBuffer vertexBuffers[] = {rayGenVertexBuffers[currentFrame]};
//         VkDeviceSize offsets[] = {0};
//         vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);

//         vkCmdBindIndexBuffer(commandBuffer, rayGenIndexBuffers[currentFrame], 0, VK_INDEX_TYPE_UINT32);

//         vkCmdDrawIndexed(commandBuffer, world.loadedChunks(0,0,0).mesh.indices.size(), 1, 0, 0, 0);

//     vkCmdEndRenderPass(commandBuffer);

//     VK_CHECK(vkEndCommandBuffer(commandBuffer));
// }

void Renderer::record_Command_Buffer_Compute(VkCommandBuffer commandBuffer){
    VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = 0; // Optional
        beginInfo.pInheritanceInfo = NULL; // Optional

    VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));

    // raygen;
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);


    transition_Image_Layout_Cmdb(commandBuffer, computeImages[currentFrame], VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0, VK_ACCESS_SHADER_WRITE_BIT
    );

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);

    //move to argument  
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computeLayout, 0, 1, &computeDescriptorSets[currentFrame], 0, 0);

    // vkCmdDispatch(commandBuffer, 1920/8, 1080/4, 1);
    // float data[32] = {111, 222};
    // for(int i=0; i<100; i++){
    //     vkCmdPushConstants(commandBuffer, computeLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(float)*2 , &data);
    //     vkCmdDispatch(commandBuffer, 5, 5, 45);
    //     data[0]++;
    //     data[1]++;
    // }
    vkCmdDispatch(commandBuffer, window.width/8, window.height/4, 1);

    VK_CHECK(vkEndCommandBuffer(commandBuffer));
}

void Renderer::create_Sync_Objects(){
     imageAvailableSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
     renderFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
    computeFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);
     rayGenFinishedSemaphores.resize(MAX_FRAMES_IN_FLIGHT);

       rayGenInFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
    graphicalInFlightFences.resize(MAX_FRAMES_IN_FLIGHT);
      computeInFlightFences.resize(MAX_FRAMES_IN_FLIGHT);

    VkSemaphoreCreateInfo semaphoreInfo = {};
        semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    VkFenceCreateInfo fenceInfo{};
        fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
        fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;

    for (i32 i=0; i < MAX_FRAMES_IN_FLIGHT; i++){
        VK_CHECK(vkCreateSemaphore(device, &semaphoreInfo, NULL,  &imageAvailableSemaphores[i]));
        VK_CHECK(vkCreateSemaphore(device, &semaphoreInfo, NULL,  &renderFinishedSemaphores[i]));
        VK_CHECK(vkCreateSemaphore(device, &semaphoreInfo, NULL, &computeFinishedSemaphores[i]));
        VK_CHECK(vkCreateSemaphore(device, &semaphoreInfo, NULL,  &rayGenFinishedSemaphores[i]));
        VK_CHECK(vkCreateFence(device, &fenceInfo, NULL, &graphicalInFlightFences[i]));
        VK_CHECK(vkCreateFence(device, &fenceInfo, NULL,   &computeInFlightFences[i]));
        VK_CHECK(vkCreateFence(device, &fenceInfo, NULL,    &rayGenInFlightFences[i]));
    }
}

// void Renderer::draw_Frame(){
//     VkSubmitInfo submitInfo = {};

//     //raygen submission    
//     vkWaitForFences(device, 1, &rayGenInFlightFences[currentFrame], VK_TRUE, 1000000);
//     vkResetFences  (device, 1, &rayGenInFlightFences[currentFrame]);

//     vkResetCommandBuffer(rayGenCommandBuffers[currentFrame], 0);
//     record_Command_Buffer_RayGen(rayGenCommandBuffers[currentFrame]);
//     submitInfo = {};
//         submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
//         submitInfo.commandBufferCount = 1;
//         submitInfo.pCommandBuffers = &rayGenCommandBuffers[currentFrame];
//         submitInfo.signalSemaphoreCount = 1;
//         submitInfo.pSignalSemaphores = &rayGenFinishedSemaphores[currentFrame];
//     VK_CHECK(vkQueueSubmit(graphicsQueue, 1, &submitInfo, rayGenInFlightFences[currentFrame]));

//     // RT submission
//     vkWaitForFences(device, 1, &computeInFlightFences[currentFrame], VK_TRUE, UINT32_MAX);
//     vkResetFences  (device, 1, &computeInFlightFences[currentFrame]);
    
//     vkResetCommandBuffer(computeCommandBuffers[currentFrame], /*VkCommandBufferResetFlagBits*/ 0);
//     record_Command_Buffer_Compute(computeCommandBuffers[currentFrame]);
//     vector<VkSemaphore> computeWaitSemaphores = {rayGenFinishedSemaphores[currentFrame]};
//     VkPipelineStageFlags computeWaitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
//     submitInfo = {};
//         submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
//         submitInfo.pWaitDstStageMask = computeWaitStages;
//         submitInfo.waitSemaphoreCount = computeWaitSemaphores.size();
//         submitInfo.pWaitSemaphores    = computeWaitSemaphores.data();
//         submitInfo.commandBufferCount = 1;
//         submitInfo.pCommandBuffers = &computeCommandBuffers[currentFrame];
//         submitInfo.signalSemaphoreCount = 1;
//         submitInfo.pSignalSemaphores = &computeFinishedSemaphores[currentFrame];
//     VK_CHECK(vkQueueSubmit(computeQueue, 1, &submitInfo, computeInFlightFences[currentFrame]));


//     // showing everything on screen submission

//     u32 imageIndex;
//     VkResult result = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);
//     if (result == VK_ERROR_OUT_OF_DATE_KHR) {
//         recreate_Swapchain();
//         return; // can be avoided, but it is just 1 frame 
//     } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
//         cout << KRED"failed to acquire swap chain image!\n"KEND;
//         exit(result);
//     }
//     vkWaitForFences(device, 1, &graphicalInFlightFences[currentFrame], VK_TRUE, UINT32_MAX);
//     vkResetFences  (device, 1, &graphicalInFlightFences[currentFrame]);

//     vkResetCommandBuffer(graphicalCommandBuffers[currentFrame], 0);
//     record_Command_Buffer_Graphical(graphicalCommandBuffers[currentFrame], imageIndex);
//     // vector<VkSemaphore> waitSemaphores = {imageAvailableSemaphores[currentFrame]};
//     vector<VkSemaphore> waitSemaphores = {computeFinishedSemaphores[currentFrame], imageAvailableSemaphores[currentFrame]};
//     vector<VkSemaphore> signalSemaphores = {renderFinishedSemaphores[currentFrame]};
//     VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
//     // VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
//     // VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
//     submitInfo = {};
//         submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
//         submitInfo.waitSemaphoreCount = waitSemaphores.size();
//         submitInfo.pWaitSemaphores    = waitSemaphores.data();
//         submitInfo.pWaitDstStageMask = waitStages;
//         submitInfo.commandBufferCount = 1;
//         submitInfo.pCommandBuffers = &graphicalCommandBuffers[currentFrame];
//         submitInfo.signalSemaphoreCount = signalSemaphores.size();
//         submitInfo.pSignalSemaphores    = signalSemaphores.data();

//     VK_CHECK(vkQueueSubmit(graphicsQueue, 1, &submitInfo, graphicalInFlightFences[currentFrame]));

//     VkSwapchainKHR swapchains[] = {swapchain};
//     VkPresentInfoKHR presentInfo = {};
//         presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
//         presentInfo.waitSemaphoreCount = signalSemaphores.size();
//         presentInfo.pWaitSemaphores = signalSemaphores.data();
//         presentInfo.swapchainCount = 1;
//         presentInfo.pSwapchains = swapchains;
//         presentInfo.pImageIndices = &imageIndex;
//         presentInfo.pResults = NULL; // Optional

//     result = vkQueuePresentKHR(presentQueue, &presentInfo);
//     if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
//         recreate_Swapchain();
//     } else if (result != VK_SUCCESS) {
//         cout << KRED"failed to present swap chain image!\n"KEND;
//         exit(result);
//     }

//     currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
// }

void Renderer::start_Frame(){
    //nothing here... 
}

void Renderer::start_RayGen(){
    vkWaitForFences(device, 1, &rayGenInFlightFences[currentFrame], VK_TRUE, UINT32_MAX);
    vkResetFences  (device, 1, &rayGenInFlightFences[currentFrame]);

    VkCommandBuffer &commandBuffer = rayGenCommandBuffers[currentFrame];
    vkResetCommandBuffer(commandBuffer, 0);
    VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = 0; // Optional
        beginInfo.pInheritanceInfo = NULL; // Optional
    VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));

    vector<VkClearValue> clearColors = {
        {{0.111f, 0.666f, 0.111f, 1.0f}}, 
        {{0.111f, 0.666f, 0.111f, 1.0f}}, 
        {{0.0, 0}}
    };
    VkRenderPassBeginInfo renderPassInfo = {};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = rayGenRenderPass;
        renderPassInfo.framebuffer = rayGenFramebuffers[currentFrame];
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = swapChainExtent;
        renderPassInfo.clearValueCount = clearColors.size();
        renderPassInfo.pClearValues    = clearColors.data();
    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, rayGenPipeline);

    VkViewport viewport = {};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width  = (float)(swapChainExtent.width );
        viewport.height = (float)(swapChainExtent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor = {};
        scissor.offset = {0, 0};
        scissor.extent = swapChainExtent;
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
}
void Renderer::draw_RayGen_Mesh(Mesh &mesh){
    VkCommandBuffer &commandBuffer = rayGenCommandBuffers[currentFrame];
        VkBuffer vertexBuffers[] = {mesh.vertexes.buf[currentFrame]};
        VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(commandBuffer, mesh.indexes.buf[currentFrame], 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(commandBuffer, mesh.icount, 1, 0, 0, 0);
}
void Renderer::end_RayGen(){
    VkCommandBuffer &commandBuffer = rayGenCommandBuffers[currentFrame];
    vkCmdEndRenderPass(commandBuffer);
    VK_CHECK(vkEndCommandBuffer(commandBuffer));

    VkSubmitInfo submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &rayGenCommandBuffers[currentFrame];
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &rayGenFinishedSemaphores[currentFrame];
    VK_CHECK(vkQueueSubmit(graphicsQueue, 1, &submitInfo, rayGenInFlightFences[currentFrame]));
}

void Renderer::start_RayTrace(){
    VkCommandBuffer &commandBuffer = computeCommandBuffers[currentFrame];
    vkWaitForFences(device, 1, &computeInFlightFences[currentFrame], VK_TRUE, UINT32_MAX);
    vkResetFences  (device, 1, &computeInFlightFences[currentFrame]);
    
    vkResetCommandBuffer(computeCommandBuffers[currentFrame], /*VkCommandBufferResetFlagBits*/ 0);
    VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = 0; // Optional
        beginInfo.pInheritanceInfo = NULL; // Optional
    VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);
    
    transition_Image_Layout_Cmdb(commandBuffer, computeImages[currentFrame], VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0, VK_ACCESS_SHADER_WRITE_BIT
    );

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computePipeline);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, computeLayout, 0, 1, &computeDescriptorSets[currentFrame], 0, 0);
}
static int itime = 0;
void Renderer::end_RayTrace(){
    VkCommandBuffer &commandBuffer = computeCommandBuffers[currentFrame];
    float time = glfwGetTime();
    itime++;
    // printf("time %f\n", time);
    // int data[1] = {*(int*)(&time)};
    vkCmdPushConstants(commandBuffer, computeLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(int)*1 , &itime);
    vkCmdDispatch(commandBuffer, window.width/8, window.height/4, 1);
    VK_CHECK(vkEndCommandBuffer(commandBuffer));

    vector<VkSemaphore> computeWaitSemaphores = {rayGenFinishedSemaphores[currentFrame]};
    VkPipelineStageFlags computeWaitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    VkSubmitInfo submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.pWaitDstStageMask = computeWaitStages;
        submitInfo.waitSemaphoreCount = computeWaitSemaphores.size();
        submitInfo.pWaitSemaphores    = computeWaitSemaphores.data();
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &computeCommandBuffers[currentFrame];
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &computeFinishedSemaphores[currentFrame];
    VK_CHECK(vkQueueSubmit(computeQueue, 1, &submitInfo, computeInFlightFences[currentFrame]));
}

void Renderer::start_Present(){
    VkCommandBuffer &commandBuffer = graphicalCommandBuffers[currentFrame];

    VkResult result = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);
    if (result == VK_ERROR_OUT_OF_DATE_KHR) {
        cout << KRED"failed to present swap chain image!\n" KEND;
        recreate_Swapchain();
        return; // can be avoided, but it is just 1 frame 
    } else if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        cout << KRED"failed to acquire swap chain image!\n" KEND;
        exit(result);
    }
    vkWaitForFences(device, 1, &graphicalInFlightFences[currentFrame], VK_TRUE, UINT32_MAX);
    vkResetFences  (device, 1, &graphicalInFlightFences[currentFrame]);

    vkResetCommandBuffer(graphicalCommandBuffers[currentFrame], 0);
    VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = 0; // Optional
        beginInfo.pInheritanceInfo = NULL; // Optional

    VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));

    transition_Image_Layout_Cmdb(commandBuffer, computeImages[currentFrame], VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        0, VK_ACCESS_SHADER_READ_BIT
    );

    VkClearValue clearColor = {{{0.111f, 0.444f, 0.666f, 1.0f}}};
    VkRenderPassBeginInfo renderPassInfo = {};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = graphicalRenderPass;
        renderPassInfo.framebuffer = swapChainFramebuffers[imageIndex];
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = swapChainExtent;
        renderPassInfo.clearValueCount = 1;
        renderPassInfo.pClearValues = &clearColor;
    
    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsLayout, 0, 1, &graphicalDescriptorSets[currentFrame], 0, NULL);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

    VkViewport viewport = {};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width  = (float)(swapChainExtent.width );
        viewport.height = (float)(swapChainExtent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor = {};
        scissor.offset = {0, 0};
        scissor.extent = swapChainExtent;
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
}
void Renderer::end_Present(){
    VkCommandBuffer &commandBuffer = graphicalCommandBuffers[currentFrame];

    vkCmdDraw(commandBuffer, 3, 1, 0, 0);
    // vkCmdDraw(commandBuffer, 3, 1, 0, 0);

    vkCmdEndRenderPass(commandBuffer);

    VK_CHECK(vkEndCommandBuffer(commandBuffer));

    // vector<VkSemaphore> waitSemaphores = {imageAvailableSemaphores[currentFrame]};
    vector<VkSemaphore> waitSemaphores = {computeFinishedSemaphores[currentFrame], imageAvailableSemaphores[currentFrame]};
    vector<VkSemaphore> signalSemaphores = {renderFinishedSemaphores[currentFrame]};
    //TODO: replace with VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    // VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    // VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    VkSubmitInfo submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.waitSemaphoreCount = waitSemaphores.size();
        submitInfo.pWaitSemaphores    = waitSemaphores.data();
        submitInfo.pWaitDstStageMask = waitStages;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &graphicalCommandBuffers[currentFrame];
        submitInfo.signalSemaphoreCount = signalSemaphores.size();
        submitInfo.pSignalSemaphores    = signalSemaphores.data();

    VK_CHECK(vkQueueSubmit(graphicsQueue, 1, &submitInfo, graphicalInFlightFences[currentFrame]));

    VkSwapchainKHR swapchains[] = {swapchain};
    VkPresentInfoKHR presentInfo = {};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = signalSemaphores.size();
        presentInfo.pWaitSemaphores = signalSemaphores.data();
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapchains;
        presentInfo.pImageIndices = &imageIndex;
        presentInfo.pResults = NULL; // Optional

    VkResult result = vkQueuePresentKHR(presentQueue, &presentInfo);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        cout << KRED"failed to present swap chain image!\n" KEND;
        recreate_Swapchain();
    } else if (result != VK_SUCCESS) {
        cout << KRED"failed to present swap chain image!\n" KEND;
        exit(result);
    }
}

void Renderer::end_Frame(){
    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void Renderer::create_Image_Storages(vector<VkImage> &images, vector<VmaAllocation> &allocs, vector<VkImageView> &views, 
    VkImageType type, VkFormat format, VkImageUsageFlags usage, VkImageAspectFlags aspect, VkImageLayout layout, VkPipelineStageFlagBits pipeStage, VkAccessFlags access, 
    uvec3 hwd){
    
    images.resize(MAX_FRAMES_IN_FLIGHT);
    allocs.resize(MAX_FRAMES_IN_FLIGHT);
     views.resize(MAX_FRAMES_IN_FLIGHT);
    
    for (i32 i=0; i < MAX_FRAMES_IN_FLIGHT; i++){
        VkImageCreateInfo imageInfo = {};
            imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            imageInfo.imageType = type;
            imageInfo.format = format;
            imageInfo.mipLevels = 1;
            imageInfo.arrayLayers = 1;
            imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
            imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
            imageInfo.usage = usage;
            imageInfo.extent.height = hwd.x;
            imageInfo.extent.width  = hwd.y;
            imageInfo.extent.depth  = hwd.z;
            imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        VmaAllocationCreateInfo allocInfo = {};
            allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
        VK_CHECK(vmaCreateImage(VMAllocator, &imageInfo, &allocInfo, &images[i], &allocs[i], NULL));

        VkImageViewCreateInfo viewInfo = {};
            viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            viewInfo.image = images[i];
            if(type == VK_IMAGE_TYPE_2D){
                viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            } else if(type == VK_IMAGE_TYPE_3D) {
                viewInfo.viewType = VK_IMAGE_VIEW_TYPE_3D;
            }
            viewInfo.format = format;
            viewInfo.subresourceRange.aspectMask = aspect;
            viewInfo.subresourceRange.baseMipLevel = 0;
            viewInfo.subresourceRange.baseArrayLayer = 0;
            viewInfo.subresourceRange.levelCount = 1;
            viewInfo.subresourceRange.layerCount = 1;
        VK_CHECK(vkCreateImageView(device, &viewInfo, NULL, &views[i]));

        transition_Image_Layout_Singletime(images[i], format, aspect, VK_IMAGE_LAYOUT_UNDEFINED, layout,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, pipeStage,
            0, access
        );
    }
}
//TODO: need to find memory, verify flags
//make so each chunk is stored in staging buffer and then copied to fast memory
tuple<Buffer, Buffer> Renderer::create_RayGen_VertexBuffers(vector<Vertex> vertices, vector<u32> indices){
    VkDeviceSize bufferSizeV = sizeof(Vertex)*vertices.size();
    VkDeviceSize bufferSizeI = sizeof(u32   )* indices.size();

    vector<VkBuffer> vertexBuffers(MAX_FRAMES_IN_FLIGHT);
    vector<VkBuffer>  indexBuffers(MAX_FRAMES_IN_FLIGHT);
    vector<VmaAllocation> vertexAllocations(MAX_FRAMES_IN_FLIGHT);
    vector<VmaAllocation>  indexAllocations(MAX_FRAMES_IN_FLIGHT);

    VkBufferCreateInfo stagingBufferInfo = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        stagingBufferInfo.size = bufferSizeV;
        stagingBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    VmaAllocationCreateInfo stagingAllocInfo = {};
        stagingAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        stagingAllocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    VkBuffer stagingBufferV = {};
    VmaAllocation stagingAllocationV = {};
    VkBuffer stagingBufferI = {};
    VmaAllocation stagingAllocationI = {};
    vmaCreateBuffer(VMAllocator, &stagingBufferInfo, &stagingAllocInfo, &stagingBufferV, &stagingAllocationV, NULL);
    stagingBufferInfo.size = bufferSizeI;
    vmaCreateBuffer(VMAllocator, &stagingBufferInfo, &stagingAllocInfo, &stagingBufferI, &stagingAllocationI, NULL);

    void* data;
    vmaMapMemory(VMAllocator, stagingAllocationV, &data);
        memcpy(data, vertices.data(), bufferSizeV);
    vmaUnmapMemory(VMAllocator, stagingAllocationV);

    vmaMapMemory(VMAllocator, stagingAllocationI, &data);
        memcpy(data, indices.data(), bufferSizeI);
    vmaUnmapMemory(VMAllocator, stagingAllocationI);

    for(i32 i=0; i<MAX_FRAMES_IN_FLIGHT; i++){
        VkBufferCreateInfo bufferInfo = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
            bufferInfo.size = bufferSizeV;
            bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        VmaAllocationCreateInfo allocInfo = {};
            allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
        vmaCreateBuffer(VMAllocator, &bufferInfo, &allocInfo, &vertexBuffers[i], &vertexAllocations[i], NULL);
            bufferInfo.size = bufferSizeI;
            bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        vmaCreateBuffer(VMAllocator, &bufferInfo, &allocInfo, &indexBuffers[i], &indexAllocations[i], NULL);
               
        copy_Buffer(stagingBufferV, vertexBuffers[i], bufferSizeV);
        copy_Buffer(stagingBufferI,  indexBuffers[i], bufferSizeI);
    }

    vmaDestroyBuffer(VMAllocator, stagingBufferV, stagingAllocationV);
    vmaDestroyBuffer(VMAllocator, stagingBufferI, stagingAllocationI);

    return {
        {vertexBuffers, vertexAllocations}, 
        {indexBuffers, indexAllocations}
    };
}

void Renderer::create_Descriptor_Set_Layouts(){
    VkDescriptorSetLayoutBinding inPosMatBinding = {};
        inPosMatBinding.binding = 0;
        inPosMatBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        inPosMatBinding.descriptorCount = 1;
        inPosMatBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    VkDescriptorSetLayoutBinding inNormBinding = {};
        inNormBinding.binding = 1;
        inNormBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        inNormBinding.descriptorCount = 1;
        inNormBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    VkDescriptorSetLayoutBinding inBlocksBinding = {};
        inBlocksBinding.binding = 2;
        inBlocksBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        inBlocksBinding.descriptorCount = 1;
        inBlocksBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    VkDescriptorSetLayoutBinding inBlockPaletteBinding = {};
        inBlockPaletteBinding.binding = 3;
        inBlockPaletteBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        inBlockPaletteBinding.descriptorCount = 1;
        inBlockPaletteBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    VkDescriptorSetLayoutBinding inVoxelPaletteBinding = {};
        inVoxelPaletteBinding.binding = 4;
        inVoxelPaletteBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        inVoxelPaletteBinding.descriptorCount = 1;
        inVoxelPaletteBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    VkDescriptorSetLayoutBinding outFrameBinding = {};
        outFrameBinding.binding = 5;
        outFrameBinding.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        outFrameBinding.descriptorCount = 1;
        outFrameBinding.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
    vector<VkDescriptorSetLayoutBinding> layoutBindings = {inPosMatBinding, inNormBinding, inBlocksBinding, inBlockPaletteBinding, inVoxelPaletteBinding, outFrameBinding};

    VkDescriptorSetLayoutCreateInfo computeLayoutInfo = {};
        computeLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        computeLayoutInfo.bindingCount = layoutBindings.size();
        computeLayoutInfo.pBindings    = layoutBindings.data();
    VK_CHECK(vkCreateDescriptorSetLayout(device, &computeLayoutInfo, NULL, &computeDescriptorSetLayout));

    VkDescriptorSetLayoutBinding inFrameBinding = {};
        inFrameBinding.binding = 0;
        inFrameBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        inFrameBinding.descriptorCount = 1;
        inFrameBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    VkDescriptorSetLayoutCreateInfo graphicsLayoutInfo = {};
        graphicsLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        graphicsLayoutInfo.bindingCount = 1;
        graphicsLayoutInfo.pBindings    = &inFrameBinding;
    VK_CHECK(vkCreateDescriptorSetLayout(device, &graphicsLayoutInfo, NULL, &graphicalDescriptorSetLayout));
}

void Renderer::create_Descriptor_Pool(){
    VkDescriptorPoolSize computePoolSize = {};
        computePoolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        computePoolSize.descriptorCount = MAX_FRAMES_IN_FLIGHT*4;
    VkDescriptorPoolSize graphicalPoolSize = {};
        graphicalPoolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        graphicalPoolSize.descriptorCount = MAX_FRAMES_IN_FLIGHT*2;

    vector<VkDescriptorPoolSize> poolSizes = {computePoolSize, graphicalPoolSize};

    VkDescriptorPoolCreateInfo poolInfo = {};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = poolSizes.size();
        poolInfo.pPoolSizes = poolSizes.data();
        poolInfo.maxSets = MAX_FRAMES_IN_FLIGHT*4; //so 

    VK_CHECK(vkCreateDescriptorPool(device, &poolInfo, NULL, &descriptorPool));
}

void Renderer::allocate_Descriptors(){
      computeDescriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
    graphicalDescriptorSets.resize(MAX_FRAMES_IN_FLIGHT);
    
    //basically just MAX_FRAMES_IN_FLIGHT of same descriptors
    
    vector<VkDescriptorSetLayout> computeLayouts(MAX_FRAMES_IN_FLIGHT, computeDescriptorSetLayout);
    VkDescriptorSetAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = descriptorPool;
        allocInfo.descriptorSetCount = computeLayouts.size();
        allocInfo.pSetLayouts        = computeLayouts.data();
    VK_CHECK(vkAllocateDescriptorSets(device, &allocInfo, computeDescriptorSets.data()));

    vector<VkDescriptorSetLayout> graphicsLayouts(MAX_FRAMES_IN_FLIGHT, graphicalDescriptorSetLayout);
    allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = descriptorPool;
        allocInfo.descriptorSetCount = graphicsLayouts.size();
        allocInfo.pSetLayouts        = graphicsLayouts.data();
    VK_CHECK(vkAllocateDescriptorSets(device, &allocInfo, graphicalDescriptorSets.data()));
}

void Renderer::setup_Compute_Descriptors(){
    for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorImageInfo inputPosMatInfo = {};
            inputPosMatInfo.imageView = rayGenPosMatImageViews[i];
            inputPosMatInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        VkDescriptorImageInfo inputNormInfo = {};
            inputNormInfo.imageView = rayGenNormImageViews[i];
            inputNormInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        VkDescriptorImageInfo inputBlockInfo = {};
            inputBlockInfo.imageView = computeBlocksImageViews[i];
            inputBlockInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        VkDescriptorImageInfo inputBlockPaletteInfo = {};
            inputBlockPaletteInfo.imageView = computeBlockPaletteImageViews[i];
            inputBlockPaletteInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        VkDescriptorImageInfo inputVoxelPaletteInfo = {};
            inputVoxelPaletteInfo.imageView = computeVoxelPaletteImageViews[i];
            inputVoxelPaletteInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        VkDescriptorImageInfo outputFrameInfo = {};
            outputFrameInfo.imageView = computeImageViews[i];
            outputFrameInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            
        VkWriteDescriptorSet PosMatWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            PosMatWrite.dstSet = computeDescriptorSets[i];
            PosMatWrite.dstBinding = 0;
            PosMatWrite.dstArrayElement = 0;
            PosMatWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            PosMatWrite.descriptorCount = 1;
            PosMatWrite.pImageInfo = &inputPosMatInfo;
        VkWriteDescriptorSet NormWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            NormWrite.dstSet = computeDescriptorSets[i];
            NormWrite.dstBinding = 1;
            NormWrite.dstArrayElement = 0;
            NormWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            NormWrite.descriptorCount = 1;
            NormWrite.pImageInfo = &inputNormInfo;
        VkWriteDescriptorSet blockWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            blockWrite.dstSet = computeDescriptorSets[i];
            blockWrite.dstBinding = 2;
            blockWrite.dstArrayElement = 0;
            blockWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            blockWrite.descriptorCount = 1;
            blockWrite.pImageInfo = &inputBlockInfo;
        VkWriteDescriptorSet blockPaletteWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            blockPaletteWrite.dstSet = computeDescriptorSets[i];
            blockPaletteWrite.dstBinding = 3;
            blockPaletteWrite.dstArrayElement = 0;
            blockPaletteWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            blockPaletteWrite.descriptorCount = 1;
            blockPaletteWrite.pImageInfo = &inputBlockPaletteInfo;
        VkWriteDescriptorSet voxelPaletteWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            voxelPaletteWrite.dstSet = computeDescriptorSets[i];
            voxelPaletteWrite.dstBinding = 4;
            voxelPaletteWrite.dstArrayElement = 0;
            voxelPaletteWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            voxelPaletteWrite.descriptorCount = 1;
            voxelPaletteWrite.pImageInfo = &inputVoxelPaletteInfo;
        VkWriteDescriptorSet outWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            outWrite.dstSet = computeDescriptorSets[i];
            outWrite.dstBinding = 5 ;
            outWrite.dstArrayElement = 0;
            outWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            outWrite.descriptorCount = 1;
            outWrite.pImageInfo = &outputFrameInfo;
        vector<VkWriteDescriptorSet> descriptorWrites = {PosMatWrite, NormWrite, blockWrite, blockPaletteWrite, voxelPaletteWrite, outWrite};

        vkUpdateDescriptorSets(device, descriptorWrites.size(), descriptorWrites.data(), 0, NULL);
    }
}

void Renderer::create_samplers(){
    VkSamplerCreateInfo samplerInfo = {};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.anisotropyEnable = VK_FALSE;
        // samplerInfo.maxAnisotropy = 0;
        samplerInfo.borderColor = VK_BORDER_COLOR_INT_OPAQUE_BLACK;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;
        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.compareOp = VK_COMPARE_OP_ALWAYS;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_LINEAR;
        samplerInfo.mipLodBias = 0.0f;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = 0.0f;
    
    computeImageSamplers.resize(MAX_FRAMES_IN_FLIGHT);
    for (auto& sampler : computeImageSamplers) {
        VK_CHECK(vkCreateSampler(device, &samplerInfo, NULL, &sampler));
    }
}

void Renderer::setup_Graphical_Descriptors(){
    for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorImageInfo storageImageInfo = {};
            storageImageInfo.imageView = computeImageViews[i];
            storageImageInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            storageImageInfo.sampler = computeImageSamplers[i];
        VkWriteDescriptorSet descriptorWrite = {};
            descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrite.dstSet = graphicalDescriptorSets[i];
            descriptorWrite.dstBinding = 0;
            descriptorWrite.dstArrayElement = 0;
            descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            descriptorWrite.descriptorCount = 1;
            descriptorWrite.pImageInfo = &storageImageInfo;
        vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, NULL);
    }
}

void Renderer::create_Compute_Pipeline(){
    auto compShaderCode = read_Shader("shaders/comp.spv");

    compShaderModule = create_Shader_Module(compShaderCode);
    
    //single stage
    VkPipelineShaderStageCreateInfo compShaderStageInfo = {};
        compShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        compShaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        compShaderStageInfo.module = compShaderModule;
        compShaderStageInfo.pName = "main";

    VkPushConstantRange pushRange = {};
        // pushRange.size = 256;
        pushRange.size = sizeof(int)*1;
        pushRange.offset = 0;
        pushRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO; 
        pipelineLayoutInfo.setLayoutCount = 1; // 1 input (image from swapchain)  for now
        pipelineLayoutInfo.pSetLayouts = &computeDescriptorSetLayout;
        pipelineLayoutInfo.pushConstantRangeCount = 1;
        pipelineLayoutInfo.pPushConstantRanges = &pushRange;
        // pipelineLayoutInfo.pushConstantRangeCount = 0;
        // pipelineLayoutInfo.pPushConstantRanges = NULL;
    VK_CHECK(vkCreatePipelineLayout(device, &pipelineLayoutInfo, NULL, &computeLayout));

    VkComputePipelineCreateInfo pipelineInfo = {};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipelineInfo.stage = compShaderStageInfo; //always single stage so no pointer :)
        pipelineInfo.layout = computeLayout;

    VK_CHECK(vkCreateComputePipelines(device, NULL, 1, &pipelineInfo, NULL, &computePipeline));
//I<3Vk
}

u32 Renderer::find_Memory_Type(u32 typeFilter, VkMemoryPropertyFlags properties){
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);    

    for (u32 i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
}

void Renderer::create_Buffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& bufferMemory) {
    VkBufferCreateInfo bufferInfo{};
        bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        bufferInfo.size = size;
        bufferInfo.usage = usage;
        bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

    VK_CHECK(vkCreateBuffer(device, &bufferInfo, NULL, &buffer));

    VkMemoryRequirements memRequirements;
    vkGetBufferMemoryRequirements(device, buffer, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        allocInfo.allocationSize = memRequirements.size;
        allocInfo.memoryTypeIndex = find_Memory_Type(memRequirements.memoryTypeBits, properties);

    VK_CHECK(vkAllocateMemory(device, &allocInfo, NULL, &bufferMemory));

    VK_CHECK(vkBindBufferMemory(device, buffer, bufferMemory, 0));
}

void Renderer::copy_Buffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
    VkCommandBuffer commandBuffer= begin_Single_Time_Commands();

    VkBufferCopy copyRegion{};
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

    end_Single_Time_Commands(commandBuffer);
}
void Renderer::copy_Buffer(VkBuffer srcBuffer, VkImage dstImage, VkDeviceSize size, uvec3 hwd, VkImageLayout layout) {
    VkCommandBuffer commandBuffer= begin_Single_Time_Commands();

    VkBufferImageCopy copyRegion = {};
    // copyRegion.size = size;
        copyRegion.imageExtent.height = hwd.x;
        copyRegion.imageExtent.width  = hwd.y;
        copyRegion.imageExtent.depth  = hwd.z;
        copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copyRegion.imageSubresource.layerCount = 1;
    vkCmdCopyBufferToImage(commandBuffer, srcBuffer, dstImage, layout, 1, &copyRegion);

    end_Single_Time_Commands(commandBuffer);
}

VkCommandBuffer Renderer::begin_Single_Time_Commands() {
    VkCommandBufferAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    allocInfo.commandPool = commandPool;
    allocInfo.commandBufferCount = 1;

    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers(device, &allocInfo, &commandBuffer);

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

    vkBeginCommandBuffer(commandBuffer, &beginInfo);

    return commandBuffer;
}
void Renderer::end_Single_Time_Commands(VkCommandBuffer commandBuffer) {
    vkEndCommandBuffer(commandBuffer);

    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;

    vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle(graphicsQueue);

    vkFreeCommandBuffers(device, commandPool, 1, &commandBuffer);
}

void Renderer::transition_Image_Layout_Singletime(VkImage image, VkFormat format, VkImageAspectFlags aspect,VkImageLayout oldLayout, VkImageLayout newLayout,
        VkPipelineStageFlags sourceStage, VkPipelineStageFlags destinationStage, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask) {

    VkCommandBuffer commandBuffer= begin_Single_Time_Commands();

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = aspect;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    barrier.srcAccessMask = srcAccessMask;
    barrier.dstAccessMask = dstAccessMask;

    vkCmdPipelineBarrier(
        commandBuffer,
        sourceStage, destinationStage,
        0,
        0, NULL,
        0, NULL,
        1, &barrier
    );

    end_Single_Time_Commands(commandBuffer);
}
void Renderer::transition_Image_Layout_Cmdb(VkCommandBuffer commandBuffer, VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout,
        VkPipelineStageFlags sourceStage, VkPipelineStageFlags destinationStage, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask) {

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = oldLayout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = image;
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    barrier.srcAccessMask = srcAccessMask;
    barrier.dstAccessMask = dstAccessMask;

    vkCmdPipelineBarrier(
        commandBuffer,
        sourceStage, destinationStage,
        0,
        0, NULL,
        0, NULL,
        1, &barrier
    );
}
//TODO: make use of frames in flight - do not wait for this things
void Renderer::update_Block_Palette(Block* blockPalette){
    VkDeviceSize bufferSize = sizeof(Block)*BLOCK_PALETTE_SIZE;
    table3d<MatID_t> blockPaletteLinear = {};
    blockPaletteLinear.resize(16, 16, 16*BLOCK_PALETTE_SIZE);
    for(int N=0; N<BLOCK_PALETTE_SIZE; N++){
        for(int x=0; x<16; x++){
            for(int y=0; y<16; y++){
                for(int z=0; z<16; z++){
                    blockPaletteLinear(x,y,z+16*N) = blockPalette[N].voxels[x][y][z].matID;
                    // blockPaletteLinear(x,y,z+16*N) = 1;
                    // printl(blockPalette[N].voxels[x][y][z].matID );
                    // printf("%d ", blockPaletteLinear(x,y,z+16*N));
                    // printf("%d ", blockPaletteLinear(x,y,z+16*N));
                }
            }
        }
        // printf("\n");
    }
    // cout << "(int)blockPaletteLinear(0,0,0)" " "<< (int)blockPaletteLinear(0,0,0) << "\n";
    // cout << "(int)blockPaletteLinear(0,0,0)" " "<< (int)((int*)blockPaletteLinear.data())[0] << "\n";
    // printl((int)blockPaletteLinear(0,0,0));
    // printl((int)blockPaletteLinear(0,5,5));
    // printl((int)blockPaletteLinear(2,4,4));
    // printl((int)blockPaletteLinear(1,6,6));
    // printl((int)blockPaletteLinear(0,2,4));
    // printl((int)blockPaletteLinear(12,13,7));
    // printl((int)blockPaletteLinear(15,15,15+255*16));
    // printl((int)blockPaletteLinear(1,1,16));
    // printl((int)blockPaletteLinear(2,2,17));
    // printl((int)blockPaletteLinear(1,2,18));
    // printl((int)blockPaletteLinear(1,2,48));
    VkBufferCreateInfo stagingBufferInfo = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        stagingBufferInfo.size = bufferSize;
        stagingBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    VmaAllocationCreateInfo stagingAllocInfo = {};
        stagingAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        stagingAllocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    VkBuffer stagingBuffer = {};
    VmaAllocation stagingAllocation = {};
    vmaCreateBuffer(VMAllocator, &stagingBufferInfo, &stagingAllocInfo, &stagingBuffer, &stagingAllocation, NULL);

    void* data;
    vmaMapMemory(VMAllocator, stagingAllocation, &data);
        memcpy(data, blockPaletteLinear.data(), bufferSize);
        // cout << "\n PIZDEC " << *((int*) data);
        // cout << "\n PIZDEC " << *((int*) blockPaletteLinear.data());
        // cout << "\n PIZDEC " << (int)*((char*)data);
        // cout << "\n PIZDEC " << (int)*((char*)blockPaletteLinear.data());
    vmaUnmapMemory(VMAllocator, stagingAllocation);
    // vmaMapMemory(VMAllocator, stagingAllocation, &data);
    // cout << "\n PIZDEC" << *((int*)data);
    // cout << "\n PIZDEC" << *((char*)data);
    // vmaUnmapMemory(VMAllocator, stagingAllocation);

    for(i32 i=0; i<MAX_FRAMES_IN_FLIGHT; i++){
        copy_Buffer(stagingBuffer, computeBlockPaletteImages[i], bufferSize, uvec3(16,16,BLOCK_PALETTE_SIZE*16), VK_IMAGE_LAYOUT_GENERAL);
    }

    vmaDestroyBuffer(VMAllocator, stagingBuffer, stagingAllocation);
}

void Renderer::update_Voxel_Palette(Material* materialPalette){
    VkDeviceSize bufferSize = sizeof(Material)*256;

    VkBufferCreateInfo stagingBufferInfo = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        stagingBufferInfo.size = bufferSize;
        stagingBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    VmaAllocationCreateInfo stagingAllocInfo = {};
        stagingAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        stagingAllocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    VkBuffer stagingBuffer = {};
    VmaAllocation stagingAllocation = {};
    vmaCreateBuffer(VMAllocator, &stagingBufferInfo, &stagingAllocInfo, &stagingBuffer, &stagingAllocation, NULL);

    void* data;
    vmaMapMemory(VMAllocator, stagingAllocation, &data);
        memcpy(data, materialPalette, bufferSize);
    vmaUnmapMemory(VMAllocator, stagingAllocation);

    for(i32 i=0; i<MAX_FRAMES_IN_FLIGHT; i++){
        copy_Buffer(stagingBuffer, computeVoxelPaletteImages[i], bufferSize, uvec3(256,6,1), VK_IMAGE_LAYOUT_GENERAL);
    }

    vmaDestroyBuffer(VMAllocator, stagingBuffer, stagingAllocation);
}
void Renderer::update_Blocks(BlockID_t* blocks){
    VkDeviceSize bufferSize = sizeof(BlockID_t)*8*8*8;

    VkBufferCreateInfo stagingBufferInfo = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
        stagingBufferInfo.size = bufferSize;
        stagingBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    VmaAllocationCreateInfo stagingAllocInfo = {};
        stagingAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        stagingAllocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
    VkBuffer stagingBuffer = {};
    VmaAllocation stagingAllocation = {};
    vmaCreateBuffer(VMAllocator, &stagingBufferInfo, &stagingAllocInfo, &stagingBuffer, &stagingAllocation, NULL);

    void* data;
    vmaMapMemory(VMAllocator, stagingAllocation, &data);
        memcpy(data, blocks, bufferSize);
    vmaUnmapMemory(VMAllocator, stagingAllocation);

    for(i32 i=0; i<MAX_FRAMES_IN_FLIGHT; i++){
        VkBufferCreateInfo bufferInfo = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
            bufferInfo.size = bufferSize;
            bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        VmaAllocationCreateInfo allocInfo = {};

        copy_Buffer(stagingBuffer, computeBlocksImages[i], bufferSize, uvec3(8,8,8), VK_IMAGE_LAYOUT_GENERAL);
    }

    vmaDestroyBuffer(VMAllocator, stagingBuffer, stagingAllocation);
}