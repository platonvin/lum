#include "render.h" 

//its global var due to simplisity of global var's enabling /disabling
static VkDebugUtilsMessengerEXT debugMessenger;

//do free
char** get_required_instance_extensions(uint32_t* requiredInstanceExtensionsCount){
    char** requiredInstanceExtensions;
    const char* debugInstanceExtensions[] = {
        "VK_EXT_debug_utils",
    };

    uint32_t debugInstanceExtensionsCount = sizeof(debugInstanceExtensions)/sizeof(debugInstanceExtensions[0]);

    //required for GLFW window creation
    uint32_t glfwExtensionCount = 0;
    const char** glfwInstanceExtensions;
    glfwInstanceExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);
    
    uint32_t supportedExtensionCount = 0;
    VkExtensionProperties* supportedInstanceExtensions;
        VK_CHECK(vkEnumerateInstanceExtensionProperties(NULL, &supportedExtensionCount, NULL));
        supportedInstanceExtensions = alloca(sizeof(VkExtensionProperties) * supportedExtensionCount); //TODO: wrap to macro so everyone thinks it's just malloc and does not complain
        VK_CHECK(vkEnumerateInstanceExtensionProperties(NULL, &supportedExtensionCount, supportedInstanceExtensions));    

    // printf("List of supported Instanceextensions:\n");
    // for (int i=0; i < supportedExtensionCount; i++){
    //     VkExtensionProperties extension = supportedInstanceExtensions[i];
    //     printf("%s\n", extension.extensionName);
    // }

    printf("\nAll required for GLFW Instanceextensions\n");
    for (int j=0; j < glfwExtensionCount; j++){
        bool isSupported = false;
        for (int i=0; i < supportedExtensionCount; i++){
            VkExtensionProperties extension = supportedInstanceExtensions[i];
            if(strcmp(extension.extensionName, glfwInstanceExtensions[j]) == 0){
                isSupported = true;
            }
        }
        if(isSupported) {
            printf(KGRN"%s is supported\n"KNRM, glfwInstanceExtensions[j]);
        } else {
            printf(KRED"%s is not supported, but required for GLFW\n"KNRM, glfwInstanceExtensions[j]);
            exit(1);
        }
    }
    printf("All required for debug Instanceextensions\n");
    for (int j=0; j < debugInstanceExtensionsCount; j++){
        bool isSupported = false;
        for (int i=0; i < supportedExtensionCount; i++){
            VkExtensionProperties extension = supportedInstanceExtensions[i];
            if(strcmp(extension.extensionName, debugInstanceExtensions[j]) == 0){
                isSupported = true;
            }
        }
        if(isSupported) {
            printf("\033[32m%s is supported\n\033[0m", debugInstanceExtensions[j]);
        } else {
            printf("\033[31m%s is not supported, but required for debugging\n\033[0m", debugInstanceExtensions[j]);
            exit(1);
        }
    }

    *requiredInstanceExtensionsCount = glfwExtensionCount + debugInstanceExtensionsCount;
    requiredInstanceExtensions = calloc(*requiredInstanceExtensionsCount, sizeof(char*));
    for(int i=0; i < *requiredInstanceExtensionsCount; i++){
        requiredInstanceExtensions[i] = calloc(VK_MAX_EXTENSION_NAME_SIZE, sizeof(char));
    }
    
    //copy GLFW required Instanceextensions to our total ones
    for (int i=0; i < glfwExtensionCount; i++){
        printf("copying %s\n", glfwInstanceExtensions[i]);
        strcpy(requiredInstanceExtensions[i], glfwInstanceExtensions[i]);
    }
    //copy debug Instanceextensions to our total ones
    for (int i=glfwExtensionCount, j=0; i < *requiredInstanceExtensionsCount; i++, j++){
        printf("copying %s\n", debugInstanceExtensions[j]);
        strcpy(requiredInstanceExtensions[i], debugInstanceExtensions[j]);
    }

    // printf("returned\n");
    return requiredInstanceExtensions;
}
//do free
char** get_required_device_extensions(uint32_t* requiredDeviceExtensionsCount, VkPhysicalDevice device){
    char** requiredDeviceExtensions;
    const char* debugDeviceExtensions[] = {
        VK_KHR_SWAPCHAIN_EXTENSION_NAME,
    };
    const uint32_t debugDeviceExtensionsCount = sizeof(debugDeviceExtensions)/sizeof(debugDeviceExtensions[0]);
    *requiredDeviceExtensionsCount = debugDeviceExtensionsCount;

    uint32_t supportedDeviceExtensionsCount = 0;
    VkExtensionProperties* supportedDeviceExtensions;
        VK_CHECK(vkEnumerateDeviceExtensionProperties(device, NULL, &supportedDeviceExtensionsCount, NULL));
        supportedDeviceExtensions = alloca(sizeof(VkLayerProperties) * supportedDeviceExtensionsCount); //TODO: wrap to macro so everyone thinks it's just malloc and does not complain
        VK_CHECK(vkEnumerateDeviceExtensionProperties(device, NULL, &supportedDeviceExtensionsCount, supportedDeviceExtensions));    

    // printf("List of supported devise extension:\n");
    // for (int i=0; i < supportedDeviceExtensionsCount; i++){
    //     VkExtensionProperties extension = supportedDeviceExtensions[i];
    //     printf("%s\n", extension.extensionName);
    // }

    printf("\nAll required debug devise extension\n");
    for (int j=0; j < debugDeviceExtensionsCount; j++){
        bool isSupported = false;
        for (int i=0; i < supportedDeviceExtensionsCount; i++){
            VkExtensionProperties extension = supportedDeviceExtensions[i];
            if(strcmp(extension.extensionName, debugDeviceExtensions[j]) == 0){
                isSupported = true;
            }
        }
        if(isSupported) {
            printf("\033[32m%s is supported\n\033[0m", debugDeviceExtensions[j]);
        } else {
            printf("\033[31m%s is not supported, but required\n\033[0m", debugDeviceExtensions[j]);
            exit(1);
        }
    }

    *requiredDeviceExtensionsCount = debugDeviceExtensionsCount;
    requiredDeviceExtensions = calloc(*requiredDeviceExtensionsCount, sizeof(char*));
    for(int i=0; i < *requiredDeviceExtensionsCount; i++){
        requiredDeviceExtensions[i] = calloc(VK_MAX_EXTENSION_NAME_SIZE, sizeof(char));
    }
    
    //copy Validaton layers to our total ones
    for (int i=0; i < *requiredDeviceExtensionsCount; i++){
        printf("copying %s\n %d", debugDeviceExtensions[i]);
        strcpy(requiredDeviceExtensions[i], debugDeviceExtensions[i]);
    }

    // printf("returned\n");
    return requiredDeviceExtensions;
}
//do free
char** get_required_layers(uint32_t* requiredLayersCount){
    char** requiredLayers;

    //validation layer
    const char* debugLayers[] = {
        "VK_LAYER_KHRONOS_validation",
        // "VK_LAYER_LUNARG_standard_validation",
    };
    const uint32_t debugLayersCount = sizeof(debugLayers)/sizeof(debugLayers[0]);
    *requiredLayersCount = debugLayersCount;
    uint32_t supportedLayersCount = 0;
    VkLayerProperties* supportedLayers;
        VK_CHECK(vkEnumerateInstanceLayerProperties(&supportedLayersCount, NULL));
        supportedLayers = alloca(sizeof(VkLayerProperties) * supportedLayersCount); //TODO: wrap to macro so everyone thinks it's just malloc and does not complain
        VK_CHECK(vkEnumerateInstanceLayerProperties(&supportedLayersCount, supportedLayers));    

    // printf("List of supported layers:\n");
    // for (int i=0; i < supportedLayersCount; i++){
    //     VkLayerProperties layer = supportedLayers[i];
    //     printf("%s\n", layer.layerName);
    // }

    printf("\nAll required debug layers\n");
    for (int j=0; j < debugLayersCount; j++){
        bool isSupported = false;
        for (int i=0; i < supportedLayersCount; i++){
            VkLayerProperties layer = supportedLayers[i];
            if(strcmp(layer.layerName, debugLayers[j]) == 0){
                isSupported = true;
            }
        }
        if(isSupported) {
            printf("\033[32m%s is supported\n\033[0m", debugLayers[j]);
        } else {
            printf("\033[31m%s is not supported, but required\n\033[0m", debugLayers[j]);
            exit(1);
        }
    }

    *requiredLayersCount = debugLayersCount;
    requiredLayers = calloc(*requiredLayersCount, sizeof(char*));
    for(int i=0; i < *requiredLayersCount; i++){
        requiredLayers[i] = calloc(VK_MAX_EXTENSION_NAME_SIZE, sizeof(char));
    }
    
    //copy Validaton layers to our total ones
    for (int i=0; i < debugLayersCount; i++){
        printf("copying %s\n %d", debugLayers[i]);
        strcpy(requiredLayers[i], debugLayers[i]);
    }

    // printf("returned\n");
    return requiredLayers;
}

