#define VMA_IMPLEMENTATION
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#include "render.hpp" 

using namespace std;
using namespace glm;



tuple<int, int> get_block_xy(int N);

void Renderer::init(int x_size, int y_size, int z_size, int block_palette_size, int copy_queue_size, vec2 ratio, bool vsync, bool fullscreen){
    world_size = ivec3(x_size, y_size, z_size);
     origin_world.allocate(world_size);
    current_world.allocate(world_size);
    _ratio = ratio;
    is_scaled = (ratio != vec2(1));
    is_vsync = vsync;
    is_fullscreen = fullscreen;

    create_Window();
    // create_window
    VK_CHECK(volkInitialize());
    get_Instance_Extensions();
    create_Instance();
    volkLoadInstance(instance);   

#ifndef VKNDEBUG
    setup_Debug_Messenger();
#endif
    create_Surface();
    pick_Physical_Device();
    create_Logical_Device();

    create_Allocator();
    
    create_Swapchain();
    create_Swapchain_Image_Views();

    create_RenderPass_Graphical();
    create_RenderPass_RayGen();

    create_Command_Pool();
    create_Command_Buffers(&graphicalCommandBuffers, MAX_FRAMES_IN_FLIGHT);
    create_Command_Buffers(&   rayGenCommandBuffers, MAX_FRAMES_IN_FLIGHT);
    create_Command_Buffers(&  computeCommandBuffers, MAX_FRAMES_IN_FLIGHT);

    create_samplers();

    create_SwapchainDependent();

    create_Buffer_Storages(&staging_world,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        world_size.x*world_size.y*world_size.z*sizeof(BlockID_t), true);
    create_Image_Storages(&world,
        VK_IMAGE_TYPE_3D,
        VK_FORMAT_R16_SINT,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT,
        VK_IMAGE_LAYOUT_GENERAL,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT,
        world_size); //TODO: dynamic
    create_Image_Storages(&origin_block_palette,
        VK_IMAGE_TYPE_3D,
        VK_FORMAT_R8G8_UINT,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT,
        VK_IMAGE_LAYOUT_GENERAL,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
        {16*BLOCK_PALETTE_SIZE_X, 16*BLOCK_PALETTE_SIZE_Y, 16}); //TODO: dynamic
    create_Image_Storages(&block_palette,
        VK_IMAGE_TYPE_3D,
        VK_FORMAT_R8G8_UINT,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT,
        VK_IMAGE_LAYOUT_GENERAL,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
        {16*BLOCK_PALETTE_SIZE_X, 16*BLOCK_PALETTE_SIZE_Y, 16}); //TODO: dynamic
    create_Image_Storages(&material_palette,
        VK_IMAGE_TYPE_2D,
        VK_FORMAT_R32_SFLOAT, //try R32G32
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT,
        VK_IMAGE_LAYOUT_GENERAL,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_MEMORY_READ_BIT ,
        {6, 256, 1}); //TODO: dynamic, text formats, pack Material into smth other than 6 floats :)

    // create_Buffer_Storages(&staging_uniform,
    //     VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
    //     sizeof(mat4)*2, true);
    create_Buffer_Storages(&uniform,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        sizeof(mat4)*2, false);
    // staging_uniform_mapped.resize(MAX_FRAMES_IN_FLIGHT);
    staging_world_mapped.resize(MAX_FRAMES_IN_FLIGHT);
    for (int i=0; i<MAX_FRAMES_IN_FLIGHT; i++){
        // vmaMapMemory(VMAllocator, staging_uniform[i].alloc, &staging_uniform_mapped[i]);
        vmaMapMemory(VMAllocator,  staging_world[i].alloc, &staging_world_mapped[i]);
    }
    
    create_Descriptor_Set_Layouts();
    create_Descriptor_Pool();
    allocate_Descriptors();
    setup_Raytrace_Descriptors();
    setup_Denoise_Descriptors();
    if(is_scaled) {
        setup_Upscale_Descriptors();
    }
    setup_Graphical_Descriptors();
    setup_RayGen_Descriptors();

    create_RayGen_Pipeline();
    create_Graphics_Pipeline();

    create_compute_pipelines();
    
    create_Sync_Objects();
}
void Renderer::delete_Images(vector<Image>* images){
    for (int i=0; i<MAX_FRAMES_IN_FLIGHT; i++){
         vkDestroyImageView(device,  (*images)[i].view, NULL);
        vmaDestroyImage(VMAllocator, (*images)[i].image, (*images)[i].alloc);
    }
}
void Renderer::delete_Images(Image* image){
    // for (int i=0; i<MAX_FRAMES_IN_FLIGHT; i++){
         vkDestroyImageView(device,  (*image).view, NULL);
        vmaDestroyImage(VMAllocator, (*image).image, (*image).alloc);
    // }
}
void Renderer::cleanup(){
    delete_Images(&world);
    delete_Images(&origin_block_palette);
    delete_Images(&       block_palette);
    delete_Images(&material_palette);

    vkDestroySampler(device, nearestSampler, NULL);
    vkDestroySampler(device,  linearSampler, NULL);

    for (int i=0; i<MAX_FRAMES_IN_FLIGHT; i++){

        
        // vmaDestroyBuffer(VMAllocator, depthStaginBuffers[i], paletteCounterBufferAllocations[i]);
        // vmaDestroyBuffer(VMAllocator, copyCounterBuffers[i], copyCounterBufferAllocations[i]);

            // vmaUnmapMemory(VMAllocator,                          staging_uniform[i].alloc);
        // vmaDestroyBuffer(VMAllocator, staging_uniform[i].buffer, staging_uniform[i].alloc);
            vmaUnmapMemory(VMAllocator,                           staging_world[i].alloc);
        vmaDestroyBuffer(VMAllocator,  staging_world[i].buffer, staging_world[i].alloc);
        vmaDestroyBuffer(VMAllocator,  uniform[i].buffer, uniform[i].alloc);
    }

    vkDestroyDescriptorPool(device, descriptorPool, NULL);
    vkDestroyDescriptorSetLayout(device,     RayGenDescriptorSetLayout, NULL);
    // vkDestroyDescriptorSetLayout(device,   blockifyDescriptorSetLayout, NULL);
    // vkDestroyDescriptorSetLayout(device,       copyDescriptorSetLayout, NULL);
    vkDestroyDescriptorSetLayout(device,        mapDescriptorSetLayout, NULL);
    // vkDestroyDescriptorSetLayout(device,         dfDescriptorSetLayout, NULL);
    vkDestroyDescriptorSetLayout(device,   raytraceDescriptorSetLayout, NULL);
    vkDestroyDescriptorSetLayout(device,    denoiseDescriptorSetLayout, NULL);
    vkDestroyDescriptorSetLayout(device,    upscaleDescriptorSetLayout, NULL);
    vkDestroyDescriptorSetLayout(device,  graphicalDescriptorSetLayout, NULL);

    for (int i=0; i < MAX_FRAMES_IN_FLIGHT; i++){
        vkDestroySemaphore(device,  imageAvailableSemaphores[i], NULL);
        vkDestroySemaphore(device,  renderFinishedSemaphores[i], NULL);
        vkDestroySemaphore(device, raytraceFinishedSemaphores[i], NULL);
        vkDestroySemaphore(device,  rayGenFinishedSemaphores[i], NULL);
        vkDestroyFence(device, graphicalInFlightFences[i], NULL);
        vkDestroyFence(device,   raytraceInFlightFences[i], NULL);
        vkDestroyFence(device,    rayGenInFlightFences[i], NULL);

    }
    vkDestroyCommandPool(device, commandPool, NULL);
    vkDestroyRenderPass(device, graphicalRenderPass, NULL);
    vkDestroyRenderPass(device, rayGenRenderPass, NULL);

    vkDestroyPipeline(device,   rayGenPipeline, NULL);
    // vkDestroyPipeline(device, blockifyPipeline, NULL);
    // vkDestroyPipeline(device,     copyPipeline, NULL);
    vkDestroyPipeline(device,      mapPipeline, NULL);
    // vkDestroyPipeline(device,       dfxPipeline, NULL);
    // vkDestroyPipeline(device,       dfyPipeline, NULL);
    // vkDestroyPipeline(device,       dfzPipeline, NULL);
    vkDestroyPipeline(device, raytracePipeline, NULL);
    vkDestroyPipeline(device, denoisePipeline, NULL);
    vkDestroyPipeline(device, upscalePipeline, NULL);
    vkDestroyPipeline(device, graphicsPipeline, NULL);

    vkDestroyPipelineLayout(device,   rayGenPipelineLayout, NULL);
    // vkDestroyPipelineLayout(device, blockifyLayout, NULL);
    // vkDestroyPipelineLayout(device,     copyLayout, NULL);
    vkDestroyPipelineLayout(device,      mapLayout, NULL);
    // vkDestroyPipelineLayout(device,       dfxLayout, NULL);
    // vkDestroyPipelineLayout(device,       dfyLayout, NULL);
    // vkDestroyPipelineLayout(device,       dfzLayout, NULL);
    vkDestroyPipelineLayout(device, raytraceLayout, NULL);
    vkDestroyPipelineLayout(device,  denoiseLayout, NULL);
    vkDestroyPipelineLayout(device,  upscaleLayout, NULL);
    vkDestroyPipelineLayout(device, graphicsLayout, NULL);

    vkDestroyShaderModule(device, rayGenVertShaderModule, NULL);
    vkDestroyShaderModule(device, rayGenFragShaderModule, NULL); 
    vkDestroyShaderModule(device, graphicsVertShaderModule, NULL);
    vkDestroyShaderModule(device, graphicsFragShaderModule, NULL); 
println

    cleanup_SwapchainDependent();

println
    //do before destroyDevice
    vmaDestroyAllocator(VMAllocator);
    vkDestroyDevice(device, NULL);
    //"unpicking" physical device is unnessesary :)
    vkDestroySurfaceKHR(instance, surface, NULL);
    #ifndef VKNDEBUG
    vkDestroyDebugUtilsMessengerEXT(instance, debugMessenger, NULL);
    #endif
println
    vkDestroyInstance(instance, NULL);
println
    glfwDestroyWindow(window.pointer);
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
    swapchain_images.resize(imageCount);
    vector<VkImage> imgs (imageCount);

    vkGetSwapchainImagesKHR(device, swapchain, &imageCount, imgs.data());
    for(int i=0; i<imageCount; i++){
        swapchain_images[i].image = imgs[i];
    }


    swapChainImageFormat = surfaceFormat.format;
    swapChainExtent = extent;
    
    raytraceExtent.width  = swapChainExtent.width  / _ratio.x;
    raytraceExtent.height = swapChainExtent.height / _ratio.y;
}

void Renderer::cleanup_SwapchainDependent(){
    #ifdef STENCIL
    for (int i=0; i<MAX_FRAMES_IN_FLIGHT; i++){
         vkDestroyImageView(device,    depthViews[i], NULL);
         vkDestroyImageView(device,  stencilViews[i], NULL);
        vmaDestroyImage(VMAllocator, depthStencilImages[i], depthStencilAllocs[i]);
    }
    #else
    delete_Images(&depthBuffer);
    #endif
    if(is_scaled){
        delete_Images(&upscaled_frame);    
        delete_Images(&matNorm);
    }
    delete_Images(&matNorm_downscaled);

    delete_Images(&oldUv);
    delete_Images(&step_count);
    // delete_Images(&raytraced_frame);
    // delete_Images(&denoised_frame);
    delete_Images(&frame);
    
    for (auto framebuffer : rayGenFramebuffers) {
        vkDestroyFramebuffer(device, framebuffer, NULL);
    }
    for (auto framebuffer : swapChainFramebuffers) {
        vkDestroyFramebuffer(device, framebuffer, NULL);
    }
    for (auto img : swapchain_images) {
        vkDestroyImageView(device, img.view, NULL);
    }
    vkDestroySwapchainKHR(device, swapchain, NULL);
}
void Renderer::create_SwapchainDependent(){
    // create_Image_Storages(&raytraced_frame,
    //     VK_IMAGE_TYPE_2D,
    //     RAYTRACED_IMAGE_FORMAT,
    //     VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
    //     VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
    //     VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
    //     VK_IMAGE_ASPECT_COLOR_BIT,
    //     VK_IMAGE_LAYOUT_GENERAL,
    //     VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
    //     VK_ACCESS_SHADER_WRITE_BIT,
    //     {raytraceExtent.width, raytraceExtent.height, 1});
    // create_Image_Storages(&denoised_frame,
    //     VK_IMAGE_TYPE_2D,
    //     RAYTRACED_IMAGE_FORMAT,
    //     VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
    //     VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
    //     VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
    //     VK_IMAGE_ASPECT_COLOR_BIT,
    //     VK_IMAGE_LAYOUT_GENERAL,
    //     VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
    //     VK_ACCESS_SHADER_WRITE_BIT, 
    //     {raytraceExtent.width, raytraceExtent.height, 1});
    create_Image_Storages(&frame,
        VK_IMAGE_TYPE_2D,
        RAYTRACED_IMAGE_FORMAT,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT,
        VK_IMAGE_LAYOUT_GENERAL,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_ACCESS_SHADER_WRITE_BIT, 
        {raytraceExtent.width, raytraceExtent.height, 1});


    create_Image_Storages(&oldUv,
        VK_IMAGE_TYPE_2D,
        OLD_UV_FORMAT,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT,
        VK_IMAGE_LAYOUT_GENERAL,
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT, 
        {swapChainExtent.width, swapChainExtent.height, 1});
    if(is_scaled){
        // create_Image_Storages(&oldUv_downscaled,
        //     VK_IMAGE_TYPE_2D,
        //     OLD_UV_FORMAT,
        //     VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        //     VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        //     VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        //     VK_IMAGE_ASPECT_COLOR_BIT,
        //     VK_IMAGE_LAYOUT_GENERAL,
        //     VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        //     VK_ACCESS_SHADER_WRITE_BIT, 
        //     {raytraceExtent.width, raytraceExtent.height, 1});

        
        create_Image_Storages(&matNorm,
            VK_IMAGE_TYPE_2D,
            VK_FORMAT_R8G8B8A8_SNORM,
            VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
            VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
            VK_IMAGE_ASPECT_COLOR_BIT,
            VK_IMAGE_LAYOUT_GENERAL,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_ACCESS_SHADER_WRITE_BIT | VK_ACCESS_SHADER_READ_BIT, 
            {swapChainExtent.width, swapChainExtent.height, 1});
        create_Image_Storages(&matNorm_downscaled,
            VK_IMAGE_TYPE_2D,
            VK_FORMAT_R8G8B8A8_SNORM,
            VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
            VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
            VK_IMAGE_ASPECT_COLOR_BIT,
            VK_IMAGE_LAYOUT_GENERAL,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_ACCESS_SHADER_WRITE_BIT, 
            {raytraceExtent.width, raytraceExtent.height, 1});

        create_Image_Storages(&upscaled_frame,
            VK_IMAGE_TYPE_2D,
            FINAL_IMAGE_FORMAT,
            VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
            VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
            VK_IMAGE_ASPECT_COLOR_BIT,
            VK_IMAGE_LAYOUT_GENERAL,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_ACCESS_SHADER_WRITE_BIT, 
            {swapChainExtent.width, swapChainExtent.height, 1});
    } else {
        //dont need matNorm
        create_Image_Storages(&matNorm_downscaled,
            VK_IMAGE_TYPE_2D,
            VK_FORMAT_R8G8B8A8_SNORM,
            VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
            VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
            VK_IMAGE_ASPECT_COLOR_BIT,
            VK_IMAGE_LAYOUT_GENERAL,
            VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            VK_ACCESS_SHADER_WRITE_BIT, 
            {swapChainExtent.width, swapChainExtent.height, 1});
    }
    #ifdef STENCIL
    create_Image_Storages_DepthStencil(&depthStencilImages, &depthStencilAllocs, &depthViews, &stencilViews,
        VK_IMAGE_TYPE_2D,
        DEPTH_FORMAT,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT,
        DEPTH_LAYOUT,
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        // {swapChainExtent.height, swapChainExtent.width, 1});
        {swapChainExtent.width, swapChainExtent.height, 1});
    #else
    create_Image_Storages(&depthBuffer,
        VK_IMAGE_TYPE_2D,
        DEPTH_FORMAT,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        VK_IMAGE_ASPECT_DEPTH_BIT,
        DEPTH_LAYOUT,
        VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT,
        VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT,
        // {swapChainExtent.height, swapChainExtent.width, 1});
        {swapChainExtent.width, swapChainExtent.height, 1});
    #endif

    create_Image_Storages(&step_count,
        VK_IMAGE_TYPE_2D,
        VK_FORMAT_R8G8B8A8_UINT,
        VK_IMAGE_USAGE_STORAGE_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT,
        VK_IMAGE_LAYOUT_GENERAL,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_SHADER_WRITE_BIT ,
        {(raytraceExtent.width+7) / 8, (raytraceExtent.height+7) / 8, 1}); // local_size / warp_size of raytracer

    vector<vector<VkImageView>> swapViews(1);
    // for()
    for(int i=0; i<swapchain_images.size(); i++) {
        swapViews[0].push_back(swapchain_images[i].view);
    }
    create_N_Framebuffers(&swapChainFramebuffers, &swapViews, graphicalRenderPass, swapchain_images.size(), swapChainExtent.width, swapChainExtent.height);
// printl(swapchain_images.images.size());
    // vector<vector<VkImageView>> rayGenVeiws = {gBuffer.views, rayGenNormImageViews, rayGenPosDiffImageViews, depth.view};

    vector<vector<VkImageView>> rayGenVeiws(3);
    for(int i=0; i<MAX_FRAMES_IN_FLIGHT; i++) {
        if(is_scaled){
            rayGenVeiws[0].push_back(matNorm.view);
        } else {
            rayGenVeiws[0].push_back(matNorm_downscaled[i].view);
        }
        rayGenVeiws[1].push_back(         oldUv.view);
        
        #ifdef STENCIL
        rayGenVeiws[2].push_back(    depthViews[i]);
        #else 
        rayGenVeiws[2].push_back(depthBuffer[i].view);
        #endif
    }
    create_N_Framebuffers(&rayGenFramebuffers, &rayGenVeiws, rayGenRenderPass, MAX_FRAMES_IN_FLIGHT, swapChainExtent.width, swapChainExtent.height);
}
void Renderer::recreate_SwapchainDependent(){
    int width = 0, height = 0;
    glfwGetFramebufferSize(window.pointer, &width, &height);
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(window.pointer, &width, &height);
        glfwWaitEvents();
    }
    // vkWaitForFences(device, 1, &graphicalInFlightFences[currentFrame], VK_TRUE, UINT32_MAX);
    // vkResetFences  (device, 1, &graphicalInFlightFences[currentFrame]);
    // VkSemaphoreSignalInfo si = {};
    // si.semaphore
    // vkSignalSemaphore(rayGenFinishedSemaphores[currentFrame]);
    
    vkDeviceWaitIdle(device);
    // crash(impelent function before actual usage please);
    cleanup_SwapchainDependent();

    create_Swapchain();
    create_Swapchain_Image_Views();

    create_SwapchainDependent();
    
    setup_Raytrace_Descriptors();
    setup_Denoise_Descriptors();
    if(is_scaled) {
        setup_Upscale_Descriptors();
    }
    setup_Graphical_Descriptors();
    setup_RayGen_Descriptors();
} 

void Renderer::create_Swapchain_Image_Views(){
    for(i32 i=0; i<swapchain_images.size(); i++){
        VkImageViewCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = swapchain_images[i].image;
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

        VK_CHECK(vkCreateImageView(device, &createInfo, NULL, &swapchain_images[i].view));
    }
}

void Renderer::create_N_Framebuffers(vector<VkFramebuffer>* framebuffers, vector<vector<VkImageView>>* views, VkRenderPass renderPass, u32 N, u32 Width, u32 Height){
    (*framebuffers).resize(N);

    for (u32 i = 0; i < N; i++) {
        vector<VkImageView> attachments = {};
        for (auto viewVec : *views) {
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

        VK_CHECK(vkCreateFramebuffer(device, &framebufferInfo, NULL, &(*framebuffers)[i]));
    }
}

void Renderer::start_Frame(){
    vec3 up = glm::vec3(0.0f, 0.0f, 1.0f); // Up vector

    mat4 view = glm::lookAt(camera_pos, camera_pos+camera_dir, up);
    mat4 projection = glm::ortho(-1920.f/42.f, 1920.f/42.f, 1080.f/42.f, -1080.f/42.f, -0.f, 200.f); // => *100.0 decoding
    mat4     worldToScreen = projection * view;
    // mat4 old_worldToScreen = projection * view;
        old_trans = current_trans;
    current_trans = worldToScreen;

    // struct unicopy {mat4 trans; mat4 old_trans;} unicopy = {current_trans, old_trans};
    
    // memcpy(staging_uniform_mapped[currentFrame], &unicopy, sizeof(unicopy));
    // vkDeviceWaitIdle(device);
    // vmaFlushAllocation(VMAllocator, staging_uniform[currentFrame].alloc, 0, sizeof(unicopy));

    // old_trans = old_worldToScreen;
}

void Renderer::startRaygen(){
    vkWaitForFences(device, 1, &rayGenInFlightFences[currentFrame], VK_TRUE, UINT32_MAX);
    vkResetFences  (device, 1, &rayGenInFlightFences[currentFrame]);

    VkCommandBuffer &commandBuffer = rayGenCommandBuffers[currentFrame];
    vkResetCommandBuffer(commandBuffer, 0);
    VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = 0;
        beginInfo.pInheritanceInfo = NULL;
    VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));

        vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            0,
            0, NULL,
            0, NULL,
            0, NULL
        ); 
    
    struct unicopy {mat4 trans; mat4 old_trans;} unicopy = {current_trans, old_trans};
    vkCmdUpdateBuffer(commandBuffer, uniform[currentFrame].buffer, 0, sizeof(unicopy), &unicopy);

    
    vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            0,
            0, NULL,
            0, NULL,
            0, NULL
        ); 
    VkBufferMemoryBarrier staging_uniform_barrier{};
        staging_uniform_barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        staging_uniform_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        staging_uniform_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        staging_uniform_barrier.buffer = uniform[currentFrame].buffer;
        staging_uniform_barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT| VK_ACCESS_MEMORY_READ_BIT;
        staging_uniform_barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
        staging_uniform_barrier.size = VK_WHOLE_SIZE;

    vkCmdPipelineBarrier( 
        commandBuffer,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        0,
        0, NULL,
        1, &staging_uniform_barrier,
        0, NULL
    );
    vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            0,
            0, NULL,
            0, NULL,
            0, NULL
        ); 

    vector<VkClearValue> clearColors = {
        {}, 
        // {}, 
        {}, 
        {}
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
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, rayGenPipelineLayout, 0, 1, &RayGenDescriptorSets[currentFrame], 0, 0);

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
void Renderer::RaygenMesh(Mesh *mesh){
    
    VkCommandBuffer &commandBuffer = rayGenCommandBuffers[currentFrame];
        VkBuffer vertexBuffers[] = {(*mesh).vertexes[currentFrame].buffer};
        VkDeviceSize offsets[] = {0};
    
    //m2s = w2s * m2w
    mat4 id = identity<dmat4>();
    mat4 m2s     = ((*mesh).    transform);
    // mat4 m2s     = (*mesh).transform*current_trans;
    mat4 m2s_old =     ((*mesh).old_transform);
    // mat4 m2s_old =     (*mesh).old_transform*old_trans;
    // struct {mat4 m1; mat4 m2; mat4 m3; mat4 m4;} tmp = {(*mesh).transform, (*mesh).old_transform, current_trans, old_trans};
    // struct {mat4 m1; mat4 m2; mat4 m3; mat4 m4;} tmp = {m2s, m2s_old, id, id};
    struct {mat4 m1; mat4 m2; mat4 m3; mat4 m4;} tmp = {m2s, m2s_old, current_trans, old_trans};
    //TODO:
    vkCmdPushConstants(commandBuffer, rayGenPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(tmp), &tmp);
    // vkCmdPushConstants(commandBuffer, rayGenPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, sizeof(mat4), sizeof(mat4), &(*mesh).old_transform);
    // vkcmd

    (*mesh).old_transform = (*mesh).transform;

    // glm::mult

    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
    vkCmdBindIndexBuffer(commandBuffer, (*mesh).indexes[currentFrame].buffer, 0, VK_INDEX_TYPE_UINT32);
    vkCmdDrawIndexed(commandBuffer, (*mesh).icount, 1, 0, 0, 0);
}
void Renderer::endRaygen(){
    VkCommandBuffer &commandBuffer = rayGenCommandBuffers[currentFrame];

    vkCmdEndRenderPass(commandBuffer);

    VkImageBlit blit = {};
        blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        blit.srcSubresource.layerCount = 1;
        blit.dstSubresource.layerCount = 1;
        blit.srcOffsets[0].x = 0;
        blit.srcOffsets[0].y = 0;
        blit.srcOffsets[0].z = 0;
        blit.srcOffsets[1].x = swapChainExtent.width;
        blit.srcOffsets[1].y = swapChainExtent.height;
        blit.srcOffsets[1].z = 1;

        blit.dstOffsets[0].x = 0;
        blit.dstOffsets[0].y = 0;
        blit.dstOffsets[0].z = 0;
        blit.dstOffsets[1].x = swapChainExtent.width;
        blit.dstOffsets[1].y = swapChainExtent.height;
        blit.dstOffsets[1].z = 1;
    VkFilter blit_filter = VK_FILTER_NEAREST;
    VkImageMemoryBarrier barrier = {};
        vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            0,
            0, NULL,
            0, NULL,
            0, NULL
        ); 
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER; 
        barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        if(is_scaled){
            barrier.image = matNorm.image;
        } else {
            barrier.image = matNorm_downscaled[currentFrame].image;
            // barrier.image = matNorm.image;
        }
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT  | VK_ACCESS_MEMORY_WRITE_BIT;    
        vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            0,
            0, NULL,
            0, NULL,
            1, &barrier
        );
        // barrier.image = oldUv[currentFrame].image;
        barrier.image = oldUv.image;
        vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            0,
            0, NULL,
            0, NULL,
            1, &barrier
        );
    //LOL drawing to depth twice is more efficient than drawing once and copying
    if (is_scaled) {
            blit.dstOffsets[1].x = raytraceExtent.width;
            blit.dstOffsets[1].y = raytraceExtent.height;
        // vkCmdBlitImage(commandBuffer, matNorm[currentFrame].image, VK_IMAGE_LAYOUT_GENERAL, matNorm_downscaled[currentFrame].image, VK_IMAGE_LAYOUT_GENERAL, 1, &blit, VK_FILTER_NEAREST);
        vkCmdBlitImage(commandBuffer, matNorm.image, VK_IMAGE_LAYOUT_GENERAL, matNorm_downscaled[currentFrame].image, VK_IMAGE_LAYOUT_GENERAL, 1, &blit, VK_FILTER_NEAREST);
        // vkCmdBlitImage(commandBuffer, oldUv[currentFrame].image, VK_IMAGE_LAYOUT_GENERAL, oldUv_downscaled[currentFrame].image, VK_IMAGE_LAYOUT_GENERAL, 1, &blit, VK_FILTER_LINEAR);
        // vkCmdBlitImage(commandBuffer, oldUv.image, VK_IMAGE_LAYOUT_GENERAL, oldUv_downscaled[currentFrame].image, VK_IMAGE_LAYOUT_GENERAL, 1, &blit, VK_FILTER_LINEAR);
    }

    if (is_scaled) {
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER; 
        barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT  | VK_ACCESS_MEMORY_WRITE_BIT;    
        barrier.image = matNorm_downscaled[currentFrame].image;
        vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            0,
            0, NULL,
            0, NULL,
            1, &barrier
        );
        // barrier.image = oldUv_downscaled.image;
        // vkCmdPipelineBarrier(
        //     commandBuffer,
        //     VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        //     0,
        //     0, NULL,
        //     0, NULL,
        //     1, &barrier
        // );
    } 
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER; 
        barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        // barrier.image = matNorm[currentFrame].image;
        barrier.image = matNorm_downscaled[currentFrame].image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT  | VK_ACCESS_MEMORY_WRITE_BIT;    
        vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            0,
            0, NULL,
            0, NULL,
            1, &barrier
        );
        barrier.image = oldUv.image;
        vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            0,
            0, NULL,
            0, NULL,
            1, &barrier
        );