static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageType,
        const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
        void* pUserData) {
    printf("validation layer: %s", pCallbackData->pMessage);
    return VK_FALSE;
}

void setup_debug_messenger(VkInstance instance){
    // VkDebugUtilsMessengerEXT debugMessenger;
    
    VkDebugUtilsMessengerCreateInfoEXT debugUtilsCreateInfo = {0};
        debugUtilsCreateInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
        debugUtilsCreateInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
        debugUtilsCreateInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
        debugUtilsCreateInfo.pfnUserCallback = debugCallback;

    VK_CHECK(vkCreateDebugUtilsMessengerEXT(instance, &debugUtilsCreateInfo, NULL, &debugMessenger));

    // return debugMessenger;
}

VkInstance create_instance(){
    VkInstance instance = {0};

    VkApplicationInfo app_info = {0};
        app_info.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        app_info.pApplicationName = "Lol vulkan raytracer";
        app_info.applicationVersion = VK_MAKE_VERSION(1,0,0);
        app_info.pEngineName = "my vk engine";
        app_info.engineVersion = VK_MAKE_VERSION(1,0,0);
        app_info.apiVersion = VK_API_VERSION_1_3;
    

    uint32_t requiredInstanceExtensionsCount = 0;
    uint32_t requiredLayersCount     = 0;

    const char* const* requiredInstanceExtensions = (const char* const*) get_required_instance_extensions(&requiredInstanceExtensionsCount);
    const char* const* requiredLayers = (const char* const*) get_required_layers (&requiredLayersCount);


    VkInstanceCreateInfo createInfo = {0};
        createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        createInfo.pApplicationInfo = &app_info;
        createInfo.enabledExtensionCount   = requiredInstanceExtensionsCount;
        createInfo.ppEnabledExtensionNames = requiredInstanceExtensions;
        createInfo.enabledLayerCount   = requiredLayersCount;
        createInfo.ppEnabledLayerNames = requiredLayers;

    VK_CHECK(vkCreateInstance(&createInfo, NULL, &instance));

    //Volk is dynamicly loading vulkan functions
    //This is required according to Volk readme 
    volkLoadInstance(instance);   

    return instance;
}