#ifdef STENCIL
        barrier.image = depthStencilImages[currentFrame];
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
#else 
        barrier.image = depthBuffer[currentFrame].image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
#endif
        vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            0,
            0, NULL,
            0, NULL,
            1, &barrier
        );
    vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            0,
            0, NULL,
            0, NULL,
            0, NULL
        ); 

    VK_CHECK(vkEndCommandBuffer(commandBuffer));
    VkSubmitInfo submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &rayGenCommandBuffers[currentFrame];
        submitInfo.signalSemaphoreCount = 1;
        submitInfo.pSignalSemaphores = &rayGenFinishedSemaphores[currentFrame];
    VK_CHECK(vkQueueSubmit(graphicsQueue, 1, &submitInfo, rayGenInFlightFences[currentFrame]));
}
void Renderer::startCompute(){
    VkCommandBuffer &commandBuffer = computeCommandBuffers[currentFrame];
    vkWaitForFences(device, 1, &raytraceInFlightFences[currentFrame], VK_TRUE, UINT32_MAX);
    vkResetFences  (device, 1, &raytraceInFlightFences[currentFrame]);

    
    vkResetCommandBuffer(computeCommandBuffers[currentFrame], /*VkCommandBufferResetFlagBits*/ 0);
    VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = 0;
        beginInfo.pInheritanceInfo = NULL;
    VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));

    vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            0,
            0, NULL,
            0, NULL,
            0, NULL
        ); 
        
    VkImageCopy copy_op = {};
        copy_op.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copy_op.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copy_op.srcSubresource.layerCount = 1;
        copy_op.dstSubresource.layerCount = 1;
        copy_op.srcSubresource.baseArrayLayer = 0;
        copy_op.dstSubresource.baseArrayLayer = 0;
        copy_op.srcSubresource.mipLevel = 0;
        copy_op.dstSubresource.mipLevel = 0;
        copy_op.srcOffset = {0,0,0};
        copy_op.dstOffset = {0,0,0};
        copy_op.extent = {16*BLOCK_PALETTE_SIZE_X,16*BLOCK_PALETTE_SIZE_Y,16};
    vkCmdCopyImage(commandBuffer, 
        origin_block_palette[currentFrame].image,
        VK_IMAGE_LAYOUT_GENERAL, //TODO:
        block_palette.image,
        VK_IMAGE_LAYOUT_GENERAL, //TODO:
        1,
        &copy_op
    );
    VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = block_palette.image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT ;
        barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;

    vkCmdPipelineBarrier(
        commandBuffer,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        0,
        0, NULL,
        0, NULL,
        1, &barrier
    );
}
void Renderer::startBlockify(){
    VkCommandBuffer &commandBuffer = computeCommandBuffers[currentFrame];

    copy_queue.clear();

    palette_counter = 6; //TODO: change to static block count. Currently its size of 1 cause of air
    size_t size_to_copy = world_size.x * world_size.y * world_size.z * sizeof(BlockID_t);
    memcpy(current_world.data(), origin_world.data(), size_to_copy);

    vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            0,
            0, NULL,
            0, NULL,
            0, NULL
        ); 
}
struct AABB {
  vec3 min;
  vec3 max;
};
static AABB get_shift(mat4 trans, ivec3 size){
    vec3 box = vec3(size-1);
    vec3 zero = vec3(0);
    vec3 corners[8];
    corners[0] = zero;
    corners[1] = vec3(zero.x, box.y, zero.z);
    corners[2] = vec3(zero.x, box.y, box.z);
    corners[3] = vec3(zero.x, zero.y, box.z);
    corners[4] = vec3(box.x, zero.y, zero.z);
    corners[5] = vec3(box.x, box.y, zero.z);
    corners[6] = box;
    corners[7] = vec3(box.x, zero.y, box.z);
        
    // transform the first corner
    vec3 tmin = vec3(trans * vec4(corners[0],1.0));
    vec3 tmax = tmin;
        
    // transform the other 7 corners and compute the result AABB
    for(int i = 1; i < 8; i++)
    {
        vec3 point = vec3(trans * vec4(corners[i],1.0));

        tmin = min(tmin, point);
        tmax = max(tmax, point);
    }
    AABB rbox;
    
    rbox.min = tmin;
    rbox.max = tmax;
    
    return rbox;
}
// allocates temp block in palette for every block that intersects with every mesh blockified
//TODO: MAKE THIS ACTUALLY DYNAMIC
void Renderer::blockifyMesh(Mesh* mesh){
    //cpu code that does smth in mapped memory and than "flushes" to gpu

    struct AABB border_in_voxel = get_shift((*mesh).transform, (*mesh).size);
    
    struct {ivec3 min; ivec3 max;} border; 
    border.min = ivec3(border_in_voxel.min) / ivec3(16) -ivec3(1);
    border.max = ivec3(border_in_voxel.max) / ivec3(16) +ivec3(1);

    // border.min = ivec3(0);
    // border.max = ivec3(7);

    border.min = clamp(border.min, ivec3(0), ivec3(7));
    border.max = clamp(border.max, ivec3(0), ivec3(7));
    
    for(int xx = border.min.x; xx <= border.max.x; xx++){
    for(int yy = border.min.y; yy <= border.max.y; yy++){
    for(int zz = border.min.z; zz <= border.max.z; zz++){
        //if inside model TODO
        // if(
        //     any(greaterThanEqual(ivec3(xx,yy,zz), ivec3(8))) ||
        //     any(   lessThan     (ivec3(xx,yy,zz), ivec3(0)))) {
        //         continue;
        //     }
        //static blocks TODO
        int current_block = current_world(xx,yy,zz); 
        if (current_block == 0){ //if not allocated
            current_world(xx,yy,zz) = palette_counter;
            palette_counter++;
        } else {
            if(current_block <= 5) { // static and non-empty
                //add to copy queue
                ivec2 src_block = {}; 
                tie(src_block.x, src_block.y) = get_block_xy(current_block);
                ivec2 dst_block = {}; 
                tie(dst_block.x, dst_block.y) = get_block_xy(palette_counter);
                VkImageCopy 
                    static_block_copy = {};
                    static_block_copy.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                    static_block_copy.srcSubresource.baseArrayLayer = 0;
                    static_block_copy.srcSubresource.layerCount = 1;
                    static_block_copy.srcSubresource.mipLevel = 0;
                    static_block_copy.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                    static_block_copy.dstSubresource.baseArrayLayer = 0;
                    static_block_copy.dstSubresource.layerCount = 1;
                    static_block_copy.dstSubresource.mipLevel = 0;
                    static_block_copy.extent = {16,16,16};
                    static_block_copy.srcOffset = {src_block.x*16, src_block.y*16, 0};
                    static_block_copy.dstOffset = {dst_block.x*16, dst_block.y*16, 0};
                    // static_block_copy.
                    copy_queue.push_back(static_block_copy);

                    current_world(xx,yy,zz) = palette_counter;
                    palette_counter++; // yeah i dont trust myself in this  
            } else {
                //already new block, just leave it
            }
        }
        // block_image[xx + yy*8 + zz*8*8] = palette_counter++;
    }}}
}
void Renderer::endBlockify(){
    VkCommandBuffer &commandBuffer = computeCommandBuffers[currentFrame];
    // int* block_image = (int*)stagingWorldvector<BuffersMapped>[currentFrame];
    size_t size_to_copy = world_size.x * world_size.y * world_size.z * sizeof(BlockID_t);
    assert(size_to_copy != 0);
    memcpy(staging_world_mapped[currentFrame], current_world.data(), size_to_copy);
    vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            0,
            0, NULL,
            0, NULL,
            0, NULL
        ); 
    vmaFlushAllocation(VMAllocator, staging_world[currentFrame].alloc, 0, size_to_copy);

    vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            0,
            0, NULL,
            0, NULL,
            0, NULL
        ); 

    if(copy_queue.size() != 0){
        vkCmdCopyImage(commandBuffer, origin_block_palette[currentFrame].image, VK_IMAGE_LAYOUT_GENERAL, block_palette.image, VK_IMAGE_LAYOUT_GENERAL, copy_queue.size(), copy_queue.data());

        vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            0,
            0, NULL,
            0, NULL,
            0, NULL
        ); 
        VkImageMemoryBarrier block_palette_barrier{};
            block_palette_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            block_palette_barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
            block_palette_barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
            block_palette_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            block_palette_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
            block_palette_barrier.image = block_palette.image;
            block_palette_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
            block_palette_barrier.subresourceRange.baseMipLevel = 0;
            block_palette_barrier.subresourceRange.levelCount = 1;
            block_palette_barrier.subresourceRange.baseArrayLayer = 0;
            block_palette_barrier.subresourceRange.layerCount = 1;
            block_palette_barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT| VK_ACCESS_MEMORY_READ_BIT;
            block_palette_barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
        vkCmdPipelineBarrier( 
            commandBuffer,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            0,
            0, NULL,
            0, NULL,
            1, &block_palette_barrier
        );
    }
    VkBufferImageCopy copyRegion = {};
    // copyRegion.size = size;
        copyRegion.imageExtent.width  = world_size.x;
        copyRegion.imageExtent.height = world_size.y;
        copyRegion.imageExtent.depth  = world_size.z;
        copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copyRegion.imageSubresource.layerCount = 1;
    vkCmdCopyBufferToImage(commandBuffer, staging_world[currentFrame].buffer, world.image, VK_IMAGE_LAYOUT_GENERAL, 1, &copyRegion);
vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            0,
            0, NULL,
            0, NULL,
            0, NULL
        ); 
    VkImageMemoryBarrier staging_world_barrier{};
        staging_world_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        staging_world_barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        staging_world_barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        staging_world_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        staging_world_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        staging_world_barrier.image = world.image;
        staging_world_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        staging_world_barrier.subresourceRange.baseMipLevel = 0;
        staging_world_barrier.subresourceRange.levelCount = 1;
        staging_world_barrier.subresourceRange.baseArrayLayer = 0;
        staging_world_barrier.subresourceRange.layerCount = 1;
        staging_world_barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT| VK_ACCESS_MEMORY_READ_BIT;
        staging_world_barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    vkCmdPipelineBarrier( 
        commandBuffer,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        0,
        0, NULL,
        0, NULL,
        // 0, NULL
        1, &staging_world_barrier
    );
    vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            0,
            0, NULL,
            0, NULL,
            0, NULL
        ); 
}
void Renderer::blockifyCustom(void* ptr){
    size_t size_to_copy = world_size.x * world_size.y * world_size.z * sizeof(BlockID_t);
    memcpy(staging_world_mapped[currentFrame], ptr, size_to_copy);
    vmaFlushAllocation(VMAllocator, staging_world[currentFrame].alloc, 0, size_to_copy);
}

void Renderer::startMap(){
    VkCommandBuffer &commandBuffer = computeCommandBuffers[currentFrame];
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, mapPipeline);
}

void Renderer::mapMesh(Mesh* mesh){
    VkCommandBuffer &commandBuffer = computeCommandBuffers[currentFrame];
    
    //setting dynamic descriptors
        VkDescriptorImageInfo
            modelVoxelsInfo = {};
            modelVoxelsInfo.imageView = (*mesh).voxels[currentFrame].view; //CHANGE ME
            modelVoxelsInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        VkDescriptorImageInfo 
            blocksInfo = {};
            blocksInfo.imageView = world.view;
            blocksInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        VkDescriptorImageInfo 
            blockPaletteInfo = {};
            blockPaletteInfo.imageView = block_palette.view;
            blockPaletteInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

        VkWriteDescriptorSet 
            modelVoxelsWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            modelVoxelsWrite.dstSet = NULL;
            modelVoxelsWrite.dstBinding = 0;
            modelVoxelsWrite.dstArrayElement = 0;
            modelVoxelsWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            modelVoxelsWrite.descriptorCount = 1;
            modelVoxelsWrite.pImageInfo = &modelVoxelsInfo;
        VkWriteDescriptorSet 
            blocksWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            blocksWrite.dstSet = NULL;
            blocksWrite.dstBinding = 1;
            blocksWrite.dstArrayElement = 0;
            blocksWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            blocksWrite.descriptorCount = 1;
            blocksWrite.pImageInfo = &blocksInfo;
        VkWriteDescriptorSet 
            blockPaletteWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            blockPaletteWrite.dstSet = NULL;
            blockPaletteWrite.dstBinding = 2;
            blockPaletteWrite.dstArrayElement = 0;      
            blockPaletteWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            blockPaletteWrite.descriptorCount = 1;
            blockPaletteWrite.pImageInfo = &blockPaletteInfo;
        vector<VkWriteDescriptorSet> descriptorWrites = {modelVoxelsWrite, blocksWrite, blockPaletteWrite};

    vkCmdPushDescriptorSetKHR(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, mapLayout, 0, descriptorWrites.size(), descriptorWrites.data());
    struct AABB border_in_voxel = get_shift((*mesh).transform, (*mesh).size);
    
    struct {ivec3 min; ivec3 max;} border; 
    border.min = ivec3(border_in_voxel.min) -ivec3(1);
    border.max = ivec3(border_in_voxel.max) +ivec3(1);
    //todo: clamp here not in shader
    ivec3 map_area = border.max - border.min;

    // struct {mat4 trans; ivec3 size;} trans_size = {mesh.transform, mesh.size};
    struct {mat4 trans; ivec4 size;} itrans_shift = {inverse((*mesh).transform), ivec4(border.min, 0)};
    // trans = glm::scale(trans, vec3(0.95));
    vkCmdPushConstants(commandBuffer, mapLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(itrans_shift), &itrans_shift);
    // ivec3 adjusted_size = ivec3(vec3(mesh.size) * vec3(2.0));
    vkCmdDispatch(commandBuffer, (map_area.x+3) / 4, (map_area.y+3) / 4, (map_area.z+3) / 4);   
}

void Renderer::endMap(){
    VkCommandBuffer &commandBuffer = computeCommandBuffers[currentFrame];

    vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            0,
            0, NULL,
            0, NULL,
            0, NULL
        ); 

    VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = block_palette.image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT  | VK_ACCESS_MEMORY_WRITE_BIT;
    vkCmdPipelineBarrier(
        commandBuffer,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        0,
        0, NULL,
        0, NULL,
        1, &barrier
    );
    barrier.image = origin_block_palette[currentFrame].image;
    vkCmdPipelineBarrier(
        commandBuffer,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        0,
        0, NULL,
        0, NULL,
        1, &barrier
    );
    vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            0,
            0, NULL,
            0, NULL,
            0, NULL
        ); 
}

// void Renderer::recalculate_df(){
//     VkCommandBuffer &commandBuffer = computeCommandBuffers[currentFrame];

//     VkImageMemoryBarrier barrier{};
//         barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
//         barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
//         barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
//         barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
//         barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
//         barrier.image = block_palette[currentFrame].image;
//         barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
//         barrier.subresourceRange.baseMipLevel = 0;
//         barrier.subresourceRange.levelCount = 1;
//         barrier.subresourceRange.baseArrayLayer = 0;
//         barrier.subresourceRange.layerCount = 1;
//         barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT;
//         barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT  | VK_ACCESS_MEMORY_WRITE_BIT;

//     vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, dfxLayout, 0, 1, &dfDescriptorSets[currentFrame], 0, 0);
//     vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, dfxPipeline);
//         vkCmdDispatch(commandBuffer, 1, 1, 16*palette_counter);
//         vkCmdPipelineBarrier(
//             commandBuffer,
//             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
//             0,
//             0, NULL,
//             0, NULL,
//             1, &barrier
//         );
//     vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, dfyLayout, 0, 1, &dfDescriptorSets[currentFrame], 0, 0);
//     vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, dfyPipeline);
//         vkCmdDispatch(commandBuffer, 1, 1, 16*palette_counter);
//         vkCmdPipelineBarrier(
//             commandBuffer,
//             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
//             0,
//             0, NULL,
//             0, NULL,
//             1, &barrier
//         ); 
//     vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, dfzLayout, 0, 1, &dfDescriptorSets[currentFrame], 0, 0);
//     vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, dfzPipeline);
//         vkCmdDispatch(commandBuffer, 1, 1, 16*palette_counter);
//         vkCmdPipelineBarrier(
//             commandBuffer,
//             VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
//             0,
//             0, NULL,
//             0, NULL,
//             1, &barrier
//         );
// }
void Renderer::raytrace(){
    VkCommandBuffer &commandBuffer = computeCommandBuffers[currentFrame];
    vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            0,
            0, NULL,
            0, NULL,
            0, NULL
        ); 

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, raytracePipeline);
        // transition_Image_Layout_Cmdb(commandBuffer, raytraced_frame[currentFrame].image, RAYTRACED_IMAGE_FORMAT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
        //     VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        //     0, VK_ACCESS_SHADER_WRITE_BIT);

        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, raytraceLayout, 0, 1, &raytraceDescriptorSets[currentFrame], 0, 0);

        itime++;
        old_camera_pos = camera_pos;
        old_camera_dir = camera_dir;
        struct rtpc {ivec4 i; vec4 v1, v2, v3, v4;} rtpc = {ivec4(itime,0,0,0), vec4(camera_pos,0), vec4(camera_dir,0), vec4(old_camera_pos, 0), vec4(old_camera_dir, 0)};
        vkCmdPushConstants(commandBuffer, raytraceLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(rtpc), &rtpc);

        vkCmdDispatch(commandBuffer, raytraceExtent.width/8, raytraceExtent.height/8, 1);

    VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        // barrier.image = raytraced_frame[currentFrame].image;
        barrier.image = frame[currentFrame].image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT  | VK_ACCESS_MEMORY_WRITE_BIT;    
    vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            0,
            0, NULL,
            0, NULL,
            1, &barrier
        );    
        barrier.image = step_count.image;
    vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            0,
            0, NULL,
            0, NULL,
            1, &barrier
        );   
        vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            0,
            0, NULL,
            0, NULL,
            0, NULL
        ); 
    // barrier = {};
    //     barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    //     barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    //     barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    //     barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    //     barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    //     barrier.image = denoised_frame[previousFrame].image;
    //     barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    //     barrier.subresourceRange.baseMipLevel = 0;
    //     barrier.subresourceRange.levelCount = 1;
    //     barrier.subresourceRange.baseArrayLayer = 0;
    //     barrier.subresourceRange.layerCount = 1;
    //     barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT;
    //     barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT  | VK_ACCESS_MEMORY_WRITE_BIT;    
    // vkCmdPipelineBarrier(
    //         commandBuffer,
    //         VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
    //         0,
    //         0, NULL,
    //         0, NULL,
    //         1, &barrier
    //     );    
    barrier = {};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        // barrier.image = denoised_frame[currentFrame].image;
        barrier.image = frame[currentFrame].image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT  | VK_ACCESS_MEMORY_WRITE_BIT;    
    vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            0,
            0, NULL,
            0, NULL,
            1, &barrier
        );   
    vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            0,
            0, NULL,
            0, NULL,
            0, NULL
        ); 
}
void Renderer::denoise(int iterations){
    VkCommandBuffer &commandBuffer = computeCommandBuffers[currentFrame];
    vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            0,
            0, NULL,
            0, NULL,
            0, NULL
        ); 
    VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        // barrier.image = denoised_frame[currentFrame].image;
        barrier.image = frame[currentFrame].image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT |VK_ACCESS_MEMORY_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, denoisePipeline);
        // transition_Image_Layout_Cmdb(commandBuffer, raytraced_frame[currentFrame].image, RAYTRACED_IMAGE_FORMAT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
        //     VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        //     0, VK_ACCESS_SHADER_WRITE_BIT);

        // vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, raytracePipeline);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, denoiseLayout, 0, 1, &denoiseDescriptorSets[currentFrame], 0, 0);

        for(int atrous_iter = 0; atrous_iter<iterations; atrous_iter++){
        // for(int atrous_iter = 5; atrous_iter>0; atrous_iter--){
            vkCmdPushConstants(commandBuffer, denoiseLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(int), &atrous_iter);
            // vkCmdPushConstants()
            vkCmdDispatch(commandBuffer, raytraceExtent.width/8, raytraceExtent.height/8, 1);
            vkCmdPipelineBarrier(
                    commandBuffer,
                    VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                    0,
                    0, NULL,
                    0, NULL,
                    1, &barrier
                ); 
        } 

    barrier = {};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        // barrier.image = raytraced_frame[currentFrame].image;
        barrier.image = frame[currentFrame].image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT  | VK_ACCESS_MEMORY_WRITE_BIT;    
    vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            0,
            0, NULL,
            0, NULL,
            1, &barrier
        ); 
    vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            0,
            0, NULL,
            0, NULL,
            0, NULL
        ); 
}
void Renderer::upscale(){
    if(!is_scaled) crash(upscale with ratio 1? Are you stupid?);
    
    VkCommandBuffer &commandBuffer = computeCommandBuffers[currentFrame];

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, upscalePipeline);
        // transition_Image_Layout_Cmdb(commandBuffer, raytraced_frame[currentFrame].image, RAYTRACED_IMAGE_FORMAT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
        //     VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        //     0, VK_ACCESS_SHADER_WRITE_BIT);

        // vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, raytracePipeline);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, upscaleLayout, 0, 1, &upscaleDescriptorSets[currentFrame], 0, 0);

        vkCmdDispatch(commandBuffer, (swapChainExtent.width)/8, (swapChainExtent.height)/8, 1);

    VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = upscaled_frame.image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT  | VK_ACCESS_MEMORY_WRITE_BIT;    
    vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            0,
            0, NULL,
            0, NULL,
            1, &barrier
        ); 
}
void Renderer::endCompute(){
    VkCommandBuffer &commandBuffer = computeCommandBuffers[currentFrame];
    vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            0,
            0, NULL,
            0, NULL,
            0, NULL
        ); 
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
            submitInfo.pSignalSemaphores = &raytraceFinishedSemaphores[currentFrame];
    VK_CHECK(vkQueueSubmit(computeQueue, 1, &submitInfo, raytraceInFlightFences[currentFrame]));
}
void Renderer::present(){
    VkCommandBuffer &commandBuffer = graphicalCommandBuffers[currentFrame];

    VkResult result = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        // cout << KRED"failed to present swap chain image!\n" KEND;
        recreate_SwapchainDependent();
        return; // can be avoided, but it is just 1 frame 
    } else if ((result != VK_SUCCESS)) {
        printf(KRED "failed to acquire swap chain image!\n" KEND);
        exit(result);
    }
    vkWaitForFences(device, 1, &graphicalInFlightFences[currentFrame], VK_TRUE, UINT32_MAX);
    vkResetFences  (device, 1, &graphicalInFlightFences[currentFrame]);

    vkResetCommandBuffer(graphicalCommandBuffers[currentFrame], 0);
    VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = 0;
        beginInfo.pInheritanceInfo = NULL;

    VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));

    // transition_Image_Layout_Cmdb(commandBuffer, raytraced_frame[currentFrame].image, RAYTRACED_IMAGE_FORMAT, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
    //     VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
    //     0, VK_ACCESS_SHADER_READ_BIT
    // );

    VkClearValue clearColor = {{{0.111f, 0.444f, 0.666f, 1.0f}}};
    VkRenderPassBeginInfo renderPassInfo = {};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = graphicalRenderPass;
        renderPassInfo.framebuffer = swapChainFramebuffers[imageIndex];
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = swapChainExtent;
        renderPassInfo.clearValueCount = 1;
        renderPassInfo.pClearValues = &clearColor;
    
    vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            0,
            0, NULL,
            0, NULL,
            0, NULL
        ); 
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

vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            0,
            0, NULL,
            0, NULL,
            0, NULL
        ); 

    VK_CHECK(vkEndCommandBuffer(commandBuffer));

    // vector<VkSemaphore> waitSemaphores = {imageAvailableSemaphores[currentFrame]};
    vector<VkSemaphore> waitSemaphores = {raytraceFinishedSemaphores[currentFrame], imageAvailableSemaphores[currentFrame]};
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
        presentInfo.pResults = NULL;

    result = vkQueuePresentKHR(presentQueue, &presentInfo);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || is_resized) {
        is_resized = false; 
        // cout << KRED"failed to present swap chain image!\n" KEND;
        recreate_SwapchainDependent();
    } else if (result != VK_SUCCESS) {
        cout << KRED"failed to present swap chain image!\n" KEND;
        exit(result);
    }
}
void Renderer::end_Frame(){
    previousFrame = currentFrame;
     currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    nextFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
}

void Renderer::create_Image_Storages_DepthStencil(vector<VkImage>* images, vector<VmaAllocation>* allocs, vector<VkImageView>* depthViews, vector<VkImageView>* stencilViews,
    VkImageType type, VkFormat format, VkImageUsageFlags usage, VmaMemoryUsage vma_usage, VmaAllocationCreateFlags vma_flags, VkImageAspectFlags aspect, VkImageLayout layout, VkPipelineStageFlags pipeStage, VkAccessFlags access, 
    uvec3 size){
    (*images).resize(MAX_FRAMES_IN_FLIGHT);
    (*depthViews).resize(MAX_FRAMES_IN_FLIGHT);
    (*stencilViews).resize(MAX_FRAMES_IN_FLIGHT);
    (*allocs).resize(MAX_FRAMES_IN_FLIGHT);
    
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
            imageInfo.extent.width  = size.x;
            imageInfo.extent.height = size.y;
            imageInfo.extent.depth  = size.z;
            imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        VmaAllocationCreateInfo allocInfo = {};
            allocInfo.usage = vma_usage;
            allocInfo.flags = vma_flags;
            // allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
            // allocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;

            // allocInfo.requiredFlags = 
        VK_CHECK(vmaCreateImage(VMAllocator, &imageInfo, &allocInfo, &(*images)[i], &(*allocs)[i], NULL));

        VkImageViewCreateInfo depthViewInfo = {};
            depthViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            depthViewInfo.image = (*images)[i];
            depthViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            depthViewInfo.format = format;
            depthViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
            depthViewInfo.subresourceRange.baseMipLevel = 0;
            depthViewInfo.subresourceRange.baseArrayLayer = 0;
            depthViewInfo.subresourceRange.levelCount = 1;
            depthViewInfo.subresourceRange.layerCount = 1;
        VK_CHECK(vkCreateImageView(device, &depthViewInfo, NULL, &(*depthViews)[i]));
        VkImageViewCreateInfo stencilViewInfo = {};
            stencilViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            stencilViewInfo.image = (*images)[i];
            stencilViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            stencilViewInfo.format = format;
            stencilViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;
            stencilViewInfo.subresourceRange.baseMipLevel = 0;
            stencilViewInfo.subresourceRange.baseArrayLayer = 0;
            stencilViewInfo.subresourceRange.levelCount = 1;
            stencilViewInfo.subresourceRange.layerCount = 1;
        VK_CHECK(vkCreateImageView(device, &stencilViewInfo, NULL, &(*stencilViews)[i]));

        transition_Image_Layout_Singletime((*images)[i], format, aspect, VK_IMAGE_LAYOUT_UNDEFINED, layout,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, pipeStage,
            0, access);
    }
}

void Renderer::create_Image_Storages(vector<Image>* images, 
    VkImageType type, VkFormat format, VkImageUsageFlags usage, VmaMemoryUsage vma_usage, VmaAllocationCreateFlags vma_flags, VkImageAspectFlags aspect, VkImageLayout layout, VkPipelineStageFlags pipeStage, VkAccessFlags access, 
    uvec3 size){
    (*images).resize(MAX_FRAMES_IN_FLIGHT);
    
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
            imageInfo.extent.width  = size.x;
            imageInfo.extent.height = size.y;
            imageInfo.extent.depth  = size.z;
            imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        VmaAllocationCreateInfo allocInfo = {};
            allocInfo.usage = vma_usage;
            allocInfo.flags = vma_flags;
            // allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
            // allocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;

            // allocInfo.requiredFlags = 
        VK_CHECK(vmaCreateImage(VMAllocator, &imageInfo, &allocInfo, &(*images)[i].image, &(*images)[i].alloc, NULL));

        VkImageViewCreateInfo viewInfo = {};
            viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            viewInfo.image = (*images)[i].image;
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
        VK_CHECK(vkCreateImageView(device, &viewInfo, NULL, &(*images)[i].view));

        transition_Image_Layout_Singletime((*images)[i].image, format, aspect, VK_IMAGE_LAYOUT_UNDEFINED, layout,
            VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, pipeStage,
            0, access);
        // if(!(vma_usage & VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT)){
        // }
    }
}
void Renderer::create_Image_Storages(Image* image, 
    VkImageType type, VkFormat format, VkImageUsageFlags usage, VmaMemoryUsage vma_usage, VmaAllocationCreateFlags vma_flags, VkImageAspectFlags aspect, VkImageLayout layout, VkPipelineStageFlags pipeStage, VkAccessFlags access, 
    uvec3 size){

    VkImageCreateInfo imageInfo = {};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = type;
        imageInfo.format = format;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
        imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;

        imageInfo.usage = usage;
        imageInfo.extent.width  = size.x;
        imageInfo.extent.height = size.y;
        imageInfo.extent.depth  = size.z;
        imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    VmaAllocationCreateInfo allocInfo = {};
        allocInfo.usage = vma_usage;
        allocInfo.flags = vma_flags;

    VK_CHECK(vmaCreateImage(VMAllocator, &imageInfo, &allocInfo, &image->image, &image->alloc, NULL));

    VkImageViewCreateInfo viewInfo = {};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = image->image;
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
    VK_CHECK(vkCreateImageView(device, &viewInfo, NULL, &image->view));

    transition_Image_Layout_Singletime(image->image, format, aspect, VK_IMAGE_LAYOUT_UNDEFINED, layout,
        VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, pipeStage,
        0, access);
}

//fill manually with cmd or copy
void Renderer::create_Buffer_Storages(vector<Buffer>* buffers, VkBufferUsageFlags usage, u32 size, bool host){
    (*buffers).resize(MAX_FRAMES_IN_FLIGHT);
    // allocs.resize(MAX_FRAMES_IN_FLIGHT);
    
    for (i32 i=0; i < MAX_FRAMES_IN_FLIGHT; i++){
        VkBufferCreateInfo bufferInfo = {};
            bufferInfo.size = size;
            bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            bufferInfo.usage = usage;
        VmaAllocationCreateInfo allocInfo = {};
            // if (required_flags == VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT) {
            // }
            // allocInfo.requiredFlags = required_flags;
            if(host) {
                allocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
            }
            allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        VK_CHECK(vmaCreateBuffer(VMAllocator, &bufferInfo, &allocInfo, &(*buffers)[i].buffer, &(*buffers)[i].alloc, NULL));
    }
}

//TODO: need to find memory, verify flags
tuple<vector<Buffer>, vector<Buffer>> Renderer::create_RayGen_VertexBuffers(vector<Vertex> vertices, vector<u32> indices){
    return create_RayGen_VertexBuffers(vertices.data(), vertices.size(), indices.data(), indices.size());
}