QueueFamilyIndices find_queue_families(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface) {
    QueueFamilyIndices indices;

    uint32_t queueFamilyCount;
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, NULL);
    VkQueueFamilyProperties* queueFamilies = alloca(queueFamilyCount * sizeof(VkQueueFamilyProperties));
    vkGetPhysicalDeviceQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies);

    indices.graphical.exists = false;
    indices.present.exists = false;

    for(int i=0; i<queueFamilyCount; i++){
        if(queueFamilies[i].queueFlags = VK_QUEUE_GRAPHICS_BIT){
            indices.graphical.index = i;
            indices.graphical.exists = true;
            printf(KGRN"found graphical queue %d\n"KNRM, i);
        }
        VkBool32 isPresentSupported;
        vkGetPhysicalDeviceSurfaceSupportKHR(physicalDevice, i, surface, &isPresentSupported);
        indices.present.exists = (bool)isPresentSupported;
        if (indices.present.exists) {
            indices.present.index = i;
            printf(KGRN"found present queue %d\n"KNRM, i);
        }
        if(indices.present.exists && indices.graphical.exists){
            //found queues required
            break; 
        }
    }

    if(!indices.graphical.exists) {
        printf(KRED"graphical queue not found\n"KNRM);
        exit(1);
    }
    if(!indices.present.exists) {
        printf(KRED"present queue not found\n"KNRM);
        exit(1);
    }

    return indices;
}