tuple<vector<Buffer>, vector<Buffer>> Renderer::create_RayGen_VertexBuffers(Vertex* vertices, u32 vcount, u32* indices, u32 icount){
    VkDeviceSize bufferSizeV = sizeof(Vertex)*vcount;
    // cout << bufferSizeV;
    VkDeviceSize bufferSizeI = sizeof(u32   )*icount;

    vector<Buffer> vertexes (MAX_FRAMES_IN_FLIGHT);
    vector<Buffer>  indexes (MAX_FRAMES_IN_FLIGHT);

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
assert (VMAllocator);
assert (stagingAllocationV);
assert (&data);
    vmaMapMemory(VMAllocator, stagingAllocationV, &data);
        memcpy(data, vertices, bufferSizeV);
    vmaUnmapMemory(VMAllocator, stagingAllocationV);

    vmaMapMemory(VMAllocator, stagingAllocationI, &data);
        memcpy(data, indices, bufferSizeI);
    vmaUnmapMemory(VMAllocator, stagingAllocationI);

    for(i32 i=0; i<MAX_FRAMES_IN_FLIGHT; i++){
        VkBufferCreateInfo bufferInfo = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
            bufferInfo.size = bufferSizeV;
            bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
        VmaAllocationCreateInfo allocInfo = {};
            allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
        vmaCreateBuffer(VMAllocator, &bufferInfo, &allocInfo, &vertexes[i].buffer, &vertexes[i].alloc, NULL);
            bufferInfo.size = bufferSizeI;
            bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
        vmaCreateBuffer(VMAllocator, &bufferInfo, &allocInfo, &indexes[i].buffer, &indexes[i].alloc, NULL);
               
        copy_Buffer(stagingBufferV, vertexes[i].buffer, bufferSizeV);
        copy_Buffer(stagingBufferI,  indexes[i].buffer, bufferSizeI);
    }

    vmaDestroyBuffer(VMAllocator, stagingBufferV, stagingAllocationV);
    vmaDestroyBuffer(VMAllocator, stagingBufferI, stagingAllocationI);

    return {vertexes, indexes};
}

vector<Image> Renderer::create_RayTrace_VoxelImages(u16* voxels, ivec3 size){
    VkDeviceSize bufferSize = (sizeof(u8)*2)*size.x*size.y*size.z;

    vector<Image> voxelImages = {};

    create_Image_Storages(&voxelImages,
        VK_IMAGE_TYPE_3D,
        VK_FORMAT_R8G8_UINT,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT,
        VK_IMAGE_LAYOUT_GENERAL,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_ACCESS_SHADER_READ_BIT | VK_ACCESS_MEMORY_READ_BIT,
        size);


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
        memcpy(data, voxels, bufferSize);
    vmaUnmapMemory(VMAllocator, stagingAllocation);

    for(i32 i=0; i<MAX_FRAMES_IN_FLIGHT; i++){
        copy_Buffer(stagingBuffer, voxelImages[i].image, size, VK_IMAGE_LAYOUT_GENERAL);
    }
    vmaDestroyBuffer(VMAllocator, stagingBuffer, stagingAllocation);
    return voxelImages;
}

static u32 STORAGE_BUFFER_DESCRIPTOR_COUNT = 0;
static u32 STORAGE_IMAGE_DESCRIPTOR_COUNT = 0;
static u32 COMBINED_IMAGE_SAMPLER_DESCRIPTOR_COUNT = 0;
static u32 UNIFORM_BUFFER_DESCRIPTOR_COUNT = 0;
static void count_descriptor(const VkDescriptorType type){
    if(type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER){
        STORAGE_BUFFER_DESCRIPTOR_COUNT++;
    } else if(type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER){
        COMBINED_IMAGE_SAMPLER_DESCRIPTOR_COUNT++;
    } else if(type == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE){
        STORAGE_IMAGE_DESCRIPTOR_COUNT++;
    } else if(type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER){
        UNIFORM_BUFFER_DESCRIPTOR_COUNT++;    
    } else {
        cout << KRED "ADD DESCRIPTOR TO COUNTER\n" KEND;
        abort();
    }
}
//in order
static void create_Descriptor_Set_Layout_Helper(vector<VkDescriptorType> descriptorTypes, VkShaderStageFlags stageFlags, VkDescriptorSetLayout& layout, VkDevice device, VkDescriptorSetLayoutCreateFlags flags = 0){
    vector<VkDescriptorSetLayoutBinding> bindings = {};

    for (i32 i=0; i<descriptorTypes.size(); i++) {
        count_descriptor(descriptorTypes[i]);
        
        VkDescriptorSetLayoutBinding 
            bind = {};
            bind.binding = i;
            bind.descriptorType = descriptorTypes[i];
            bind.descriptorCount = 1;
            bind.stageFlags = stageFlags;
        bindings.push_back(bind);
    }

    VkDescriptorSetLayoutCreateInfo 
        layoutInfo = {};
        layoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        layoutInfo.flags = flags;
        layoutInfo.bindingCount = bindings.size();
        layoutInfo.pBindings    = bindings.data();
    VK_CHECK(vkCreateDescriptorSetLayout(device, &layoutInfo, NULL, &layout));
}

void Renderer::create_Descriptor_Set_Layouts(){
    //descriptors are counted after this
    // create_Descriptor_Set_Layout_Helper({
    //     VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, //palette counter
    //     VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, //copy buffer - XYZ for indirect and copy operaitions
    //     VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,  //blocks
    //     }, 
    //     VK_SHADER_STAGE_COMPUTE_BIT, blockifyDescriptorSetLayout, device);
    
    // create_Descriptor_Set_Layout_Helper({
    //     VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,  //1frame voxel palette
    //     VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, //copy buffer - XYZ for indirect and copy operaitions
    //     }, 
    //     VK_SHADER_STAGE_COMPUTE_BIT, copyDescriptorSetLayout, device);

    create_Descriptor_Set_Layout_Helper({
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, // modelVoxels
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, // blocks
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, // blockPalette
        }, 
        VK_SHADER_STAGE_COMPUTE_BIT, mapDescriptorSetLayout, device,
        VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR);
        
    // create_Descriptor_Set_Layout_Helper({
    //     VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, // blockPalette
    //     }, 
    //     VK_SHADER_STAGE_COMPUTE_BIT, dfDescriptorSetLayout, device);

    create_Descriptor_Set_Layout_Helper({
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, //mat norm (downscaled)
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, //old uv   (downscaled)
        // VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, //depth    (downscaled)
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, //depth    (!downscaled)
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, //old depth    (!downscaled)
        // VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, //old mat norm (downscaled)
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, //old mat norm (downscaled)

        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, //blocks
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, //block palette
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, //mat palette
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, //step counts
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, //out frame (raytraced)
        // VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, //in previous frame (raytraced) to temporaly denoise
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, //in previous frame (raytraced) to temporaly denoise
        // VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, //in previous frame (raytraced) to temporaly denoise
        }, 
        VK_SHADER_STAGE_COMPUTE_BIT, raytraceDescriptorSetLayout, device);

    create_Descriptor_Set_Layout_Helper({
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, //mat norm (downscaled)
        // VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, //depth    (downscaled)
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, //depth    (downscaled)
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, //temp_denoised raytraced
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, //svgf denoised (final output)
        }, 
        VK_SHADER_STAGE_COMPUTE_BIT, denoiseDescriptorSetLayout, device);

    create_Descriptor_Set_Layout_Helper({
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, //mat norm (downscaled)
        // VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, //depth    (downscaled)
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, //mat norm (highres)
        // VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, //depth    (highres)
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, //denoised in
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, //upscaled out
        }, 
        VK_SHADER_STAGE_COMPUTE_BIT, upscaleDescriptorSetLayout, device);

    create_Descriptor_Set_Layout_Helper({
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, //frame-constant world_to_screen mat4
        }, 
        VK_SHADER_STAGE_VERTEX_BIT, RayGenDescriptorSetLayout, device);

    create_Descriptor_Set_Layout_Helper({
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, //in frame (raytraced)
        }, 
        VK_SHADER_STAGE_FRAGMENT_BIT, graphicalDescriptorSetLayout, device);
}

void Renderer::create_Descriptor_Pool(){
    printl(STORAGE_IMAGE_DESCRIPTOR_COUNT);
    printl(COMBINED_IMAGE_SAMPLER_DESCRIPTOR_COUNT);
    printl(STORAGE_BUFFER_DESCRIPTOR_COUNT);
    printl(UNIFORM_BUFFER_DESCRIPTOR_COUNT);

    VkDescriptorPoolSize STORAGE_IMAGE_PoolSize = {};
        STORAGE_IMAGE_PoolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        STORAGE_IMAGE_PoolSize.descriptorCount = STORAGE_IMAGE_DESCRIPTOR_COUNT*8;
    VkDescriptorPoolSize COMBINED_IMAGE_SAMPLER_PoolSize = {};
        COMBINED_IMAGE_SAMPLER_PoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        COMBINED_IMAGE_SAMPLER_PoolSize.descriptorCount = COMBINED_IMAGE_SAMPLER_DESCRIPTOR_COUNT*8;
    VkDescriptorPoolSize STORAGE_BUFFER_PoolSize = {};
        STORAGE_BUFFER_PoolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        STORAGE_BUFFER_PoolSize.descriptorCount = STORAGE_BUFFER_DESCRIPTOR_COUNT*8;
    VkDescriptorPoolSize UNIFORM_BUFFER_PoolSize = {};
        UNIFORM_BUFFER_PoolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        UNIFORM_BUFFER_PoolSize.descriptorCount = UNIFORM_BUFFER_DESCRIPTOR_COUNT*8;

    vector<VkDescriptorPoolSize> poolSizes = {
        STORAGE_IMAGE_PoolSize, 
        COMBINED_IMAGE_SAMPLER_PoolSize, 
        // STORAGE_BUFFER_PoolSize,
        UNIFORM_BUFFER_PoolSize};

    VkDescriptorPoolCreateInfo poolInfo = {};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = poolSizes.size();
        poolInfo.pPoolSizes = poolSizes.data();
        poolInfo.maxSets = MAX_FRAMES_IN_FLIGHT*8; //becuase frames_in_flight multiply of 5 differents sets, each for shader 

    VK_CHECK(vkCreateDescriptorPool(device, &poolInfo, NULL, &descriptorPool));
}

static void allocate_Descriptors_helper(vector<VkDescriptorSet>& sets, VkDescriptorSetLayout layout, VkDescriptorPool pool, VkDevice device){
    sets.resize(MAX_FRAMES_IN_FLIGHT);
    vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, layout);
    VkDescriptorSetAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = pool;
        allocInfo.descriptorSetCount = layouts.size();
        allocInfo.pSetLayouts        = layouts.data();
    VK_CHECK(vkAllocateDescriptorSets(device, &allocInfo, sets.data()));
}
void Renderer::allocate_Descriptors(){
    allocate_Descriptors_helper( raytraceDescriptorSets,  raytraceDescriptorSetLayout, descriptorPool, device); 
    allocate_Descriptors_helper(  denoiseDescriptorSets,   denoiseDescriptorSetLayout, descriptorPool, device); 
    allocate_Descriptors_helper(  upscaleDescriptorSets,   upscaleDescriptorSetLayout, descriptorPool, device); 
    // allocate_Descriptors_helper( blockifyDescriptorSets,  blockifyDescriptorSetLayout, descriptorPool, device); 
    // allocate_Descriptors_helper(     copyDescriptorSets,      copyDescriptorSetLayout, descriptorPool, device); 
    //we do not allocate this because it's descriptors are managed trough push_descriptor mechanism
    // allocate_Descriptors_helper(      mapDescriptorSets,       mapDescriptorSetLayout, descriptorPool, device); 
    // allocate_Descriptors_helper(     dfDescriptorSets,      dfDescriptorSetLayout, descriptorPool, device); 
    allocate_Descriptors_helper(graphicalDescriptorSets, graphicalDescriptorSetLayout, descriptorPool, device);     
    allocate_Descriptors_helper(RayGenDescriptorSets, RayGenDescriptorSetLayout, descriptorPool, device);     
}
// void Renderer::setup_Copy_Descriptors(){
//     for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
//         VkDescriptorImageInfo inputBlockPaletteInfo = {};
//             inputBlockPaletteInfo.imageView = block_palette[i].view;
//             inputBlockPaletteInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
//         VkDescriptorBufferInfo copyCounterBufferInfo = {};
//             copyCounterBufferInfo.buffer = copyCounterBuffers[i];
//             copyCounterBufferInfo.offset = 0;
//             copyCounterBufferInfo.range = VK_WHOLE_SIZE;

//         VkWriteDescriptorSet blockPaletteWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
//             blockPaletteWrite.dstSet = copyDescriptorSets[i];
//             blockPaletteWrite.dstBinding = 0;
//             blockPaletteWrite.dstArrayElement = 0;
//             blockPaletteWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
//             blockPaletteWrite.descriptorCount = 1;
//             blockPaletteWrite.pImageInfo = &inputBlockPaletteInfo;
//         VkWriteDescriptorSet copyCounterWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
//             copyCounterWrite.dstSet = copyDescriptorSets[i];
//             copyCounterWrite.dstBinding = 1;
//             copyCounterWrite.dstArrayElement = 0;
//             copyCounterWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
//             copyCounterWrite.descriptorCount = 1;
//             copyCounterWrite.pBufferInfo = &copyCounterBufferInfo;
//         vector<VkWriteDescriptorSet> descriptorWrites = {blockPaletteWrite, copyCounterWrite};

//         vkUpdateDescriptorSets(device, descriptorWrites.size(), descriptorWrites.data(), 0, NULL);
//     }
// }
//SHOULD NOT EXIST push descriptors to command buffer instead
void Renderer::setup_Map_Descriptors(){
    // for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
    // }
}
// void Renderer::setup_Df_Descriptors(){
//     for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
//         VkDescriptorImageInfo inputBlockPaletteInfo = {};
//             inputBlockPaletteInfo.imageView = block_palette[i].view;
//             inputBlockPaletteInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

//         VkWriteDescriptorSet blockPaletteWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
//             blockPaletteWrite.dstSet = dfDescriptorSets[i];
//             blockPaletteWrite.dstBinding = 0;
//             blockPaletteWrite.dstArrayElement = 0;
//             blockPaletteWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
//             blockPaletteWrite.descriptorCount = 1;
//             blockPaletteWrite.pImageInfo = &inputBlockPaletteInfo;
//         vector<VkWriteDescriptorSet> descriptorWrites = {blockPaletteWrite};

//         vkUpdateDescriptorSets(device, descriptorWrites.size(), descriptorWrites.data(), 0, NULL);
//     }
// }
void Renderer::setup_Raytrace_Descriptors(){
    for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        int previous_frame = i - 1;
        // int previous_frame = i;
        if (previous_frame < 0) previous_frame = MAX_FRAMES_IN_FLIGHT-1; // so frames do not intersect

        VkDescriptorImageInfo matNormInfo = {};
            // matNormInfo.imageView = is_scaled? matNorm_downscaled[i].view : matNorm[i].view;
            matNormInfo.imageView = matNorm_downscaled[i].view;
            matNormInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        VkDescriptorImageInfo oldUvInfo = {};
            oldUvInfo.imageView = oldUv.view;
            oldUvInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            oldUvInfo.sampler = linearSampler;
        VkDescriptorImageInfo depthInfo = {};
#ifdef STENCIL
            depthInfo.imageView = depthViews[i];
#else 
            depthInfo.imageView = depthBuffer[i].view;
#endif
            depthInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            depthInfo.sampler = linearSampler;
        VkDescriptorImageInfo old_depthInfo = {};
            // old_depthInfo.imageView = is_scaled? depth_downscaled[i].view : depth[i].view;
#ifdef STENCIL
            old_depthInfo.imageView = depthViews[previous_frame];
#else 
            old_depthInfo.imageView = depthBuffer[previous_frame].view;
#endif
            old_depthInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            old_depthInfo.sampler = linearSampler;
        VkDescriptorImageInfo old_matNormInfo = {};
            old_matNormInfo.imageView = matNorm_downscaled[previous_frame].view;
            old_matNormInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            old_matNormInfo.sampler = nearestSampler;
        VkDescriptorImageInfo inputBlockInfo = {};
            inputBlockInfo.imageView = world.view;
            inputBlockInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            // inputBlockInfo.sampler = blockPaletteImageSamplers[i];
        VkDescriptorImageInfo inputBlockPaletteInfo = {};
            inputBlockPaletteInfo.imageView = block_palette.view;
            inputBlockPaletteInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        VkDescriptorImageInfo inputVoxelPaletteInfo = {};
            inputVoxelPaletteInfo.imageView = material_palette[i].view;
            inputVoxelPaletteInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        VkDescriptorImageInfo outputFrameInfo = {};
            outputFrameInfo.imageView = frame[i].view;
            outputFrameInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            // outputFrameInfo.sampler = nearestSampler;
        VkDescriptorImageInfo stepCountInfo = {};
            stepCountInfo.imageView = step_count.view;
            stepCountInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        VkDescriptorImageInfo readFrameInfo = {};
            readFrameInfo.imageView = frame[previous_frame].view;
            // readFrameInfo.imageView = raytraced_frame[previous_frame].view;
            readFrameInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            readFrameInfo.sampler = linearSampler;
            
        VkWriteDescriptorSet matNormWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            matNormWrite.dstSet = raytraceDescriptorSets[i];
            matNormWrite.dstBinding = 0;
            matNormWrite.dstArrayElement = 0;
            matNormWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            matNormWrite.descriptorCount = 1;
            matNormWrite.pImageInfo = &matNormInfo;
        VkWriteDescriptorSet oldUvWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            oldUvWrite.dstSet = raytraceDescriptorSets[i];
            oldUvWrite.dstBinding = 1;
            oldUvWrite.dstArrayElement = 0;
            oldUvWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            oldUvWrite.descriptorCount = 1;
            oldUvWrite.pImageInfo = &oldUvInfo;
        VkWriteDescriptorSet depthWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            depthWrite.dstSet = raytraceDescriptorSets[i];
            depthWrite.dstBinding = 2;
            depthWrite.dstArrayElement = 0;
            // depthWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            depthWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            depthWrite.descriptorCount = 1;
            depthWrite.pImageInfo = &depthInfo;
        VkWriteDescriptorSet old_depthWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            old_depthWrite.dstSet = raytraceDescriptorSets[i];
            old_depthWrite.dstBinding = 3;
            old_depthWrite.dstArrayElement = 0;
            // old_depthWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            old_depthWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            old_depthWrite.descriptorCount = 1;
            old_depthWrite.pImageInfo = &old_depthInfo;

        VkWriteDescriptorSet old_matNormWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            old_matNormWrite.dstSet = raytraceDescriptorSets[i];
            old_matNormWrite.dstBinding = 4;
            old_matNormWrite.dstArrayElement = 0;
            old_matNormWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            old_matNormWrite.descriptorCount = 1;
            old_matNormWrite.pImageInfo = &old_matNormInfo;
        VkWriteDescriptorSet blockWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            blockWrite.dstSet = raytraceDescriptorSets[i];
            blockWrite.dstBinding = 5;
            blockWrite.dstArrayElement = 0;
            blockWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            blockWrite.descriptorCount = 1;
            blockWrite.pImageInfo = &inputBlockInfo;
        VkWriteDescriptorSet blockPaletteWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            blockPaletteWrite.dstSet = raytraceDescriptorSets[i];
            blockPaletteWrite.dstBinding = 6;
            blockPaletteWrite.dstArrayElement = 0;
            blockPaletteWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            blockPaletteWrite.descriptorCount = 1;
            blockPaletteWrite.pImageInfo = &inputBlockPaletteInfo;
        VkWriteDescriptorSet voxelPaletteWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            voxelPaletteWrite.dstSet = raytraceDescriptorSets[i];
            voxelPaletteWrite.dstBinding = 7;
            voxelPaletteWrite.dstArrayElement = 0;
            voxelPaletteWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            voxelPaletteWrite.descriptorCount = 1;
            voxelPaletteWrite.pImageInfo = &inputVoxelPaletteInfo;
        VkWriteDescriptorSet stepCount = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            stepCount.dstSet = raytraceDescriptorSets[i];
            stepCount.dstBinding = 8;
            stepCount.dstArrayElement = 0;
            stepCount.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            stepCount.descriptorCount = 1;
            stepCount.pImageInfo = &stepCountInfo;
        VkWriteDescriptorSet outNewRaytracedWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            outNewRaytracedWrite.dstSet = raytraceDescriptorSets[i];
            outNewRaytracedWrite.dstBinding = 9;
            outNewRaytracedWrite.dstArrayElement = 0;
            outNewRaytracedWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            outNewRaytracedWrite.descriptorCount = 1;
            outNewRaytracedWrite.pImageInfo = &outputFrameInfo;
        //binds to previous frame as intended
        VkWriteDescriptorSet oldDenoisedWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            oldDenoisedWrite.dstSet = raytraceDescriptorSets[i];
            oldDenoisedWrite.dstBinding = 10;
            oldDenoisedWrite.dstArrayElement = 0;
            // outReadSampler.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            oldDenoisedWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            oldDenoisedWrite.descriptorCount = 1;
            oldDenoisedWrite.pImageInfo = &readFrameInfo; // to prevent reading what you write
        vector<VkWriteDescriptorSet> descriptorWrites = {matNormWrite, oldUvWrite, depthWrite, old_depthWrite, old_matNormWrite, blockWrite, blockPaletteWrite, voxelPaletteWrite, stepCount, outNewRaytracedWrite, oldDenoisedWrite/*blockPaletteSampler*/};

        vkUpdateDescriptorSets(device, descriptorWrites.size(), descriptorWrites.data(), 0, NULL);
    }
}
void Renderer::setup_Denoise_Descriptors(){
    for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorImageInfo matNormInfo = {};
            matNormInfo.imageView = matNorm_downscaled[i].view;
            matNormInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        VkDescriptorImageInfo depthInfo = {};
            // depthInfo.imageView = is_scaled? depth_downscaled[i].view : depth[i].view;
#ifdef STENCIL
            depthInfo.imageView = depthViews[i];
#else
            depthInfo.imageView = depthBuffer[i].view;
#endif
            depthInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            depthInfo.sampler = nearestSampler;
            
        VkDescriptorImageInfo readFrameInfo = {};
            readFrameInfo.imageView = frame[i].view;
            readFrameInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            // readFrameInfo.sampler = raytracedImageSamplers[previous_frame];
        VkDescriptorImageInfo outputFrameInfo = {};
            outputFrameInfo.imageView = frame[i].view;
            outputFrameInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            
        VkWriteDescriptorSet matNormWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            matNormWrite.dstSet = denoiseDescriptorSets[i];
            matNormWrite.dstBinding = 0;
            matNormWrite.dstArrayElement = 0;
            matNormWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            matNormWrite.descriptorCount = 1;
            matNormWrite.pImageInfo = &matNormInfo;
        VkWriteDescriptorSet depthWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            depthWrite.dstSet = denoiseDescriptorSets[i];
            depthWrite.dstBinding = 1;
            depthWrite.dstArrayElement = 0;
            // depthWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            depthWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            depthWrite.descriptorCount = 1;
            depthWrite.pImageInfo = &depthInfo;
        VkWriteDescriptorSet raytracedFrameWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
           raytracedFrameWrite.dstSet = denoiseDescriptorSets[i];
           raytracedFrameWrite.dstBinding = 2;
           raytracedFrameWrite.dstArrayElement = 0;
           raytracedFrameWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
           raytracedFrameWrite.descriptorCount = 1;
           raytracedFrameWrite.pImageInfo = &readFrameInfo;
        //binds to previous frame as intended
        VkWriteDescriptorSet denoisedFrameWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            denoisedFrameWrite.dstSet = denoiseDescriptorSets[i];
            denoisedFrameWrite.dstBinding = 3;
            denoisedFrameWrite.dstArrayElement = 0;
            denoisedFrameWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            denoisedFrameWrite.descriptorCount = 1;
            denoisedFrameWrite.pImageInfo = &outputFrameInfo; // to prevent reading what you write
        vector<VkWriteDescriptorSet> descriptorWrites = {matNormWrite, depthWrite, raytracedFrameWrite, denoisedFrameWrite/*blockPaletteSampler*/};

        vkUpdateDescriptorSets(device, descriptorWrites.size(), descriptorWrites.data(), 0, NULL);
    }
}
void Renderer::setup_Upscale_Descriptors(){
    for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorImageInfo lowres_matNormInfo = {};
            lowres_matNormInfo.imageView = matNorm_downscaled[i].view;
            lowres_matNormInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        VkDescriptorImageInfo highres_matNormInfo = {};
            highres_matNormInfo.imageView = matNorm.view; //will be NULL_HANDLE if not scaled, but this function will not be called anyways
            highres_matNormInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        VkDescriptorImageInfo denoisedFrameInfo = {};
            denoisedFrameInfo.imageView = frame[i].view;
            denoisedFrameInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        VkDescriptorImageInfo upscaledFrameInfo = {};
            upscaledFrameInfo.imageView = upscaled_frame.view;
            upscaledFrameInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            
        VkWriteDescriptorSet lowres_matNormWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            lowres_matNormWrite.dstSet = upscaleDescriptorSets[i];
            lowres_matNormWrite.dstBinding = 0;
            lowres_matNormWrite.dstArrayElement = 0;
            lowres_matNormWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            lowres_matNormWrite.descriptorCount = 1;
            lowres_matNormWrite.pImageInfo = &lowres_matNormInfo;
        VkWriteDescriptorSet highres_matNormWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
           highres_matNormWrite.dstSet = upscaleDescriptorSets[i];
           highres_matNormWrite.dstBinding = 1;
           highres_matNormWrite.dstArrayElement = 0;
           highres_matNormWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
           highres_matNormWrite.descriptorCount = 1;
           highres_matNormWrite.pImageInfo = &highres_matNormInfo;
        VkWriteDescriptorSet denoisedFrameWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
           denoisedFrameWrite.dstSet = upscaleDescriptorSets[i];
           denoisedFrameWrite.dstBinding = 2;
           denoisedFrameWrite.dstArrayElement = 0;
           denoisedFrameWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
           denoisedFrameWrite.descriptorCount = 1;
           denoisedFrameWrite.pImageInfo = &denoisedFrameInfo;
        VkWriteDescriptorSet upscaledFrameWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
           upscaledFrameWrite.dstSet = upscaleDescriptorSets[i];
           upscaledFrameWrite.dstBinding = 3;
           upscaledFrameWrite.dstArrayElement = 0;
           upscaledFrameWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
           upscaledFrameWrite.descriptorCount = 1;
           upscaledFrameWrite.pImageInfo = &upscaledFrameInfo;
        vector<VkWriteDescriptorSet> descriptorWrites = {lowres_matNormWrite, highres_matNormWrite, denoisedFrameWrite, upscaledFrameWrite/*blockPaletteSampler*/};

        vkUpdateDescriptorSets(device, descriptorWrites.size(), descriptorWrites.data(), 0, NULL);
    }
}