bool check_device_extension_support(VkPhysicalDevice device){
    uint32_t availablExtensionCount;
    uint32_t requiredExtensionCount;
    VkExtensionProperties* availablExtensions;
    char**                 requiredExtensions;
    VK_CHECK(vkEnumerateDeviceExtensionProperties(device, NULL, &availablExtensionCount, NULL));
    availablExtensions = alloca(availablExtensionCount * sizeof(VkExtensionProperties));
    VK_CHECK(vkEnumerateDeviceExtensionProperties(device, NULL, &availablExtensionCount, availablExtensions));
printf("lul\n");
    requiredExtensions = get_required_device_extensions(&requiredExtensionCount, device);
printf("lul\n");
    
    for (int i=0; i<requiredExtensionCount; i++) {
        bool supports_required = false;
        for (int j=0; j<availablExtensionCount; j++) {
            // printf("checking for %s support\n", requiredExtensions[i]);
            if(strcmp(availablExtensions[j].extensionName, requiredExtensions[i]) == 0){
                supports_required = true;
            }
        }
        if (!supports_required) return false;
    }

    return true;
}

SwapChainSupportDetails query_swap_chain_support(VkPhysicalDevice device, VkSurfaceKHR surface) {
    SwapChainSupportDetails details;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, surface, &details.capabilities);

    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &details.formatCount, NULL);
    details.formats = calloc(details.formatCount, sizeof(VkSurfaceFormatKHR)); //might be reused
    vkGetPhysicalDeviceSurfaceFormatsKHR(device, surface, &details.formatCount, details.formats);

    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &details.presentModesCount, NULL);
    details.presentModes = calloc(details.formatCount, sizeof(VkSurfaceFormatKHR)); //might be reused
    vkGetPhysicalDeviceSurfacePresentModesKHR(device, surface, &details.presentModesCount, details.presentModes);

    return details;
}

bool is_physical_device_suitable(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface) {
    VkPhysicalDeviceProperties device_properties;
    VkPhysicalDeviceFeatures   device_features;
    vkGetPhysicalDeviceProperties(physicalDevice, &device_properties);
    vkGetPhysicalDeviceFeatures(physicalDevice, &device_features);

    QueueFamilyIndices indices = find_queue_families(physicalDevice, surface);
    SwapChainSupportDetails swapChainSupport = query_swap_chain_support(physicalDevice, surface);

    // uint32_t deviceExtensionsCount;
    // char** deviceExtensions = get_required_device_extensions(&deviceExtensionsCount);
    
    //if gpu is non-integrated (discrete) and can draw graphics
    //AND
    //if has propoper queues (or queue)
    //AND
    //if supports required device extensions
    //AND
    //if swapchain is fine (has at least one format and one present mode)
    return (device_properties.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) 
        && (indices.graphical.exists) && (indices.present.exists)
        && check_device_extension_support(physicalDevice)
        && (swapChainSupport.formatCount > 0) 
        && (swapChainSupport.presentModesCount > 0);
}

VkPhysicalDevice pick_physical_device(VkInstance instance, VkSurfaceKHR surface){
    VkPhysicalDevice pickedPhysicalDevice;

    uint32_t deviceCount = 0;
    VK_CHECK(vkEnumeratePhysicalDevices(instance, &deviceCount, NULL));
    assert(deviceCount > 0);

    VkPhysicalDevice* physicalDevices = alloca(deviceCount * sizeof(VkPhysicalDevice));
    VK_CHECK(vkEnumeratePhysicalDevices(instance, &deviceCount, physicalDevices));

    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    for each(physicalDevice, physicalDevices, deviceCount) {
        // printf("%u\n", physicalDevice);
        if (is_physical_device_suitable(physicalDevice, surface)){
            pickedPhysicalDevice = physicalDevice;//could be moved out btw
            break;
        }
    }
    printf("selected physicalDevice %u\n", pickedPhysicalDevice);
    if(pickedPhysicalDevice == VK_NULL_HANDLE){
        printf(KRED"No suitable GPU\n"KNRM);
        exit(1);
    }

    return pickedPhysicalDevice;
}