void Renderer::create_samplers(){
    VkSamplerCreateInfo samplerInfo = {};
        samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
        samplerInfo.magFilter = VK_FILTER_NEAREST;
        samplerInfo.minFilter = VK_FILTER_NEAREST;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
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
    
    // samplerInfo.magFilter = VK_FILTER_LINEAR;
    // samplerInfo.minFilter = VK_FILTER_LINEAR;
    // nearestSamplers.resize(MAX_FRAMES_IN_FLIGHT);
    //  linearSamplers.resize(MAX_FRAMES_IN_FLIGHT);
    // for (auto& sampler : nearestSamplers) {
    // }
    VK_CHECK(vkCreateSampler(device, &samplerInfo, NULL, &nearestSampler));
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    VK_CHECK(vkCreateSampler(device, &samplerInfo, NULL, &linearSampler));
    // samplerInfo.magFilter = VK_FILTER_CUBIC_EXT;
    // samplerInfo.minFilter = VK_FILTER_CUBIC_EXT;
    // for (auto& sampler : linearSamplers) {
    // }
}

void Renderer::setup_Graphical_Descriptors(){
    for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorImageInfo storageImageInfo = {};
            if(is_scaled) {
                storageImageInfo.imageView = upscaled_frame.view;
            } else {
                storageImageInfo.imageView = frame[i].view;
            }
            storageImageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            storageImageInfo.sampler = linearSampler;
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
void Renderer::setup_RayGen_Descriptors(){
    for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorBufferInfo transInfo = {};
            transInfo.offset = 0;
            transInfo.buffer = uniform[i].buffer;
            transInfo.range = VK_WHOLE_SIZE;
        VkWriteDescriptorSet descriptorWrite = {};
            descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            descriptorWrite.dstSet = RayGenDescriptorSets[i];
            descriptorWrite.dstBinding = 0;
            descriptorWrite.dstArrayElement = 0;
            descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            descriptorWrite.descriptorCount = 1;
            descriptorWrite.pBufferInfo = &transInfo;
        vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, NULL);
    }
}

vector<char> read_Shader(const char* filename);

void Renderer::create_compute_pipelines_helper(const char* name, VkDescriptorSetLayout  descriptorSetLayout, VkPipelineLayout* pipelineLayout, VkPipeline* pipeline, u32 push_size){
    auto compShaderCode = read_Shader(name);

    VkShaderModule module = create_Shader_Module(&compShaderCode);
    
    //single stage
    VkPipelineShaderStageCreateInfo compShaderStageInfo = {};
        compShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        compShaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        compShaderStageInfo.module = module;
        compShaderStageInfo.pName = "main";

    VkPushConstantRange pushRange = {};
        // pushRange.size = 256;
        pushRange.size = push_size; //trans
        pushRange.offset = 0;
        pushRange.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

    VkPipelineLayoutCreateInfo pipelineLayoutInfo = {};
        pipelineLayoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO; 
        pipelineLayoutInfo.setLayoutCount = 1; // 1 input (image from swapchain)  for now
        pipelineLayoutInfo.pSetLayouts = &descriptorSetLayout;
        if(push_size != 0) {
            pipelineLayoutInfo.pushConstantRangeCount = 1;
            pipelineLayoutInfo.pPushConstantRanges = &pushRange;
        }
        // pipelineLayoutInfo.pushConstantRangeCount = 0;
        // pipelineLayoutInfo.pPushConstantRanges = NULL;
    VK_CHECK(vkCreatePipelineLayout(device, &pipelineLayoutInfo, NULL, pipelineLayout));

    VkComputePipelineCreateInfo pipelineInfo = {};
        pipelineInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        pipelineInfo.stage = compShaderStageInfo; //always single stage so no pointer :)
        pipelineInfo.layout = *pipelineLayout;

    VK_CHECK(vkCreateComputePipelines(device, NULL, 1, &pipelineInfo, NULL, pipeline));
    vkDestroyShaderModule(device, module, NULL);
}

void Renderer::create_compute_pipelines(){
    // create_compute_pipelines_helper("shaders/compiled/blockify.spv", blockifyDescriptorSetLayout, &blockifyLayout, &blockifyPipeline, sizeof(mat4));
    // create_compute_pipelines_helper("shaders/compiled/copy.spv"    , copyDescriptorSetLayout    , &copyLayout    , &copyPipeline    , 0);
    create_compute_pipelines_helper("shaders/compiled/map.spv"     , mapDescriptorSetLayout     , &mapLayout     , &mapPipeline     , sizeof(mat4) + sizeof(ivec4));
    // create_compute_pipelines_helper("shaders/compiled/dfx.spv"      , dfDescriptorSetLayout      , &dfxLayout      , &dfxPipeline      , 0);
    // create_compute_pipelines_helper("shaders/compiled/dfy.spv"      , dfDescriptorSetLayout      , &dfyLayout      , &dfyPipeline      , 0);
    // create_compute_pipelines_helper("shaders/compiled/dfz.spv"      , dfDescriptorSetLayout      , &dfzLayout      , &dfzPipeline      , 0);
    create_compute_pipelines_helper("shaders/compiled/comp.spv"    , raytraceDescriptorSetLayout, &raytraceLayout, &raytracePipeline, sizeof(ivec4) + sizeof(vec4)*4);
    create_compute_pipelines_helper("shaders/compiled/denoise.spv" ,  denoiseDescriptorSetLayout,  &denoiseLayout,  &denoisePipeline, sizeof(int));
    // create_compute_pipelines_helper("shaders/compiled/upscale.spv"    ,  upscaleDescriptorSetLayout,  &upscaleLayout,  &upscalePipeline, 0);
    if(is_scaled){
        create_compute_pipelines_helper("shaders/compiled/upscale.spv" ,  upscaleDescriptorSetLayout,  &upscaleLayout,  &upscalePipeline, 0);
    }
    // create_Blockify_Pipeline();
    // create_Copy_Pipeline();
    // create_Map_Pipeline();
    // create_Df_Pipeline();
    // create_Raytrace_Pipeline();

}

u32 Renderer::find_Memory_Type(u32 typeFilter, VkMemoryPropertyFlags properties){
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(physicalDevice, &memProperties);    

    for (u32 i = 0; i < memProperties.memoryTypeCount; i++) {
        if ((typeFilter & (1 << i)) && (memProperties.memoryTypes[i].propertyFlags & properties) == properties) {
            return i;
        }
    }
    cout << "memory type not found";
    abort();
}

// void Renderer::create_Buffer(VkDeviceSize size, VkBufferUsageFlags usage, VkMemoryPropertyFlags properties, VkBuffer* buffer, VkDeviceMemory* bufferMemory) {
//     VkBufferCreateInfo bufferInfo{};
//         bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
//         bufferInfo.size = size;
//         bufferInfo.usage = usage;
//         bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

//     VK_CHECK(vkCreateBuffer(device, &bufferInfo, NULL, buffer));

//     VkMemoryRequirements memRequirements;
//     vkGetBufferMemoryRequirements(device, (*buffer), &memRequirements);

//     VkMemoryAllocateInfo allocInfo{};
//         allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
//         allocInfo.allocationSize = memRequirements.size;
//         allocInfo.memoryTypeIndex = find_Memory_Type(memRequirements.memoryTypeBits, properties);

//     VK_CHECK(vkAllocateMemory(device, &allocInfo, NULL, bufferMemory));

//     VK_CHECK(vkBindBufferMemory(device, (*buffer), (*bufferMemory), 0));
// }

#undef copy_Buffer
void Renderer::copy_Buffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
    VkCommandBuffer commandBuffer= begin_Single_Time_Commands();

    VkBufferCopy copyRegion{};
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

    end_Single_Time_Commands(commandBuffer);
}

void Renderer::copy_Buffer(VkBuffer srcBuffer, VkImage dstImage, uvec3 size, VkImageLayout layout) {
    VkCommandBuffer commandBuffer= begin_Single_Time_Commands();

    VkBufferImageCopy copyRegion = {};
    // copyRegion.size = size;
        copyRegion.imageExtent.width  = size.x;
        copyRegion.imageExtent.height = size.y;
        copyRegion.imageExtent.depth  = size.z;
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

    //TODO: change to barrier
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

void Renderer::copy_Whole_Image(VkExtent3D extent, VkCommandBuffer cmdbuf, VkImage src, VkImage dst){
    VkImageCopy copy_op = {};
        copy_op.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copy_op.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copy_op.srcSubresource.layerCount = 1;
        copy_op.dstSubresource.layerCount = 1;
        copy_op.srcSubresource.baseArrayLayer = 0;
        copy_op.dstSubresource.baseArrayLayer = 0;
        copy_op.srcSubresource.mipLevel = 0;
        copy_op.dstSubresource.mipLevel = 0;
        copy_op.srcOffset = {0, 0,0};
        copy_op.dstOffset = {0, 0,0};
        copy_op.extent = extent;
    vkCmdCopyImage(cmdbuf, 
        src,
        VK_IMAGE_LAYOUT_GENERAL, //TODO:
        dst,
        VK_IMAGE_LAYOUT_GENERAL, //TODO:
        1,
        &copy_op
    );
    VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = dst;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT ;
        barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;

    vkCmdPipelineBarrier(
        cmdbuf,
        VK_PIPELINE_STAGE_TRANSFER_BIT, 
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0,
        0, NULL,
        0, NULL,
        1, &barrier
    );
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
    barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
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

//NOTE: all following funcs are not updates - they are just "loads"

//TODO: make use of frames in flight - do not wait for this things
//TODO: do smth with frames in flight
tuple<int, int> get_block_xy(int N){
    int x = N % BLOCK_PALETTE_SIZE_X;
    int y = N / BLOCK_PALETTE_SIZE_X;
    assert(y <= BLOCK_PALETTE_SIZE_Y);
    return tuple(x,y);
}
// void Renderer::load_vertices(Mesh* mesh, Vertex* vertices){
//     crash("not implemented yet");
// }