VkDevice create_logical_device(VkInstance instance, VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, VkQueue* graphicsQueue, VkQueue* presentQueue){
    VkDevice device;
    
    QueueFamilyIndices indices = find_queue_families(physicalDevice, surface);

    //TODO : reimplement with set if needed more than 2 queues

    uint32_t deviceExtensionsCount;
    char** deviceExtensions = get_required_device_extensions(&deviceExtensionsCount, physicalDevice);

    VkDeviceCreateInfo deviceCreateInfo = {0};
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.enabledExtensionCount = deviceExtensionsCount;
    deviceCreateInfo.ppEnabledExtensionNames = (const char* const*) deviceExtensions;
    // deviceCreateInfo.pEnabledFeatures = &deviceFeatures;

    float queuePriority = 1.0f;
    if(indices.graphical.index == indices.present.index){
    VkDeviceQueueCreateInfo queueCreateInfo = {0};
        queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        queueCreateInfo.queueFamilyIndex = indices.graphical.index;
        queueCreateInfo.queueCount = 1;
        queueCreateInfo.pQueuePriorities = &queuePriority;

        deviceCreateInfo.queueCreateInfoCount = 1;
        deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
    }
    else {
        VkDeviceQueueCreateInfo graphicalQueueCreateInfo = {0};
        graphicalQueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        graphicalQueueCreateInfo.queueFamilyIndex = indices.graphical.index;
        graphicalQueueCreateInfo.queueCount = 1;
        graphicalQueueCreateInfo.pQueuePriorities = &queuePriority;

        VkDeviceQueueCreateInfo presentQueueCreateInfo = {0};
        presentQueueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
        presentQueueCreateInfo.queueFamilyIndex = indices.present.index;
        presentQueueCreateInfo.queueCount = 1;
        presentQueueCreateInfo.pQueuePriorities = &queuePriority;

        VkDeviceQueueCreateInfo* queueCreateInfos = alloca(2 * sizeof(VkDeviceQueueCreateInfo));
        queueCreateInfos[0] = graphicalQueueCreateInfo;
        queueCreateInfos[1] = presentQueueCreateInfo;

        deviceCreateInfo.queueCreateInfoCount = 2;
        deviceCreateInfo.pQueueCreateInfos = queueCreateInfos;
    }


    VK_CHECK(vkCreateDevice(physicalDevice, &deviceCreateInfo, NULL, &device));

    vkGetDeviceQueue(device, indices.graphical.index, 0, graphicsQueue);
    vkGetDeviceQueue(device, indices.present.index,   0, presentQueue);

    return device;
}

VkSurfaceKHR create_surface(VkInstance instance, GLFWwindow* window){
    VkSurfaceKHR surface;

    VK_CHECK(glfwCreateWindowSurface(instance, window, NULL, &surface));

    return surface;
}

VkSurfaceFormatKHR choose_swap_surface_format(VkSurfaceFormatKHR* availableFormats, uint32_t formatCount) {
    VkSurfaceFormatKHR format;
    for each(format, availableFormats, formatCount){
        if (format.format == VK_FORMAT_B8G8R8A8_SRGB && format.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR) {
            return format;
        }
    }
    printf("where is your BGR_SRGB?\n");
    return availableFormats[0];
}
VkPresentModeKHR choose_swap_present_mode(VkPresentModeKHR* availablePresentModes, uint32_t modeCount) {
    VkPresentModeKHR mode;
    for each(mode, availablePresentModes, modeCount){
        if (mode == VK_PRESENT_MODE_MAILBOX_KHR) {
            return mode;
        }
    }
    printf("where is your VK_PRESENT_MODE_MAILBOX_KHR?\n");
    return availablePresentModes[0];
}

//size
VkExtent2D choose_swap_extent(VkSurfaceCapabilitiesKHR capabilities, GLFWwindow* window){
    //also height but why check?
    if (capabilities.currentExtent.width != UINT32_MAX){
        return capabilities.currentExtent;
    } else {
        int width, height;
        glfwGetFramebufferSize(window, &width, &height);

        VkExtent2D actualExtent = {width, height};

        actualExtent.width = Clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
        actualExtent.height = Clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

        return actualExtent;
    }
}

VkSwapchainKHR create_swap_chain(VkDevice device, VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, GLFWwindow* window){
    VkSwapchainKHR swapchain;
    
    SwapChainSupportDetails swapChainSupport = query_swap_chain_support(physicalDevice, surface);

    VkSurfaceFormatKHR surfaceFormat = choose_swap_surface_format(swapChainSupport.formats, swapChainSupport.formatCount);
    VkPresentModeKHR presentMode = choose_swap_present_mode(swapChainSupport.presentModes, swapChainSupport.presentModesCount);
    VkExtent2D extent = choose_swap_extent(swapChainSupport.capabilities, window);

    //+1 to make it less responsive but faster
    uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;
    //because 0 means no maximum
    if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }
    //never ever count how many createInfo's is in your code
    VkSwapchainCreateInfoKHR swapchainCreateInfo = {0};
        swapchainCreateInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
        swapchainCreateInfo.surface = surface;
        swapchainCreateInfo.minImageCount = imageCount;
        swapchainCreateInfo.imageFormat = surfaceFormat.format;
        swapchainCreateInfo.imageColorSpace = surfaceFormat.colorSpace;
        swapchainCreateInfo.imageExtent = extent;
        swapchainCreateInfo.imageArrayLayers = 1;
        swapchainCreateInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

    QueueFamilyIndices indices = find_queue_families(physicalDevice, surface);
    uint32_t queueFamilyIndices[] = {indices.graphical.index, indices.present.index};

    //if two different queues
    if (indices.graphical.index != indices.present.index) {
        swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
        swapchainCreateInfo.queueFamilyIndexCount = 2;
        swapchainCreateInfo.pQueueFamilyIndices = queueFamilyIndices;
    } else { //single queue draws and shows
        swapchainCreateInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
        swapchainCreateInfo.queueFamilyIndexCount = 0; // Optional
        swapchainCreateInfo.pQueueFamilyIndices = NULL; // Optional
    }

    swapchainCreateInfo.preTransform = swapChainSupport.capabilities.currentTransform;
    swapchainCreateInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    swapchainCreateInfo.presentMode = presentMode;
    swapchainCreateInfo.clipped = VK_TRUE;
    //should set to NULL?
    swapchainCreateInfo.oldSwapchain = VK_NULL_HANDLE; 

    VK_CHECK(vkCreateSwapchainKHR(device, &swapchainCreateInfo, NULL, &swapchain));
    return swapchain;
}

RenderContext init_renderer(){
    RenderContext rCtx = {0};

    rCtx.window = create_window();
    VK_CHECK(volkInitialize());

    rCtx.instance = create_instance();
    //comment to disable. Writes to global var
    setup_debug_messenger(rCtx.instance);

    rCtx.surface = create_surface(rCtx.instance, rCtx.window.pointer);
    rCtx.physicalDevice = pick_physical_device(rCtx.instance, rCtx.surface);
    rCtx.device = create_logical_device(rCtx.instance, rCtx.physicalDevice, rCtx.surface, &rCtx.graphicsQueue, &rCtx.presentQueue);
    rCtx.swapchain = create_swap_chain(rCtx.device, rCtx.physicalDevice, rCtx.surface, rCtx.window.pointer);

    
    return rCtx;
}

void render_cleanup(RenderContext rCtx){
    vkDestroySwapchainKHR(rCtx.device, rCtx.swapchain, NULL);
    vkDestroySurfaceKHR(rCtx.instance, rCtx.surface, NULL);
    vkDestroyDevice(rCtx.device, NULL);
    vkDestroyDebugUtilsMessengerEXT(rCtx.instance, debugMessenger, NULL);
    vkDestroyInstance(rCtx.instance, NULL);

    glfwTerminate();
}