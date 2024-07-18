#define VMA_IMPLEMENTATION
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#include "render.hpp" 

using namespace std;
using namespace glm;



tuple<int, int> get_block_xy(int N);

void Renderer::init(int x_size, int y_size, int z_size, int _static_block_palette_size, int max_particle_count, float ratio, bool vsync, bool fullscreen){
    world_size = ivec3(x_size, y_size, z_size);
     origin_world.allocate(world_size);
    current_world.allocate(world_size);
    _ratio = ratio;
    // fratio = _rat
    _max_particle_count = max_particle_count;
    is_scaled = (ratio != float(1));
    is_vsync = vsync;
    is_fullscreen = fullscreen;
    static_block_palette_size = _static_block_palette_size;

    UiRenderInterface = new(MyRenderInterface);
    UiRenderInterface->render = this;

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

    create_Swapchain(); //should be before anything related to its size and format
    create_Swapchain_Image_Views();

    create_RenderPass_Graphical();
    create_RenderPass_RayGen();
    create_RenderPass_RayGen_Particles();

    create_samplers();

    printl(VK_FORMAT_UNDEFINED)
    printl(swapChainImageFormat)
    
    create_Command_Pool();
    create_Command_Buffers(&overlayCommandBuffers, MAX_FRAMES_IN_FLIGHT);
    create_Command_Buffers(&   rayGenCommandBuffers, MAX_FRAMES_IN_FLIGHT);
    create_Command_Buffers(&  computeCommandBuffers, MAX_FRAMES_IN_FLIGHT);

    VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_SECONDARY;
        allocInfo.commandBufferCount = MAX_FRAMES_IN_FLIGHT;
    copyOverlayCommandBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    VK_CHECK(vkAllocateCommandBuffers(device, &allocInfo, copyOverlayCommandBuffers.data())); 
    renderOverlayCommandBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    VK_CHECK(vkAllocateCommandBuffers(device, &allocInfo, renderOverlayCommandBuffers.data())); 
    
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
        world_size); //TODO: dynamic
    create_Image_Storages(&radiance_cache,
        VK_IMAGE_TYPE_3D,
        VK_FORMAT_R16G16B16A16_UNORM,
        VK_IMAGE_USAGE_STORAGE_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT,
        {RCACHE_RAYS_PER_PROBE * world_size.x, world_size.y, world_size.z}); //TODO: dynamic
    create_Image_Storages(&origin_block_palette,
        VK_IMAGE_TYPE_3D,
        VK_FORMAT_R8_UINT,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT,
        {16*BLOCK_PALETTE_SIZE_X, 16*BLOCK_PALETTE_SIZE_Y, 16}); //TODO: dynamic
    // create_Image_Storages(&block_palette,
    //     VK_IMAGE_TYPE_3D,
    //     VK_FORMAT_R8_UINT,
    //     VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
    //     VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
    //     VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
    //     VK_IMAGE_ASPECT_COLOR_BIT,
    //     VK_IMAGE_LAYOUT_GENERAL,
    //     VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
    //     VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
    //     {16*BLOCK_PALETTE_SIZE_X, 16*BLOCK_PALETTE_SIZE_Y, 16}); //TODO: dynamic
    create_Image_Storages(&material_palette,
        VK_IMAGE_TYPE_2D,
        VK_FORMAT_R32_SFLOAT, //try R32G32
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT,
        {6, 256, 1}); //TODO: dynamic, text formats, pack Material into smth other than 6 floats :)

    create_Buffer_Storages(&gpu_particles,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        _max_particle_count*sizeof(Particle), true);
    // create_Buffer_Storages(&staging_uniform,
    //     VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
    //     sizeof(mat4)*2, true);
    create_Buffer_Storages(&uniform,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        sizeof(mat4)*2, false);
    // staging_uniform_mapped.resize(MAX_FRAMES_IN_FLIGHT);
    staging_world_mapped.resize(MAX_FRAMES_IN_FLIGHT);
    gpu_particles_mapped.resize(MAX_FRAMES_IN_FLIGHT);
    for (int i=0; i<MAX_FRAMES_IN_FLIGHT; i++){
        vmaMapMemory(VMAllocator, gpu_particles[i].alloc, &gpu_particles_mapped[i]);
        vmaMapMemory(VMAllocator, staging_world[i].alloc, &staging_world_mapped[i]);
    }
    transition_Image_Layout_Singletime(&world, VK_IMAGE_LAYOUT_GENERAL);
    transition_Image_Layout_Singletime(&radiance_cache, VK_IMAGE_LAYOUT_GENERAL);
    for (int i=0; i<MAX_FRAMES_IN_FLIGHT; i++){
        transition_Image_Layout_Singletime(&origin_block_palette[i], VK_IMAGE_LAYOUT_GENERAL);
        transition_Image_Layout_Singletime(&material_palette[i], VK_IMAGE_LAYOUT_GENERAL);
    }
    create_Descriptor_Set_Layouts();
    create_Descriptor_Pool();
    allocate_Descriptors();

    setup_Raytrace_Descriptors();
println
    setup_Radiance_Cache_Descriptors();
println
    setup_Do_Light_Descriptors();
println

    setup_Denoise_Descriptors();
    if(is_scaled) {
        setup_Upscale_Descriptors();
    }
    setup_Accumulate_Descriptors();
    // setup_Graphical_Descriptors();
    setup_RayGen_Descriptors();
    setup_RayGen_Particles_Descriptors();

    create_RayGen_Pipeline();
    create_RayGen_Particles_Pipeline();
    create_Graphics_Pipeline();

    create_compute_pipelines();
    create_Sync_Objects();
println
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
    delete UiRenderInterface;

	
    // delete_Images(&lowres_sunlight);
    delete_Images(&world);
    delete_Images(&radiance_cache);
    delete_Images(&origin_block_palette);
    // delete_Images(&       block_palette);
    delete_Images(&material_palette);

    vkDestroySampler(device, nearestSampler, NULL);
    vkDestroySampler(device,  linearSampler, NULL);
    vkDestroySampler(device,      uiSampler, NULL);

    for (int i=0; i<MAX_FRAMES_IN_FLIGHT; i++){

        
        // vmaDestroyBuffer(VMAllocator, depthStaginBuffers[i], paletteCounterBufferAllocations[i]);
        // vmaDestroyBuffer(VMAllocator, copyCounterBuffers[i], copyCounterBufferAllocations[i]);

            // vmaUnmapMemory(VMAllocator,                          staging_uniform[i].alloc);
        // vmaDestroyBuffer(VMAllocator, staging_uniform[i].buffer, staging_uniform[i].alloc);
            vmaUnmapMemory(VMAllocator,                           staging_world[i].alloc);
            vmaUnmapMemory(VMAllocator,                           gpu_particles[i].alloc);
        vmaDestroyBuffer(VMAllocator,  staging_world[i].buffer, staging_world[i].alloc);
        vmaDestroyBuffer(VMAllocator,  gpu_particles[i].buffer, gpu_particles[i].alloc);
        vmaDestroyBuffer(VMAllocator,  uniform[i].buffer, uniform[i].alloc);
    }

    vkDestroyDescriptorPool(device, descriptorPool, NULL);
    vkDestroyDescriptorSetLayout(device,     RayGenDescriptorSetLayout, NULL);
    vkDestroyDescriptorSetLayout(device,     RayGenParticlesDescriptorSetLayout, NULL);
    // vkDestroyDescriptorSetLayout(device,   blockifyDescriptorSetLayout, NULL);
    // vkDestroyDescriptorSetLayout(device,       copyDescriptorSetLayout, NULL);
    vkDestroyDescriptorSetLayout(device,        mapDescriptorSetLayout, NULL);
    // vkDestroyDescriptorSetLayout(device,         dfDescriptorSetLayout, NULL);
    vkDestroyDescriptorSetLayout(device,   raytraceDescriptorSetLayout, NULL);
    vkDestroyDescriptorSetLayout(device,   updateRadianceDescriptorSetLayout, NULL);
    vkDestroyDescriptorSetLayout(device,   doLightDescriptorSetLayout, NULL);
    vkDestroyDescriptorSetLayout(device,    denoiseDescriptorSetLayout, NULL);
    vkDestroyDescriptorSetLayout(device,    upscaleDescriptorSetLayout, NULL);
    vkDestroyDescriptorSetLayout(device,  graphicalDescriptorSetLayout, NULL);
    vkDestroyDescriptorSetLayout(device,  accumulateDescriptorSetLayout, NULL);

    for (int i=0; i < MAX_FRAMES_IN_FLIGHT; i++){
        vkDestroySemaphore(device,  imageAvailableSemaphores[i], NULL);
        vkDestroySemaphore(device,  renderFinishedSemaphores[i], NULL);
        vkDestroySemaphore(device, raytraceFinishedSemaphores[i], NULL);
        vkDestroySemaphore(device,  rayGenFinishedSemaphores[i], NULL);
        vkDestroyFence(device, graphicalInFlightFences[i], NULL);
        vkDestroyFence(device,   raytraceInFlightFences[i], NULL);
        vkDestroyFence(device,    frameInFlightFences[i], NULL);
    }
    vkDestroyCommandPool(device, commandPool, NULL);
    vkDestroyRenderPass(device, graphicalRenderPass, NULL);
    vkDestroyRenderPass(device, rayGenRenderPass, NULL);
    vkDestroyRenderPass(device, rayGenParticlesRenderPass, NULL);

    vkDestroyPipeline(device,   rayGenPipeline, NULL);
    vkDestroyPipeline(device,   rayGenParticlesPipeline, NULL);
    // vkDestroyPipeline(device, blockifyPipeline, NULL);
    // vkDestroyPipeline(device,     copyPipeline, NULL);
    vkDestroyPipeline(device,      mapPipeline, NULL);
    // vkDestroyPipeline(device,       dfxPipeline, NULL);
    // vkDestroyPipeline(device,       dfyPipeline, NULL);
    // vkDestroyPipeline(device,       dfzPipeline, NULL);
    vkDestroyPipeline(device,   raytracePipeline, NULL);
    vkDestroyPipeline(device,   radiancePipeline, NULL);
    vkDestroyPipeline(device,   doLightPipeline, NULL);

    vkDestroyPipeline(device,    denoisePipeline, NULL);
    vkDestroyPipeline(device,    upscalePipeline, NULL);
    vkDestroyPipeline(device, accumulatePipeline, NULL);
    vkDestroyPipeline(device,   graphicsPipeline, NULL);

    vkDestroyPipelineLayout(device,   rayGenPipelineLayout, NULL);
    vkDestroyPipelineLayout(device,   rayGenParticlesPipelineLayout, NULL);
    // vkDestroyPipelineLayout(device, blockifyLayout, NULL);
    // vkDestroyPipelineLayout(device,     copyLayout, NULL);
    vkDestroyPipelineLayout(device,      mapLayout, NULL);
    // vkDestroyPipelineLayout(device,       dfxLayout, NULL);
    // vkDestroyPipelineLayout(device,       dfyLayout, NULL);
    // vkDestroyPipelineLayout(device,       dfzLayout, NULL);
    vkDestroyPipelineLayout(device,   raytraceLayout, NULL);
    vkDestroyPipelineLayout(device,   radianceLayout, NULL);
    vkDestroyPipelineLayout(device,   doLightLayout, NULL);

    vkDestroyPipelineLayout(device,    denoiseLayout, NULL);
    vkDestroyPipelineLayout(device,    upscaleLayout, NULL);
    vkDestroyPipelineLayout(device, accumulateLayout, NULL);
    vkDestroyPipelineLayout(device,   graphicsLayout, NULL);

    vkDestroyShaderModule(device, rayGenVertShaderModule, NULL);
    vkDestroyShaderModule(device, rayGenFragShaderModule, NULL); 
    vkDestroyShaderModule(device, rayGenParticlesVertShaderModule, NULL);
    vkDestroyShaderModule(device, rayGenParticlesGeomShaderModule, NULL);
    vkDestroyShaderModule(device, rayGenParticlesFragShaderModule, NULL);
    vkDestroyShaderModule(device, graphicsVertShaderModule, NULL);
    vkDestroyShaderModule(device, graphicsFragShaderModule, NULL); 

    cleanup_SwapchainDependent();

    for(int i=0; i<MAX_FRAMES_IN_FLIGHT+1; i++){
        process_ui_deletion_queue();

        // char* dump;
        // vmaBuildStatsString(VMAllocator, &dump, 1);
        // cout << dump << std::fflush(stdout);
        // fflush(stdout);
        // vmaFreeStatsString(VMAllocator, dump);
    }
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
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;

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
    
    raytraceExtent.width  = swapChainExtent.width  / _ratio;
    raytraceExtent.height = swapChainExtent.height / _ratio;
}

void Renderer::cleanup_SwapchainDependent(){
    if(is_scaled){
        delete_Images(&depthBuffers_lowres);
        delete_Images(&lowres_frames);
        delete_Images(&matNorm_lowres);
        delete_Images(&oldUv_lowres);
        
        delete_Images(&depthBuffers_highres[0]);
        delete_Images(&highres_frames[0]);    
        delete_Images(&matNorm_highres[0]);
        delete_Images(&oldUv_highres);
    } else {
        delete_Images(&depthBuffers_highres);
        delete_Images(&highres_frames);    
        delete_Images(&matNorm_highres);
        delete_Images(&oldUv_highres);
    }

    
    delete_Images(&step_count);
    delete_Images(&mix_ratio);
    // delete_Images(&raytraced_frame);
    // delete_Images(&denoised_frame);
    
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
    if(is_scaled){
            create_Image_Storages(&oldUv_lowres,
                VK_IMAGE_TYPE_2D,
                OLD_UV_FORMAT,
                VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
                VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
                VK_IMAGE_ASPECT_COLOR_BIT,
                {raytraceExtent.width, raytraceExtent.height, 1});
            create_Image_Storages(&lowres_frames,
                VK_IMAGE_TYPE_2D,
                RAYTRACED_IMAGE_FORMAT,
                VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
                VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
                VK_IMAGE_ASPECT_COLOR_BIT,
                {raytraceExtent.width, raytraceExtent.height, 1});
            create_Image_Storages(&matNorm_lowres,
                VK_IMAGE_TYPE_2D,
                VK_FORMAT_R8G8B8A8_SNORM,
                VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
                VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
                VK_IMAGE_ASPECT_COLOR_BIT,
                {raytraceExtent.width, raytraceExtent.height, 1});
            create_Image_Storages(&depthBuffers_lowres,
                VK_IMAGE_TYPE_2D,
                DEPTH_FORMAT,
                VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
                VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
                VK_IMAGE_ASPECT_DEPTH_BIT,
                // {swapChainExtent.height, swapChainExtent.width, 1});
                {raytraceExtent.width, raytraceExtent.height, 1});
                
            create_Image_Storages(&oldUv_highres,
                VK_IMAGE_TYPE_2D,
                OLD_UV_FORMAT,
                VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
                VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
                VK_IMAGE_ASPECT_COLOR_BIT,
                {swapChainExtent.width, swapChainExtent.height, 1});
            //cause we only need one each and than copy to lowres
            highres_frames.resize(1);
            create_Image_Storages(&highres_frames[0],
                VK_IMAGE_TYPE_2D,
                // VK_FORMAT_R8G8B8A8_UNORM, //cause errors do not accumulate and we can save on memory bandwith
                RAYTRACED_IMAGE_FORMAT,
                VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
                VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
                VK_IMAGE_ASPECT_COLOR_BIT,
                {swapChainExtent.width, swapChainExtent.height, 1});
            matNorm_highres.resize(1);
            create_Image_Storages(&matNorm_highres[0],
                VK_IMAGE_TYPE_2D,
                VK_FORMAT_R8G8B8A8_SNORM,
                VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
                VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
                VK_IMAGE_ASPECT_COLOR_BIT,
                {swapChainExtent.width, swapChainExtent.height, 1});
            depthBuffers_highres.resize(1);
            create_Image_Storages(&depthBuffers_highres[0],
                VK_IMAGE_TYPE_2D,
                DEPTH_FORMAT,
                VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
                VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
                VK_IMAGE_ASPECT_DEPTH_BIT,
                // {swapChainExtent.height, swapChainExtent.width, 1});
                {swapChainExtent.width, swapChainExtent.height, 1});
    } else {
        create_Image_Storages(&oldUv_highres,
            VK_IMAGE_TYPE_2D,
            OLD_UV_FORMAT,
            VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
            VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
            VK_IMAGE_ASPECT_COLOR_BIT,
            {swapChainExtent.width, swapChainExtent.height, 1});
        create_Image_Storages(&matNorm_highres,
            VK_IMAGE_TYPE_2D,
            VK_FORMAT_R8G8B8A8_SNORM,
            VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
            VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
            VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
            VK_IMAGE_ASPECT_COLOR_BIT,
            {swapChainExtent.width, swapChainExtent.height, 1});
        create_Image_Storages(&depthBuffers_highres,
            VK_IMAGE_TYPE_2D,
            DEPTH_FORMAT,
            VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
            VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
            VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
            VK_IMAGE_ASPECT_DEPTH_BIT,
            {swapChainExtent.width, swapChainExtent.height, 1});
        create_Image_Storages(&highres_frames,
            VK_IMAGE_TYPE_2D,
            RAYTRACED_IMAGE_FORMAT,
            // VK_FORMAT_R8G8B8A8_UNORM,
            VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
            VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
            VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
            VK_IMAGE_ASPECT_COLOR_BIT,
            {swapChainExtent.width, swapChainExtent.height, 1});
    }

    create_Image_Storages(&step_count,
        VK_IMAGE_TYPE_2D,
        VK_FORMAT_R8G8B8A8_UINT,
        VK_IMAGE_USAGE_STORAGE_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT,
        {(raytraceExtent.width+7) / 8, (raytraceExtent.height+7) / 8, 1}); // local_size / warp_size of raytracer
        // {raytraceExtent.width, raytraceExtent.height, 1}); // local_size / warp_size of raytracer
        transition_Image_Layout_Singletime(&step_count, VK_IMAGE_LAYOUT_GENERAL);

    create_Image_Storages(&mix_ratio,
        VK_IMAGE_TYPE_2D,
        VK_FORMAT_R16_UNORM,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT,
        {raytraceExtent.width, raytraceExtent.height, 1});

    for(int i=0; i<MAX_FRAMES_IN_FLIGHT; i++) {
        transition_Image_Layout_Singletime(&mix_ratio[i], VK_IMAGE_LAYOUT_GENERAL);
    }
    transition_Image_Layout_Singletime(&oldUv_lowres,  VK_IMAGE_LAYOUT_GENERAL);
    transition_Image_Layout_Singletime(&oldUv_highres, VK_IMAGE_LAYOUT_GENERAL);

    vector<vector<VkImageView>> swapViews(1);
    for(int i=0; i<swapchain_images.size(); i++) {
        swapViews[0].push_back(swapchain_images[i].view);
    }
    create_N_Framebuffers(&swapChainFramebuffers, &swapViews, graphicalRenderPass, swapchain_images.size(), swapChainExtent.width, swapChainExtent.height);

    vector<vector<VkImageView>> rayGenVeiws(3);
    for(int i=0; i<MAX_FRAMES_IN_FLIGHT; i++) {
        if(is_scaled){
            rayGenVeiws[0].push_back(matNorm_highres[0].view); //only using first
            rayGenVeiws[1].push_back(oldUv_highres.view);
            rayGenVeiws[2].push_back(depthBuffers_highres[0].view); //only using first
        } else {
            rayGenVeiws[0].push_back(matNorm_highres[i].view);
            rayGenVeiws[1].push_back(oldUv_highres.view);
            rayGenVeiws[2].push_back(depthBuffers_highres[i].view);
        }
    }
    create_N_Framebuffers(&rayGenFramebuffers, &rayGenVeiws, rayGenRenderPass, MAX_FRAMES_IN_FLIGHT, swapChainExtent.width, swapChainExtent.height);

    // vector<vector<VkImageView>> rayGenVeiws_downscaled(3);
    // for(int i=0; i<MAX_FRAMES_IN_FLIGHT; i++) {
    //     rayGenVeiws_downscaled[0].push_back(matNorm_downscaled[i].view);
    //     rayGenVeiws_downscaled[1].push_back(oldUv_downscaled.view);   
    //     rayGenVeiws_downscaled[2].push_back(depthBuffer_downscaled[i].view);
    // }
    // create_N_Framebuffers(&rayGenFramebuffers_downscaled, &rayGenVeiws_downscaled, rayGenRenderPass, MAX_FRAMES_IN_FLIGHT, raytraceExtent.width, raytraceExtent.height);
}
void Renderer::recreate_SwapchainDependent(){
    int width = 0, height = 0;
    glfwGetFramebufferSize(window.pointer, &width, &height);
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(window.pointer, &width, &height);
        glfwWaitEvents();
    }
    
    vkDeviceWaitIdle(device);
    cleanup_SwapchainDependent();
    vkDeviceWaitIdle(device);

    create_Swapchain();
    create_Swapchain_Image_Views();

    vkDestroyCommandPool(device, commandPool, NULL);

    create_Command_Pool();
    create_Command_Buffers(&overlayCommandBuffers, MAX_FRAMES_IN_FLIGHT);
    create_Command_Buffers(&   rayGenCommandBuffers, MAX_FRAMES_IN_FLIGHT);
    create_Command_Buffers(&  computeCommandBuffers, MAX_FRAMES_IN_FLIGHT);

    VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_SECONDARY;
        allocInfo.commandBufferCount = MAX_FRAMES_IN_FLIGHT;
    copyOverlayCommandBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    VK_CHECK(vkAllocateCommandBuffers(device, &allocInfo, copyOverlayCommandBuffers.data())); 
    renderOverlayCommandBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    VK_CHECK(vkAllocateCommandBuffers(device, &allocInfo, renderOverlayCommandBuffers.data())); 

    create_SwapchainDependent();

    setup_Raytrace_Descriptors();
    setup_Denoise_Descriptors();
    setup_Radiance_Cache_Descriptors();
    setup_Do_Light_Descriptors();
    if(is_scaled) {
        setup_Upscale_Descriptors();
    }
    setup_Accumulate_Descriptors();
    setup_RayGen_Descriptors();
    setup_RayGen_Particles_Descriptors();

    
    vkDeviceWaitIdle(device);
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
        swapchain_images[i].aspect = VK_IMAGE_ASPECT_COLOR_BIT;
        
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
            // framebufferInfo.

        VK_CHECK(vkCreateFramebuffer(device, &framebufferInfo, NULL, &(*framebuffers)[i]));
    }
}

void Renderer::updateParticles(){
    int write_index = 0;
    for (int i=0; i<particles.size(); i++){
        bool should_keep = particles[i].lifeTime > 0;
        
        if(should_keep){
            particles[write_index] = particles[i];
            // delta_time = 1/75.0;
            particles[write_index].pos += particles[write_index].vel * float(delta_time);
            particles[write_index].lifeTime -= delta_time;
            
            write_index++;
        }        
    }
    particles.resize(write_index);

    int capped_particle_count = glm::clamp(int(particles.size()), 0, _max_particle_count);
    
    memcpy(gpu_particles_mapped[currentFrame], particles.data(), sizeof(Particle)*capped_particle_count);
    vmaFlushAllocation(VMAllocator, gpu_particles[currentFrame].alloc, 0, sizeof(Particle)*capped_particle_count);
}

void Renderer::process_ui_deletion_queue(){
    int write_index = 0;
    for (int i=0; i<ui_mesh_deletion_queue.size(); i++){
        bool should_keep = ui_mesh_deletion_queue[i].life_counter > 0;
        if(should_keep){
            ui_mesh_deletion_queue[write_index] = ui_mesh_deletion_queue[i];
            ui_mesh_deletion_queue[write_index].life_counter -= 1;
            write_index++;
        }
        else {
            //free buffer

            vmaDestroyBuffer(VMAllocator, ui_mesh_deletion_queue[i].mesh.vertexes.buffer, ui_mesh_deletion_queue[i].mesh.vertexes.alloc);
            vmaDestroyBuffer(VMAllocator, ui_mesh_deletion_queue[i].mesh.indexes.buffer, ui_mesh_deletion_queue[i].mesh.indexes.alloc);
        }
    }
    ui_mesh_deletion_queue.resize(write_index);

    write_index = 0;
    for (int i=0; i<ui_image_deletion_queue.size(); i++){
        bool should_keep = ui_image_deletion_queue[i].life_counter > 0;
        if(should_keep){
            ui_image_deletion_queue[write_index] = ui_image_deletion_queue[i];
            ui_image_deletion_queue[write_index].life_counter -= 1;
            write_index++;
        }
        else {
            //free image
            vmaDestroyImage(VMAllocator, ui_image_deletion_queue[i].image.image, ui_image_deletion_queue[i].image.alloc);
            vkDestroyImageView(device, ui_image_deletion_queue[i].image.view, NULL);
        }
    }
    ui_image_deletion_queue.resize(write_index);

    write_index = 0;
    for (int i=0; i<ui_buffer_deletion_queue.size(); i++){
        bool should_keep = ui_buffer_deletion_queue[i].life_counter > 0;
        if(should_keep){
            ui_buffer_deletion_queue[write_index] = ui_buffer_deletion_queue[i];
            ui_buffer_deletion_queue[write_index].life_counter -= 1;
            write_index++;
        }
        else {
            //free image
            vmaDestroyBuffer(VMAllocator, ui_buffer_deletion_queue[i].buffer.buffer, ui_buffer_deletion_queue[i].buffer.alloc);
        }
    }
    ui_buffer_deletion_queue.resize(write_index);
}

// #include <glm/gtx/string_cast.hpp>
void Renderer::start_Frame(){
    dvec3 up = glm::dvec3(0.0f, 0.0f, 1.0f); // Up vector

    dmat4 view = glm::lookAt(camera_pos, camera_pos+camera_dir, up);
    const double voxel_in_pixels = 5.0; //we want voxel to be around 10 pixels in width / height
    const double  view_width_in_voxels = 1920.0 / voxel_in_pixels; //todo make dynamic and use spec constnats
    const double view_height_in_voxels = 1080.0 / voxel_in_pixels;
    dmat4 projection = glm::ortho(-view_width_in_voxels/2.0, view_width_in_voxels/2.0, view_height_in_voxels/2.0, -view_height_in_voxels/2.0, -0.0, +2000.0); // => *100.0 decoding
    dmat4     worldToScreen = projection * view;

        old_trans = current_trans;
    current_trans = worldToScreen;
// println
    vkWaitForFences(device, 1, &frameInFlightFences[currentFrame], VK_TRUE, UINT32_MAX);
    vkResetFences  (device, 1, &frameInFlightFences[currentFrame]);
}

void Renderer::startRaygen(){
    VkCommandBuffer &commandBuffer = rayGenCommandBuffers[currentFrame];
    vkResetCommandBuffer(commandBuffer, 0);
    VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = 0;
        beginInfo.pInheritanceInfo = NULL;
    VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));
    
    struct unicopy {mat4 trans; mat4 old_trans;} unicopy = {current_trans, old_trans};
    vkCmdUpdateBuffer(commandBuffer, uniform[currentFrame].buffer, 0, sizeof(unicopy), &unicopy);

    
    cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
    VkBufferMemoryBarrier staging_uniform_barrier{};
        staging_uniform_barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        staging_uniform_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        staging_uniform_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        staging_uniform_barrier.buffer = uniform[currentFrame].buffer;
        staging_uniform_barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT| VK_ACCESS_MEMORY_READ_BIT;
        staging_uniform_barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
        staging_uniform_barrier.size = VK_WHOLE_SIZE;

    cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, &staging_uniform_barrier);
    cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

    vector<VkClearValue> clearColors = {
        {}, 
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
    
    //this is not intended to be uuncommented. Treat it as shadow of what renderpass does
    // cmdTransLayoutBarrier(commandBuffer, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 
    //     VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT,
    //     is_scaled? &matNorm_highres[0] : &matNorm_highres[currentFrame]);
    // cmdTransLayoutBarrier(commandBuffer, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, 
    //     VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT,
    //     &oldUv_highres);
    // cmdTransLayoutBarrier(commandBuffer, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, 
    //     VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT,
    //     is_scaled? &depthBuffers_highres[0] : &depthBuffers_highres[currentFrame]);

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

static VkBuffer old_buff = NULL;
void Renderer::RaygenMesh(Mesh *mesh){
    VkCommandBuffer &commandBuffer = rayGenCommandBuffers[currentFrame];

        VkBuffer vertexBuffers[] = {(*mesh).vertexes[currentFrame].buffer};
        VkDeviceSize offsets[] = {0};
    
    struct {quat r1,r2; vec4 s1,s2;} raygen_pushconstant = {(*mesh).rot, (*mesh).old_rot, vec4((*mesh).shift,0), vec4((*mesh).old_shift,0)};
    //TODO:
    vkCmdPushConstants(commandBuffer, rayGenPipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(raygen_pushconstant), &raygen_pushconstant);

    (*mesh).old_rot   = (*mesh).rot;
    (*mesh).old_shift = (*mesh).shift;

    // glm::mult
    if(old_buff != (*mesh).indexes[currentFrame].buffer){
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
        vkCmdBindIndexBuffer(commandBuffer, (*mesh).indexes[currentFrame].buffer, 0, VK_INDEX_TYPE_UINT32);
        old_buff = (*mesh).indexes[currentFrame].buffer;
    }
    vkCmdDrawIndexed(commandBuffer, (*mesh).icount, 1, 0, 0, 0);
}
void Renderer::rayGenMapParticles(){
    VkCommandBuffer &commandBuffer = rayGenCommandBuffers[currentFrame];

    vkCmdEndRenderPass(commandBuffer);

    if(is_scaled){
        matNorm_highres[0].layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        oldUv_highres.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
        depthBuffers_highres[0].layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

        cmdTransLayoutBarrier(commandBuffer,      matNorm_highres[0].layout, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
            &matNorm_highres[0]);
        cmdTransLayoutBarrier(commandBuffer,           oldUv_highres.layout, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
            &oldUv_highres);
        cmdTransLayoutBarrier(commandBuffer, depthBuffers_highres[0].layout, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
            &depthBuffers_highres[0]);
    }else{
        matNorm_highres[currentFrame].layout = VK_IMAGE_LAYOUT_GENERAL;
        oldUv_highres.layout = VK_IMAGE_LAYOUT_GENERAL;
        depthBuffers_highres[currentFrame].layout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
    
        cmdTransLayoutBarrier(commandBuffer, matNorm_highres[currentFrame].layout, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
        &matNorm_highres[currentFrame]);
        cmdTransLayoutBarrier(commandBuffer, oldUv_highres.layout, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
        &oldUv_highres);
        cmdTransLayoutBarrier(commandBuffer, depthBuffers_highres[currentFrame].layout, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
        &depthBuffers_highres[currentFrame]);
    }
    VkRenderPassBeginInfo renderPassInfo = {};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = rayGenParticlesRenderPass;
        renderPassInfo.framebuffer = rayGenFramebuffers[currentFrame];
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = swapChainExtent;
        renderPassInfo.clearValueCount = 0;
        renderPassInfo.pClearValues    = NULL;
    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    if(particles.size() > 0){ //driver probably handles this automatically, but just for safity
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, rayGenParticlesPipeline);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, rayGenParticlesPipelineLayout, 0, 1, &RayGenParticlesDescriptorSets[currentFrame], 0, 0);

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

        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, &gpu_particles[currentFrame].buffer, offsets);
        // assert(particles.size() != 0);
        vkCmdDraw(commandBuffer, particles.size(), 1, 0, 0);
    }
}

void Renderer::endRaygen(){
    VkCommandBuffer &commandBuffer = rayGenCommandBuffers[currentFrame];

    vkCmdEndRenderPass(commandBuffer);
    //layout transition renderpass made:
    if(is_scaled){
             matNorm_highres[0].layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
                  oldUv_highres.layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;
        depthBuffers_highres[0].layout = VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL;

        cmdTransLayoutBarrier(commandBuffer, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT|VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT|VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT,
            &matNorm_highres[0]);
        cmdTransLayoutBarrier(commandBuffer, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT|VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT|VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT,
            &oldUv_highres);
        cmdTransLayoutBarrier(commandBuffer, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT|VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT|VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT,
            &depthBuffers_highres[0]);
    }else{
        matNorm_highres[currentFrame].layout = VK_IMAGE_LAYOUT_GENERAL;
        oldUv_highres.layout = VK_IMAGE_LAYOUT_GENERAL;
        depthBuffers_highres[currentFrame].layout = VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL;
    
        cmdTransLayoutBarrier(commandBuffer, matNorm_highres[currentFrame].layout, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
        &matNorm_highres[currentFrame]);
        cmdTransLayoutBarrier(commandBuffer, oldUv_highres.layout, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
        &oldUv_highres);
        cmdTransLayoutBarrier(commandBuffer, depthBuffers_highres[currentFrame].layout, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
        &depthBuffers_highres[currentFrame]);
    }

    if (is_scaled) {
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
            blit.dstOffsets[1].x = raytraceExtent.width;
            blit.dstOffsets[1].y = raytraceExtent.height;
            blit.dstOffsets[1].z = 1;
        cmdTransLayoutBarrier(commandBuffer, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT, 
            &matNorm_lowres[currentFrame]);
        cmdTransLayoutBarrier(commandBuffer, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT, 
            &oldUv_lowres);
        cmdTransLayoutBarrier(commandBuffer, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT, 
            &depthBuffers_lowres[currentFrame]);
            
        vkCmdBlitImage(commandBuffer,      matNorm_highres[0].image, VK_IMAGE_LAYOUT_GENERAL,      matNorm_lowres[currentFrame].image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_NEAREST);
        vkCmdBlitImage(commandBuffer,           oldUv_highres.image, VK_IMAGE_LAYOUT_GENERAL,                      oldUv_lowres.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_NEAREST);
            blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
            blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
        vkCmdBlitImage(commandBuffer, depthBuffers_highres[0].image, VK_IMAGE_LAYOUT_GENERAL, depthBuffers_lowres[currentFrame].image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_NEAREST);
        
        //just barriers, no transition
        cmdTransLayoutBarrier(commandBuffer,      matNorm_lowres[currentFrame].layout, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT, &matNorm_lowres[currentFrame]);
        cmdTransLayoutBarrier(commandBuffer,                      oldUv_lowres.layout, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT, &oldUv_lowres);
        cmdTransLayoutBarrier(commandBuffer, depthBuffers_lowres[currentFrame].layout, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT, &depthBuffers_lowres[currentFrame]);
    } 

    // cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

    VK_CHECK(vkEndCommandBuffer(commandBuffer));
    // VkSubmitInfo submitInfo = {};
    //     submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    //     submitInfo.commandBufferCount = 1;
    //     submitInfo.pCommandBuffers = &rayGenCommandBuffers[currentFrame];
    //     submitInfo.signalSemaphoreCount = 1;
    //     submitInfo.pSignalSemaphores = &rayGenFinishedSemaphores[currentFrame];
    // VK_CHECK(vkQueueSubmit(graphicsQueue, 1, &submitInfo, frameInFlightFences[currentFrame]));
}
void Renderer::startCompute(){
    VkCommandBuffer &commandBuffer = computeCommandBuffers[currentFrame];
    // vkWaitForFences(device, 1, &raytraceInFlightFences[currentFrame], VK_TRUE, UINT32_MAX);
    // vkResetFences  (device, 1, &raytraceInFlightFences[currentFrame]);


    
    vkResetCommandBuffer(computeCommandBuffers[currentFrame], /*VkCommandBufferResetFlagBits*/ 0);
    VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = 0;
        beginInfo.pInheritanceInfo = NULL;
    VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));

    if(is_scaled){
        cmdTransLayoutBarrier(commandBuffer,                         VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT, 
            &matNorm_lowres[currentFrame]);
        cmdTransLayoutBarrier(commandBuffer,                         VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT, 
            &matNorm_lowres[previousFrame]);
        cmdTransLayoutBarrier(commandBuffer,                         VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT, 
            &oldUv_lowres);
        cmdTransLayoutBarrier(commandBuffer, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT, 
            &depthBuffers_lowres[currentFrame]);
        cmdTransLayoutBarrier(commandBuffer, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT, 
            &depthBuffers_lowres[previousFrame]);
        cmdTransLayoutBarrier(commandBuffer,        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT, 
            &mix_ratio[currentFrame]);
        cmdTransLayoutBarrier(commandBuffer,        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT, 
            &mix_ratio[previousFrame]);
        cmdTransLayoutBarrier(commandBuffer,                         VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT, 
            &lowres_frames[currentFrame]);
        cmdTransLayoutBarrier(commandBuffer,                         VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT, 
            &lowres_frames[previousFrame]);

        cmdTransLayoutBarrier(commandBuffer,                         VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT, 
            &matNorm_highres[0]);
        cmdTransLayoutBarrier(commandBuffer,                         VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT, 
            &oldUv_highres);
        cmdTransLayoutBarrier(commandBuffer, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT, 
            &depthBuffers_highres[0]);
        cmdTransLayoutBarrier(commandBuffer,                         VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT, 
            &highres_frames[0]);
        cmdTransLayoutBarrier(commandBuffer,                         VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT, 
            &step_count);
    } else{
        cmdTransLayoutBarrier(commandBuffer,                         VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT, 
            &matNorm_highres[currentFrame]);
        cmdTransLayoutBarrier(commandBuffer,                         VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT, 
            &oldUv_highres);
        cmdTransLayoutBarrier(commandBuffer, VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT, 
            &depthBuffers_highres[currentFrame]);
        cmdTransLayoutBarrier(commandBuffer,        VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT, 
            &mix_ratio[currentFrame]);
        cmdTransLayoutBarrier(commandBuffer,                         VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT, 
            &highres_frames[currentFrame]);
        cmdTransLayoutBarrier(commandBuffer,                         VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT, 
            &step_count);
    }

    cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
}
void Renderer::startBlockify(){
    VkCommandBuffer &commandBuffer = computeCommandBuffers[currentFrame];

    copy_queue.clear();

    palette_counter = static_block_palette_size; //TODO: change to static block count. Currently its size of 1 cause of air
    size_t size_to_copy = world_size.x * world_size.y * world_size.z * sizeof(BlockID_t);
    memcpy(current_world.data(), origin_world.data(), size_to_copy);
}
struct AABB {
  vec3 min;
  vec3 max;
};
static AABB get_shift(dmat4 trans, ivec3 size){
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
void Renderer::blockifyMesh(Mesh* mesh){
    mat4 rotate = toMat4((*mesh).rot);
    mat4 shift = translate(identity<mat4>(), (*mesh).shift);

    struct AABB border_in_voxel = get_shift(shift*rotate, (*mesh).size);
    
    struct {ivec3 min; ivec3 max;} border; 
    border.min = (ivec3(border_in_voxel.min)-ivec3(1)) / ivec3(16);
    border.max = (ivec3(border_in_voxel.max)+ivec3(1)) / ivec3(16);

    border.min = glm::clamp(border.min, ivec3(0), ivec3(world_size-1));
    border.max = glm::clamp(border.max, ivec3(0), ivec3(world_size-1));
    
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
        // if (current_block == 0){ //if not allocated
        //     current_world(xx,yy,zz) = palette_counter;
        //     palette_counter++;
        // } else {
            if(current_block < static_block_palette_size) { // static and non-empty
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
        // } //TODO copy empty instead of whole image
    }}}
}
void Renderer::endBlockify(){
    // vkDeviceWaitIdle(device);
    size_t size_to_copy = world_size.x * world_size.y * world_size.z * sizeof(BlockID_t);
    assert(size_to_copy != 0);
    memcpy(staging_world_mapped[currentFrame], current_world.data(), size_to_copy);
    vmaFlushAllocation(VMAllocator, staging_world[currentFrame].alloc, 0, size_to_copy);
}

void Renderer::execCopies(){
    VkCommandBuffer &commandBuffer = computeCommandBuffers[currentFrame];

    cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

    if(copy_queue.size() != 0){
        //we can copy from previous image, cause static blocks are same in both palettes. Additionaly, it gives src and dst optimal layouts
        cmdTransLayoutBarrier(commandBuffer, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT , VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT, &origin_block_palette[previousFrame]);
        cmdTransLayoutBarrier(commandBuffer, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT , VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT, &origin_block_palette[ currentFrame]);

        // typedef void (VKAPI_PTR *PFN_vkCmdCopyImage)(VkCommandBuffer commandBuffer, VkImage srcImage, VkImageLayout srcImageLayout, VkImage dstImage, VkImageLayout dstImageLayout, uint32_t regionCount, const VkImageCopy* pRegions);
        // printl(previousFrame)
        // printl( currentFrame)
        // assert(previousFrame!=currentFrame);
        // if(previousFrame != cur)
        vkCmdCopyImage(commandBuffer, origin_block_palette[previousFrame].image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, origin_block_palette[currentFrame].image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, copy_queue.size(), copy_queue.data());

        cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

        cmdTransLayoutBarrier(commandBuffer, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT, &origin_block_palette[previousFrame]);
        cmdTransLayoutBarrier(commandBuffer, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT, &origin_block_palette[ currentFrame]);
    }
    VkBufferImageCopy copyRegion = {};
    // copyRegion.size = size;
        copyRegion.imageExtent.width  = world_size.x;
        copyRegion.imageExtent.height = world_size.y;
        copyRegion.imageExtent.depth  = world_size.z;
        copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copyRegion.imageSubresource.layerCount = 1;
    cmdTransLayoutBarrier(commandBuffer, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_MEMORY_WRITE_BIT|VK_ACCESS_MEMORY_READ_BIT, VK_ACCESS_MEMORY_WRITE_BIT|VK_ACCESS_MEMORY_READ_BIT, &world);
    vkCmdCopyBufferToImage(commandBuffer, staging_world[currentFrame].buffer, world.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);
    // cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
    // VkImageMemoryBarrier staging_world_barrier{};
    //     staging_world_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    //     staging_world_barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    //     staging_world_barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    //     staging_world_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    //     staging_world_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    //     staging_world_barrier.image = world.image;
    //     staging_world_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    //     staging_world_barrier.subresourceRange.baseMipLevel = 0;
    //     staging_world_barrier.subresourceRange.levelCount = 1;
    //     staging_world_barrier.subresourceRange.baseArrayLayer = 0;
    //     staging_world_barrier.subresourceRange.layerCount = 1;
    //     staging_world_barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT| VK_ACCESS_MEMORY_READ_BIT;
    //     staging_world_barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    // cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, &staging_world_barrier);
    cmdTransLayoutBarrier(commandBuffer, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT, &world);
    cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
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
            blockPaletteInfo.imageView = origin_block_palette[currentFrame].view;
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

    dmat4 rotate = toMat4((*mesh).rot);
    dmat4 shift = translate(identity<mat4>(), (*mesh).shift);
    dmat4 transform = shift*rotate;
    
    struct AABB border_in_voxel = get_shift(transform, (*mesh).size);
    
    struct {ivec3 min; ivec3 max;} border; 
    border.min = ivec3(floor(border_in_voxel.min)) - ivec3(0);// no -ivec3(1) cause IT STRETCHES MODELS;
    border.max = ivec3( ceil(border_in_voxel.max)) + ivec3(0);// no +ivec3(1) cause IT STRETCHES MODELS;
    //todo: clamp here not in shader
    ivec3 map_area = (border.max - border.min);

    // struct {mat4 trans; ivec3 size;} trans_size = {mesh.transform, mesh.size};
    struct {mat4 trans; ivec4 shift;} itrans_shift = {mat4(inverse(transform)), ivec4(border.min, 0)};
    // trans = glm::scale(trans, vec3(0.95));
    vkCmdPushConstants(commandBuffer, mapLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(itrans_shift), &itrans_shift);
    // ivec3 adjusted_size = ivec3(vec3(mesh.size) * vec3(2.0));
    vkCmdDispatch(commandBuffer, (map_area.x*3+3) / 4, (map_area.y*3+3) / 4, (map_area.z*3+3) / 4);   
}

void Renderer::endMap(){
    VkCommandBuffer &commandBuffer = computeCommandBuffers[currentFrame];

    cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
    cmdTransLayoutBarrier(commandBuffer, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT, VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT, &origin_block_palette[currentFrame]);
    cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
}

void Renderer::raytrace(){
    VkCommandBuffer &commandBuffer = computeCommandBuffers[currentFrame];
    
    cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

    cmdTransLayoutBarrier(commandBuffer, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT, VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT, 
        &lowres_frames[currentFrame]);
    
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, raytracePipeline);

        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, raytraceLayout, 0, 1, &raytraceDescriptorSets[currentFrame], 0, 0);

        old_camera_pos = camera_pos;
        old_camera_dir = camera_dir;
        struct rtpc {vec4 v1, v2;} raytrace_pushconstant = {vec4(camera_pos,intBitsToFloat(itime)), vec4(camera_dir,0)};
        vkCmdPushConstants(commandBuffer, raytraceLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(raytrace_pushconstant), &raytrace_pushconstant);
        vkCmdDispatch(commandBuffer, (raytraceExtent.width+7)/8, (raytraceExtent.height+7)/8, 1);

    VkImageMemoryBarrier barrier{};
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
        barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;    

    // cmdTransLayoutBarrier(VkCommandBuffer commandBuffer, VkImageLayout targetLayout, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask, Image *image)
        barrier.image = is_scaled? lowres_frames[currentFrame].image : highres_frames[currentFrame].image;
    cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, &barrier);    
        barrier.image = step_count.image;
    cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, &barrier);   
        cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
        barrier.image = is_scaled? lowres_frames[currentFrame].image : highres_frames[currentFrame].image;
    cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, &barrier);   
    cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
}
void Renderer::updadeRadiance(){
    VkCommandBuffer &commandBuffer = computeCommandBuffers[currentFrame];
    
    cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

    cmdTransLayoutBarrier(commandBuffer, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT, VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT, 
        &lowres_frames[currentFrame]);
    
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, radiancePipeline);

        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, radianceLayout, 0, 1, &updateRadianceDescriptorSets[currentFrame], 0, 0);

        old_camera_pos = camera_pos;
        old_camera_dir = camera_dir;
        struct rtpc {int time;} pushconstant = {itime};
        vkCmdPushConstants(commandBuffer, radianceLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pushconstant), &pushconstant);
        
        // vkCmdDispatch(commandBuffer, RCACHE_RAYS_PER_PROBE*world_size.x, world_size.y, world_size.z);
        // vkCmdDispatch(commandBuffer, RCACHE_RAYS_PER_PROBE*world_size.x, world_size.y, 7);
        int magic_number = itime % 2;
        // ivec3 fullsize = ivec3(RCACHE_RAYS_PER_PROBE*world_size.x, world_size.y, 7);
        // ivec3 partial_dispatch_size;
        //       partial_dispatch_size.x = (fullsize.x)/2;
        //       partial_dispatch_size.y = fullsize.y;
        //       partial_dispatch_size.z = fullsize.z;

        // ivec3 dispatch_shift = ivec3(partial_dispatch_size.x, 0, 0) * magic_number;
        vkCmdDispatchBase(
            commandBuffer,
            // (world_size.x /2) * magic_number, 0, 0,
            0, 0, 0,
            (world_size.x /2), world_size.y, 7);
            // dispatch_shift.x, dispatch_shift.y, dispatch_shift.z,
            // fullsize.x, fullsize.y, fullsize.z);
            // partial_dispatch_size.x, partial_dispatch_size.y, partial_dispatch_size.z);

    VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = is_scaled? lowres_frames[currentFrame].image : highres_frames[currentFrame].image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;    

    // cmdTransLayoutBarrier(VkCommandBuffer commandBuffer, VkImageLayout targetLayout, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask, Image *image)
    barrier.image = radiance_cache.image;
    cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, &barrier);   
    cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
}
void Renderer::doLight(){
    VkCommandBuffer &commandBuffer = computeCommandBuffers[currentFrame];
    
    cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

    cmdTransLayoutBarrier(commandBuffer, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT, VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT, 
        &lowres_frames[currentFrame]);
    
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, doLightPipeline);

        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, doLightLayout, 0, 1, &doLightDescriptorSets[currentFrame], 0, 0);

        old_camera_pos = camera_pos;
        old_camera_dir = camera_dir;
        struct rtpc {vec4 v1, v2;} pushconstant = {vec4(camera_pos,intBitsToFloat(itime)), vec4(camera_dir,0)};
        vkCmdPushConstants(commandBuffer, doLightLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pushconstant), &pushconstant);
        
        vkCmdDispatch(commandBuffer, (raytraceExtent.width+7)/8, (raytraceExtent.height+7)/8, 1);

    VkImageMemoryBarrier barrier{};
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
        barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;    

    // cmdTransLayoutBarrier(VkCommandBuffer commandBuffer, VkImageLayout targetLayout, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask, Image *image)
    barrier.image = is_scaled? lowres_frames[currentFrame].image : highres_frames[currentFrame].image;
    cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, &barrier);   
    barrier.image = radiance_cache.image;
    cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, &barrier);   
    cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
}
void Renderer::denoise(int iterations, int denoising_radius, denoise_targets target){
    VkCommandBuffer &commandBuffer = computeCommandBuffers[currentFrame];
    
    cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
    VkImageMemoryBarrier barrier{};
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
        barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT |VK_ACCESS_MEMORY_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, denoisePipeline);
        VkExtent2D dispatchExtent;
        switch (target) {
            case DENOISE_TARGET_LOWRES:
                barrier.image = lowres_frames[currentFrame].image;
                vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, denoiseLayout, 0, 1, &denoiseDescriptorSets_lowres[currentFrame], 0, 0);
                dispatchExtent = raytraceExtent;
                break;
            case DENOISE_TARGET_HIGHRES:
                barrier.image = is_scaled? highres_frames[0].image : highres_frames[currentFrame].image;
                vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, denoiseLayout, 0, 1, &denoiseDescriptorSets_highres[currentFrame], 0, 0);
                dispatchExtent = swapChainExtent;
                break;
        }

        for(int atrous_iter = 0; atrous_iter<iterations; atrous_iter++){
            struct denoise_pc {int iter; int radius;} dpc = {atrous_iter, denoising_radius};
            vkCmdPushConstants(commandBuffer, denoiseLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(dpc), &dpc);
            vkCmdDispatch(commandBuffer, (dispatchExtent.width+7)/8, (dispatchExtent.height+7)/8, 1);
            cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, &barrier);
        } 

    cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
}
void Renderer::accumulate(){
    VkCommandBuffer &commandBuffer = computeCommandBuffers[currentFrame];

    cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

    cmdTransLayoutBarrier(commandBuffer, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT, VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT,
        &mix_ratio[currentFrame]); 
    
    VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = is_scaled? lowres_frames[currentFrame].image : highres_frames[currentFrame].image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT |VK_ACCESS_MEMORY_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, accumulatePipeline);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, accumulateLayout, 0, 1, &accumulateDescriptorSets[currentFrame], 0, 0);
        vkCmdDispatch(commandBuffer, (raytraceExtent.width+7)/8, (raytraceExtent.height+7)/8, 1);
    cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, &barrier); 

        barrier.image = is_scaled? lowres_frames[currentFrame].image : highres_frames[currentFrame].image;
    cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, &barrier); 
        barrier.image = mix_ratio[currentFrame].image;
    cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, &barrier); 
    cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
}
void Renderer::upscale(){
    if(!is_scaled) crash(no reason to upscale with 1:1 ratio?);

    VkCommandBuffer &commandBuffer = computeCommandBuffers[currentFrame];

    cmdTransLayoutBarrier(commandBuffer, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT, VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT,
        &lowres_frames[currentFrame]); 
    cmdTransLayoutBarrier(commandBuffer, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT, VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT,
        &highres_frames[0]); 
        
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, upscalePipeline);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, upscaleLayout, 0, 1, &upscaleDescriptorSets[currentFrame], 0, 0);
        vkCmdDispatch(commandBuffer, (swapChainExtent.width+7)/8, (swapChainExtent.height+7)/8, 1);

    VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = highres_frames[0].image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;    
    cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, &barrier); 
}
void Renderer::endCompute(){
    VkCommandBuffer &commandBuffer = computeCommandBuffers[currentFrame];
    cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
    VK_CHECK(vkEndCommandBuffer(commandBuffer));
        vector<VkSemaphore> computeWaitSemaphores = {rayGenFinishedSemaphores[currentFrame]};
        VkPipelineStageFlags computeWaitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    //     VkSubmitInfo submitInfo = {};
    //         submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    //         submitInfo.pWaitDstStageMask = computeWaitStages;
    //         submitInfo.waitSemaphoreCount = computeWaitSemaphores.size();
    //         submitInfo.pWaitSemaphores    = computeWaitSemaphores.data();
    //         submitInfo.commandBufferCount = 1;
    //         submitInfo.pCommandBuffers = &computeCommandBuffers[currentFrame];
    //         submitInfo.signalSemaphoreCount = 1;
    //         submitInfo.pSignalSemaphores = &raytraceFinishedSemaphores[currentFrame];
    // VK_CHECK(vkQueueSubmit(computeQueue, 1, &submitInfo, raytraceInFlightFences[currentFrame]));
}
void Renderer::start_ui(){
    //cause we render ui directly to swapchain image
    // VkResult result = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);
    VkResult result = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        // cout << KRED"failed to present swap chain image!\n" KEND;
        // recreate_SwapchainDependent();
        is_resized = true;
        // return; // can be avoided, but it is just 1 frame 
    } else if ((result != VK_SUCCESS)) {
        printf(KRED "failed to acquire swap chain image!\n" KEND);
        exit(result);
    }

    // vkWaitForFences(device, 1, &graphicalInFlightFences[currentFrame], VK_TRUE, UINT32_MAX);
    // vkResetFences  (device, 1, &graphicalInFlightFences[currentFrame]);

    vkResetCommandBuffer(overlayCommandBuffers[currentFrame], 0);
    vkResetCommandBuffer(copyOverlayCommandBuffers[currentFrame], 0);
    vkResetCommandBuffer(renderOverlayCommandBuffers[currentFrame], 0);
    VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = 0;
        beginInfo.pInheritanceInfo = NULL;
    VK_CHECK(vkBeginCommandBuffer(overlayCommandBuffers[currentFrame], &beginInfo));
    
    VkCommandBufferInheritanceInfo cmd_buf_inheritance_info = {};
        cmd_buf_inheritance_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO, cmd_buf_inheritance_info.pNext = NULL;
        cmd_buf_inheritance_info.subpass = 0;
        cmd_buf_inheritance_info.occlusionQueryEnable = VK_FALSE;
        cmd_buf_inheritance_info.queryFlags = 0;
        cmd_buf_inheritance_info.pipelineStatistics = 0;

        cmd_buf_inheritance_info.renderPass = NULL;
        cmd_buf_inheritance_info.framebuffer = NULL;
    beginInfo.flags = 0;
    beginInfo.pInheritanceInfo = &cmd_buf_inheritance_info;
    VK_CHECK(vkBeginCommandBuffer(copyOverlayCommandBuffers[currentFrame], &beginInfo));
        cmd_buf_inheritance_info.renderPass = graphicalRenderPass;
        cmd_buf_inheritance_info.framebuffer = swapChainFramebuffers[imageIndex];
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
        beginInfo.pInheritanceInfo = &cmd_buf_inheritance_info;
    VK_CHECK(vkBeginCommandBuffer(renderOverlayCommandBuffers[currentFrame], &beginInfo));
    vkCmdBindPipeline(renderOverlayCommandBuffers[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, graphicsPipeline);

    VkViewport viewport = {};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width  = (float)(swapChainExtent.width );
        viewport.height = (float)(swapChainExtent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
    vkCmdSetViewport(renderOverlayCommandBuffers[currentFrame], 0, 1, &viewport);

    UiRenderInterface->last_scissors.offset = {0,0};
    UiRenderInterface->last_scissors.extent = swapChainExtent;
    vkCmdSetScissor(renderOverlayCommandBuffers[currentFrame], 0, 1, &UiRenderInterface->last_scissors);

}

void Renderer::draw_ui(){
    VkCommandBuffer &commandBuffer = overlayCommandBuffers[currentFrame];

    VkImageCopy copy_to_swapchain = {};
        copy_to_swapchain.extent.width  = swapChainExtent.width;
        copy_to_swapchain.extent.height = swapChainExtent.height;
        copy_to_swapchain.extent.depth  = 1;
        copy_to_swapchain.dstOffset = {0,0};
        copy_to_swapchain.srcOffset = {0,0};
        copy_to_swapchain.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copy_to_swapchain.srcSubresource.baseArrayLayer = 0;
        copy_to_swapchain.srcSubresource.layerCount = 1;
        copy_to_swapchain.srcSubresource.mipLevel = 0;
        copy_to_swapchain.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copy_to_swapchain.dstSubresource.baseArrayLayer = 0;
        copy_to_swapchain.dstSubresource.layerCount = 1;
        copy_to_swapchain.dstSubresource.mipLevel = 0;
    VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;  

    barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.image = is_scaled? highres_frames[0].image : highres_frames[currentFrame].image;
    cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, &barrier); 
    

    // vklayout
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
    cmdTransLayoutBarrier(commandBuffer, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT, 
        is_scaled? &highres_frames[0] : &highres_frames[currentFrame]);    

    barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.image = swapchain_images[imageIndex].image;
    cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, &barrier); 
    // cmdTransLayoutBarrier(commandBuffer, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT, 
    //     &swapchain_images[imageIndex]);    
    vkCmdBlitImage(commandBuffer, (is_scaled? highres_frames[0].image : highres_frames[currentFrame].image), VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, swapchain_images[imageIndex].image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &blit, VK_FILTER_NEAREST);

    barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    barrier.image = swapchain_images[imageIndex].image;
    cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, &barrier);
    cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, &barrier);
    // barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    // cmdTransLayoutBarrier(commandBuffer, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT , &swapchain_images[imageIndex]);    

    //copying done
    VK_CHECK(vkEndCommandBuffer(copyOverlayCommandBuffers[currentFrame]));
    VK_CHECK(vkEndCommandBuffer(renderOverlayCommandBuffers[currentFrame]));

    //copies
    vkCmdExecuteCommands(commandBuffer, 1, &copyOverlayCommandBuffers[currentFrame]);

    VkClearValue clearColor = {{{0.111f, 0.444f, 0.666f, 1.0f}}};
    VkRenderPassBeginInfo renderPassInfo = {};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = graphicalRenderPass;
        renderPassInfo.framebuffer = swapChainFramebuffers[imageIndex];
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = swapChainExtent;
        renderPassInfo.clearValueCount = 1;
        renderPassInfo.pClearValues = &clearColor;
        // renderPassInfo.
    
    cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_SECONDARY_COMMAND_BUFFERS);
        vkCmdExecuteCommands(commandBuffer, 1, &renderOverlayCommandBuffers[currentFrame]);
    vkCmdEndRenderPass(commandBuffer);

    VK_CHECK(vkEndCommandBuffer(commandBuffer));
}

void Renderer::present(){
    // vector<VkSemaphore> waitSemaphores = {raytraceFinishedSemaphores[currentFrame], imageAvailableSemaphores[currentFrame]};
    vector<VkSemaphore> signalSemaphores = {renderFinishedSemaphores[currentFrame]};
    //TODO: replace with VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT
    // VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_VERTEX_INPUT_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    // VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    
    //ui drawn, and it waits for everything else already
    vector<VkSemaphore> waitSemaphores = {imageAvailableSemaphores[currentFrame]};
    VkPipelineStageFlags waitStages[] = {VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT | VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
    // vector<VkCommandBuffer>    rayGenCommandBuffers;
    // vector<VkCommandBuffer>    rayGenSecondaryCommandBuffer;
    //It is so because they are too similar and easy to record (TODO: make concurent)
    // vector<VkCommandBuffer>   computeCommandBuffers; 
    // vector<VkCommandBuffer> graphicalCommandBuffers;
    // vector<VkCommandBuffer> copyGraphicalCommandBuffers;
    // vector<VkCommandBuffer> renderGraphicalCommandBuffers;
    vector<VkCommandBuffer> commandBuffers = {rayGenCommandBuffers[currentFrame], computeCommandBuffers[currentFrame], overlayCommandBuffers[currentFrame]};
    VkSubmitInfo submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.waitSemaphoreCount = waitSemaphores.size();
        submitInfo.pWaitSemaphores    = waitSemaphores.data();
        // submitInfo.waitSemaphoreCount = 0;
        // submitInfo.pWaitSemaphores    = NULL;
        submitInfo.pWaitDstStageMask = waitStages;
        submitInfo.commandBufferCount = commandBuffers.size();
        submitInfo.pCommandBuffers = commandBuffers.data();
        submitInfo.signalSemaphoreCount = signalSemaphores.size();
        submitInfo.pSignalSemaphores    = signalSemaphores.data();
        
    VkSwapchainKHR swapchains[] = {swapchain};
    VkPresentInfoKHR presentInfo = {};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = signalSemaphores.size();
        presentInfo.pWaitSemaphores = signalSemaphores.data();
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapchains;
        presentInfo.pImageIndices = &imageIndex;
        presentInfo.pResults = NULL;
        // presentInfo.

// println
    VK_CHECK(vkQueueSubmit(graphicsQueue, 1, &submitInfo, frameInFlightFences[currentFrame]));
// println
    VkResult result = vkQueuePresentKHR(presentQueue, &presentInfo);

// println

    

    
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || is_resized) {
        is_resized = false; 
        // cout << KRED"failed to present swap chain image!\n" KEND;
// println
        recreate_SwapchainDependent();
// println
    } else if (result != VK_SUCCESS) {
        cout << KRED"failed to present swap chain image!\n" KEND;
        exit(result);
    }
}

void Renderer::end_Frame(){
// println
    previousFrame = currentFrame;
     currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    nextFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    itime++;
    process_ui_deletion_queue();
// println
}

// void Renderer::create_Image_Storages_DepthStencil(vector<VkImage>* images, vector<VmaAllocation>* allocs, vector<VkImageView>* depthViews, vector<VkImageView>* stencilViews,
//     VkImageType type, VkFormat format, VkImageUsageFlags usage, VmaMemoryUsage vma_usage, VmaAllocationCreateFlags vma_flags, VkImageAspectFlags aspect, VkImageLayout layout, VkPipelineStageFlags pipeStage, VkAccessFlags access, 
//     uvec3 size){
//     (*images).resize(MAX_FRAMES_IN_FLIGHT);
//     (*depthViews).resize(MAX_FRAMES_IN_FLIGHT);
//     (*stencilViews).resize(MAX_FRAMES_IN_FLIGHT);
//     (*allocs).resize(MAX_FRAMES_IN_FLIGHT);
    
//     for (i32 i=0; i < MAX_FRAMES_IN_FLIGHT; i++){
//         VkImageCreateInfo imageInfo = {};
//             imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
//             imageInfo.imageType = type;
//             imageInfo.format = format;
//             imageInfo.mipLevels = 1;
//             imageInfo.arrayLayers = 1;
//             imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
//             imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;

//             imageInfo.usage = usage;
//             imageInfo.extent.width  = size.x;
//             imageInfo.extent.height = size.y;
//             imageInfo.extent.depth  = size.z;
//             imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
//         VmaAllocationCreateInfo allocInfo = {};
//             allocInfo.usage = vma_usage;
//             allocInfo.flags = vma_flags;
//             // allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
//             // allocInfo.flags = VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT;

//             // allocInfo.requiredFlags = 
//         VK_CHECK(vmaCreateImage(VMAllocator, &imageInfo, &allocInfo, &(*images)[i], &(*allocs)[i], NULL));

//         VkImageViewCreateInfo depthViewInfo = {};
//             depthViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
//             depthViewInfo.image = (*images)[i];
//             depthViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
//             depthViewInfo.format = format;
//             depthViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
//             depthViewInfo.subresourceRange.baseMipLevel = 0;
//             depthViewInfo.subresourceRange.baseArrayLayer = 0;
//             depthViewInfo.subresourceRange.levelCount = 1;
//             depthViewInfo.subresourceRange.layerCount = 1;
//         VK_CHECK(vkCreateImageView(device, &depthViewInfo, NULL, &(*depthViews)[i]));
//         VkImageViewCreateInfo stencilViewInfo = {};
//             stencilViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
//             stencilViewInfo.image = (*images)[i];
//             stencilViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
//             stencilViewInfo.format = format;
//             stencilViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT;
//             stencilViewInfo.subresourceRange.baseMipLevel = 0;
//             stencilViewInfo.subresourceRange.baseArrayLayer = 0;
//             stencilViewInfo.subresourceRange.levelCount = 1;
//             stencilViewInfo.subresourceRange.layerCount = 1;
//         VK_CHECK(vkCreateImageView(device, &stencilViewInfo, NULL, &(*stencilViews)[i]));

//         transition_Image_Layout_Singletime((*images)[i], format, aspect, VK_IMAGE_LAYOUT_UNDEFINED, layout,
//             VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, pipeStage,
//             0, access);
//     }
// }
//VkCommandBuffer commandBuffer, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkDependencyFlags dependencyFlags, 
//uint32_t        memoryBarrierCount, const       VkMemoryBarrier* pMemoryBarriers,
// uint32_t bufferMemoryBarrierCount, const VkBufferMemoryBarrier* pBufferMemoryBarriers,
// uint32_t  imageMemoryBarrierCount, const  VkImageMemoryBarrier* pImageMemoryBarriers
void Renderer::cmdPipelineBarrier(VkCommandBuffer commandBuffer, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkImageMemoryBarrier* barrier){
    vkCmdPipelineBarrier(
        commandBuffer,
        srcStageMask, dstStageMask,
        0,
        0, NULL,
        0, NULL,
        1, barrier
    );
}
void Renderer::cmdPipelineBarrier(VkCommandBuffer commandBuffer, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkBufferMemoryBarrier* barrier){
    vkCmdPipelineBarrier(
        commandBuffer,
        srcStageMask, dstStageMask,
        0,
        0, NULL,
        1, barrier,
        0, NULL
    );
}
void Renderer::cmdPipelineBarrier(VkCommandBuffer commandBuffer, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask){
    vkCmdPipelineBarrier(
        commandBuffer,
        srcStageMask, dstStageMask,
        0,
            0, NULL,
            0, NULL,
            0, NULL
    );
}
#define get_layout_helper_define(layout) case layout: return #layout;

static const char* get_layout_name(VkImageLayout layout){
    switch (layout) {
    get_layout_helper_define(VK_IMAGE_LAYOUT_UNDEFINED)
    get_layout_helper_define(VK_IMAGE_LAYOUT_GENERAL)
    get_layout_helper_define(VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
    get_layout_helper_define(VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
    get_layout_helper_define(VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL)
    get_layout_helper_define(VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
    get_layout_helper_define(VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
    get_layout_helper_define(VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
    default: return "invalid layout";
    }
}
void Renderer::cmdTransLayoutBarrier(VkCommandBuffer commandBuffer, VkImageLayout targetLayout,
        VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask, 
        Image* image){

    // printl(get_layout_name(targetLayout))
    // printl(get_layout_name((*image).layout))
    
    VkImageMemoryBarrier barrier = {};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = (*image).layout;
        barrier.newLayout = targetLayout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = (*image).image;
        barrier.subresourceRange.aspectMask = (*image).aspect;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcAccessMask = srcAccessMask;
        barrier.dstAccessMask = dstAccessMask;  
    vkCmdPipelineBarrier(
        commandBuffer,
        srcStageMask, dstStageMask,
        0,
        0, NULL,
        0, NULL,
        1, &barrier
    );
    (*image).layout = targetLayout;
}
void Renderer::create_Image_Storages(vector<Image>* images, 
    VkImageType type, VkFormat format, VkImageUsageFlags usage, VmaMemoryUsage vma_usage, VmaAllocationCreateFlags vma_flags, VkImageAspectFlags aspect, 
    uvec3 size, VkSampleCountFlagBits sample_count){
    (*images).resize(MAX_FRAMES_IN_FLIGHT);
    

    for (i32 i=0; i < MAX_FRAMES_IN_FLIGHT; i++){
        (*images)[i].aspect = aspect;
        (*images)[i].layout = VK_IMAGE_LAYOUT_UNDEFINED;

        VkImageCreateInfo imageInfo = {};
            imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            imageInfo.imageType = type;
            imageInfo.format = format;
            imageInfo.mipLevels = 1;
            imageInfo.arrayLayers = 1;
            imageInfo.samples = sample_count;
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

        // transition_Image_Layout_Singletime((*images)[i].image, format, aspect, VK_IMAGE_LAYOUT_UNDEFINED, layout,
        //     VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, pipeStage,
        //     0, access);
        // if(!(vma_usage & VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT)){
        // }
    }
}
void Renderer::create_Image_Storages(Image* image, 
    VkImageType type, VkFormat format, VkImageUsageFlags usage, VmaMemoryUsage vma_usage, VmaAllocationCreateFlags vma_flags, VkImageAspectFlags aspect, 
    uvec3 size, VkSampleCountFlagBits sample_count){

    image->aspect = aspect;
    image->layout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkImageCreateInfo imageInfo = {};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = type;
        imageInfo.format = format;
        imageInfo.mipLevels = 1;
        imageInfo.arrayLayers = 1;
        imageInfo.samples = sample_count;
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

    // transition_Image_Layout_Singletime(image->image, format, aspect, VK_IMAGE_LAYOUT_UNDEFINED, layout,
    //     VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
    //     0, access);
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
                allocInfo.requiredFlags = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
            }
            allocInfo.usage = VMA_MEMORY_USAGE_AUTO;
        VK_CHECK(vmaCreateBuffer(VMAllocator, &bufferInfo, &allocInfo, &(*buffers)[i].buffer, &(*buffers)[i].alloc, NULL));
    }
}

vector<Image> Renderer::create_RayTrace_VoxelImages(Voxel* voxels, ivec3 size){
    VkDeviceSize bufferSize = (sizeof(Voxel))*size.x*size.y*size.z;

    vector<Image> voxelImages = {};

    create_Image_Storages(&voxelImages,
        VK_IMAGE_TYPE_3D,
        VK_FORMAT_R8_UINT,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT,
        size);
    for (int i=0; i<MAX_FRAMES_IN_FLIGHT; i++){
        transition_Image_Layout_Singletime(&voxelImages[i], VK_IMAGE_LAYOUT_GENERAL);
    }

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
    vmaMapMemory(VMAllocator, stagingAllocation, &data);
        memcpy(data, voxels, bufferSize);
    vmaUnmapMemory(VMAllocator, stagingAllocation);

    for(i32 i=0; i<MAX_FRAMES_IN_FLIGHT; i++){
        copy_Buffer(stagingBuffer, &voxelImages[i], size);
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
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, //depth    (!downscaled)
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, //blocks
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, //block palette
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, //mat palette
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, //step counts
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, //out frame (raytraced)
        }, 
        VK_SHADER_STAGE_COMPUTE_BIT, raytraceDescriptorSetLayout, device);

    create_Descriptor_Set_Layout_Helper({
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, //r16i       ) uniform iimage3D blocks;
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, //r8ui       ) uniform uimage3D blockPalette;
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, //r32f       ) uniform image2D voxelPalette;
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, //rgba16f    ) uniform image3D radianceCache;
        },
        VK_SHADER_STAGE_COMPUTE_BIT, updateRadianceDescriptorSetLayout, device);

    create_Descriptor_Set_Layout_Helper({
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, //rgba8_snorm) uniform   image2D matNorm;
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, //  ) uniform sampler2D depthBuffer;
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, //r16i       ) uniform  iimage3D blocks;
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, //r8ui       ) uniform  uimage3D blockPalette;
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, //r32f       ) uniform   image2D voxelPalette;
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, //rgba8ui    ) uniform  uimage2D step_count;
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, //rgba16     ) uniform   image2D outFrame;
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, //rgba16f    ) uniform   image3D radianceCache;
        }, 
        VK_SHADER_STAGE_COMPUTE_BIT, doLightDescriptorSetLayout, device);
        
    create_Descriptor_Set_Layout_Helper({
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, //mat norm
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, //depth
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, //raytraced
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, //denoised (final output) (currently same image)
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, //mix ratio
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, //mat palette
        }, 
        VK_SHADER_STAGE_COMPUTE_BIT, denoiseDescriptorSetLayout, device);

    create_Descriptor_Set_Layout_Helper({
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, //mat norm (downscaled)
        // VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, //depth    (downscaled)
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, //mat norm (highres)
        // VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, //depth    (highres)
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, //denoised in
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, //upscaled out
        // VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, //oldUv
        }, 
        VK_SHADER_STAGE_COMPUTE_BIT, upscaleDescriptorSetLayout, device);

    create_Descriptor_Set_Layout_Helper({
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, //new frame
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, //old frame
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, //new mat norm
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, //old mat norm
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, //oldUv
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, //new mixRatio
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, //old mixRatio
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, //mixRatio
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, //mixRatio
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, //mat palette
        }, 
        VK_SHADER_STAGE_COMPUTE_BIT, accumulateDescriptorSetLayout, device);

    // create_Descriptor_Set_Layout_Helper({
    //     VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, //mat norm
    //     VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, //depth
    //     VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, //accumulated
    //     VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, //variance estimation
    //     }, 
    //     VK_SHADER_STAGE_COMPUTE_BIT, estimateVarianceDescriptorSetLayout, device);
        
    create_Descriptor_Set_Layout_Helper({
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, //frame-constant world_to_screen mat4
        }, 
        VK_SHADER_STAGE_VERTEX_BIT, RayGenDescriptorSetLayout, device);
    create_Descriptor_Set_Layout_Helper({
        VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, //frame-constant world_to_screen mat4
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, //blocks
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, //voxel palette
        }, 
        VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT, RayGenParticlesDescriptorSetLayout, device);

    create_Descriptor_Set_Layout_Helper({
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, //in texture (ui)
        }, 
        VK_SHADER_STAGE_FRAGMENT_BIT, graphicalDescriptorSetLayout, device,
        VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR);
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
        poolInfo.maxSets = MAX_FRAMES_IN_FLIGHT*10; //becuase frames_in_flight multiply of 5 differents sets, each for shader 
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
    allocate_Descriptors_helper(  raytraceDescriptorSets,   raytraceDescriptorSetLayout, descriptorPool, device); 
    allocate_Descriptors_helper(   denoiseDescriptorSets_lowres,    denoiseDescriptorSetLayout, descriptorPool, device); 
    allocate_Descriptors_helper(   denoiseDescriptorSets_highres,   denoiseDescriptorSetLayout, descriptorPool, device); 
    allocate_Descriptors_helper(   upscaleDescriptorSets,    upscaleDescriptorSetLayout, descriptorPool, device); 
    allocate_Descriptors_helper(accumulateDescriptorSets, accumulateDescriptorSetLayout, descriptorPool, device); 
    // allocate_Descriptors_helper( blockifyDescriptorSets,  blockifyDescriptorSetLayout, descriptorPool, device); 
    // allocate_Descriptors_helper(     copyDescriptorSets,      copyDescriptorSetLayout, descriptorPool, device); 
    // allocate_Descriptors_helper(     dfDescriptorSets,      dfDescriptorSetLayout, descriptorPool, device); 
    allocate_Descriptors_helper(updateRadianceDescriptorSets,   updateRadianceDescriptorSetLayout, descriptorPool, device); 
    allocate_Descriptors_helper(       doLightDescriptorSets,          doLightDescriptorSetLayout, descriptorPool, device); 
    
    //we do not allocate this because it's descriptors are managed trough push_descriptor mechanism
    // allocate_Descriptors_helper(      mapDescriptorSets,       mapDescriptorSetLayout, descriptorPool, device); 
    // allocate_Descriptors_helper(graphicalDescriptorSets, graphicalDescriptorSetLayout, descriptorPool, device);     

    allocate_Descriptors_helper(RayGenDescriptorSets, RayGenDescriptorSetLayout, descriptorPool, device);     
    allocate_Descriptors_helper(RayGenParticlesDescriptorSets, RayGenParticlesDescriptorSetLayout, descriptorPool, device);      
}
void Renderer::setup_Raytrace_Descriptors(){
    for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        int previous_frame = i - 1;
        // int previous_frame = i;
        if (previous_frame < 0) previous_frame = MAX_FRAMES_IN_FLIGHT-1; // so frames do not intersect

        VkDescriptorImageInfo matNormInfo = {};
            matNormInfo.imageView = is_scaled? matNorm_lowres[i].view : matNorm_highres[i].view;
            matNormInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        VkDescriptorImageInfo depthInfo = {};
            depthInfo.imageView = is_scaled? depthBuffers_lowres[i].view : depthBuffers_highres[i].view;
            depthInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
            depthInfo.sampler = nearestSampler;
        VkDescriptorImageInfo inputBlockInfo = {};
            inputBlockInfo.imageView = world.view;
            inputBlockInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        VkDescriptorImageInfo inputBlockPaletteInfo = {};
            inputBlockPaletteInfo.imageView = origin_block_palette[i].view;
            inputBlockPaletteInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        VkDescriptorImageInfo inputVoxelPaletteInfo = {};
            inputVoxelPaletteInfo.imageView = material_palette[i].view;
            inputVoxelPaletteInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        VkDescriptorImageInfo outputFrameInfo = {};
            outputFrameInfo.imageView = is_scaled? lowres_frames[i].view : highres_frames[i].view;
            outputFrameInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        VkDescriptorImageInfo stepCountInfo = {};
            stepCountInfo.imageView = step_count.view;
            stepCountInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            
        VkWriteDescriptorSet matNormWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            matNormWrite.dstSet = raytraceDescriptorSets[i];
            matNormWrite.dstBinding = 0;
            matNormWrite.dstArrayElement = 0;
            matNormWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            matNormWrite.descriptorCount = 1;
            matNormWrite.pImageInfo = &matNormInfo;
        VkWriteDescriptorSet depthWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            depthWrite.dstSet = raytraceDescriptorSets[i];
            depthWrite.dstBinding = 1;
            depthWrite.dstArrayElement = 0;
            depthWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            depthWrite.descriptorCount = 1;
            depthWrite.pImageInfo = &depthInfo;
        VkWriteDescriptorSet blockWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            blockWrite.dstSet = raytraceDescriptorSets[i];
            blockWrite.dstBinding = 2;
            blockWrite.dstArrayElement = 0;
            blockWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            blockWrite.descriptorCount = 1;
            blockWrite.pImageInfo = &inputBlockInfo;
        VkWriteDescriptorSet blockPaletteWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            blockPaletteWrite.dstSet = raytraceDescriptorSets[i];
            blockPaletteWrite.dstBinding = 3;
            blockPaletteWrite.dstArrayElement = 0;
            blockPaletteWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            blockPaletteWrite.descriptorCount = 1;
            blockPaletteWrite.pImageInfo = &inputBlockPaletteInfo;
        VkWriteDescriptorSet voxelPaletteWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            voxelPaletteWrite.dstSet = raytraceDescriptorSets[i];
            voxelPaletteWrite.dstBinding = 4;
            voxelPaletteWrite.dstArrayElement = 0;
            voxelPaletteWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            voxelPaletteWrite.descriptorCount = 1;
            voxelPaletteWrite.pImageInfo = &inputVoxelPaletteInfo;
        VkWriteDescriptorSet stepCount = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            stepCount.dstSet = raytraceDescriptorSets[i];
            stepCount.dstBinding = 5;
            stepCount.dstArrayElement = 0;
            stepCount.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            stepCount.descriptorCount = 1;
            stepCount.pImageInfo = &stepCountInfo;
        VkWriteDescriptorSet outNewRaytracedWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            outNewRaytracedWrite.dstSet = raytraceDescriptorSets[i];
            outNewRaytracedWrite.dstBinding = 6;
            outNewRaytracedWrite.dstArrayElement = 0;
            outNewRaytracedWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            outNewRaytracedWrite.descriptorCount = 1;
            outNewRaytracedWrite.pImageInfo = &outputFrameInfo;
            
        vector<VkWriteDescriptorSet> descriptorWrites = {matNormWrite, depthWrite, blockWrite, blockPaletteWrite, voxelPaletteWrite, stepCount, outNewRaytracedWrite};
        vkUpdateDescriptorSets(device, descriptorWrites.size(), descriptorWrites.data(), 0, NULL);
    }
}

void Renderer::setup_Radiance_Cache_Descriptors(){
    for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        int previous_frame = i - 1;
        // int previous_frame = i;
        if (previous_frame < 0) previous_frame = MAX_FRAMES_IN_FLIGHT-1; // so frames do not intersect

        VkDescriptorImageInfo blocksInfo = {};
            blocksInfo.imageView = world.view;
            blocksInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        VkDescriptorImageInfo blockPaletteInfo = {};
            blockPaletteInfo.imageView = origin_block_palette[i].view;
            blockPaletteInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        VkDescriptorImageInfo voxelPaletteInfo = {};
            voxelPaletteInfo.imageView = material_palette[i].view;
            voxelPaletteInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        VkDescriptorImageInfo radianceCacheInfo = {};
            radianceCacheInfo.imageView = radiance_cache.view;
            radianceCacheInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            
        VkWriteDescriptorSet blocksWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            blocksWrite.dstSet = updateRadianceDescriptorSets[i];
            blocksWrite.dstBinding = 0;
            blocksWrite.dstArrayElement = 0;
            blocksWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            blocksWrite.descriptorCount = 1;
            blocksWrite.pImageInfo = &blocksInfo;
        VkWriteDescriptorSet blockPaletteWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            blockPaletteWrite.dstSet = updateRadianceDescriptorSets[i];
            blockPaletteWrite.dstBinding = 1;
            blockPaletteWrite.dstArrayElement = 0;
            blockPaletteWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            blockPaletteWrite.descriptorCount = 1;
            blockPaletteWrite.pImageInfo = &blockPaletteInfo;
        VkWriteDescriptorSet voxelPaletteWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            voxelPaletteWrite.dstSet = updateRadianceDescriptorSets[i];
            voxelPaletteWrite.dstBinding = 2;
            voxelPaletteWrite.dstArrayElement = 0;
            voxelPaletteWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            voxelPaletteWrite.descriptorCount = 1;
            voxelPaletteWrite.pImageInfo = &voxelPaletteInfo;
        VkWriteDescriptorSet radianceCacheWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            radianceCacheWrite.dstSet = updateRadianceDescriptorSets[i];
            radianceCacheWrite.dstBinding = 3;
            radianceCacheWrite.dstArrayElement = 0;
            radianceCacheWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            radianceCacheWrite.descriptorCount = 1;
            radianceCacheWrite.pImageInfo = &radianceCacheInfo;
            
        vector<VkWriteDescriptorSet> descriptorWrites = {blocksWrite, blockPaletteWrite, voxelPaletteWrite, radianceCacheWrite};
        vkUpdateDescriptorSets(device, descriptorWrites.size(), descriptorWrites.data(), 0, NULL);
    }
}

void Renderer::setup_Do_Light_Descriptors(){
    for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        int previous_frame = i - 1;
        // int previous_frame = i;
        if (previous_frame < 0) previous_frame = MAX_FRAMES_IN_FLIGHT-1; // so frames do not intersect

        VkDescriptorImageInfo matNormInfo = {};
            matNormInfo.imageView = is_scaled? matNorm_lowres[i].view : matNorm_highres[i].view;
            matNormInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        VkDescriptorImageInfo depthInfo = {};
            depthInfo.imageView = is_scaled? depthBuffers_lowres[i].view : depthBuffers_highres[i].view;
            depthInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
            depthInfo.sampler = nearestSampler;
        VkDescriptorImageInfo inputBlockInfo = {};
            inputBlockInfo.imageView = world.view;
            inputBlockInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        VkDescriptorImageInfo inputBlockPaletteInfo = {};
            inputBlockPaletteInfo.imageView = origin_block_palette[i].view;
            inputBlockPaletteInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        VkDescriptorImageInfo inputVoxelPaletteInfo = {};
            inputVoxelPaletteInfo.imageView = material_palette[i].view;
            inputVoxelPaletteInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        VkDescriptorImageInfo outputFrameInfo = {};
            outputFrameInfo.imageView = is_scaled? lowres_frames[i].view : highres_frames[i].view;
            outputFrameInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        VkDescriptorImageInfo stepCountInfo = {};
            stepCountInfo.imageView = step_count.view;
            stepCountInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        VkDescriptorImageInfo radianceCacheInfo = {};
            radianceCacheInfo.imageView = radiance_cache.view;
            radianceCacheInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

        VkWriteDescriptorSet matNormWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            matNormWrite.dstSet = doLightDescriptorSets[i];
            matNormWrite.dstBinding = 0;
            matNormWrite.dstArrayElement = 0;
            matNormWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            matNormWrite.descriptorCount = 1;
            matNormWrite.pImageInfo = &matNormInfo;
        VkWriteDescriptorSet depthWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            depthWrite.dstSet = doLightDescriptorSets[i];
            depthWrite.dstBinding = 1;
            depthWrite.dstArrayElement = 0;
            depthWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            depthWrite.descriptorCount = 1;
            depthWrite.pImageInfo = &depthInfo;
        VkWriteDescriptorSet blockWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            blockWrite.dstSet = doLightDescriptorSets[i];
            blockWrite.dstBinding = 2;
            blockWrite.dstArrayElement = 0;
            blockWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            blockWrite.descriptorCount = 1;
            blockWrite.pImageInfo = &inputBlockInfo;
        VkWriteDescriptorSet blockPaletteWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            blockPaletteWrite.dstSet = doLightDescriptorSets[i];
            blockPaletteWrite.dstBinding = 3;
            blockPaletteWrite.dstArrayElement = 0;
            blockPaletteWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            blockPaletteWrite.descriptorCount = 1;
            blockPaletteWrite.pImageInfo = &inputBlockPaletteInfo;
        VkWriteDescriptorSet voxelPaletteWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            voxelPaletteWrite.dstSet = doLightDescriptorSets[i];
            voxelPaletteWrite.dstBinding = 4;
            voxelPaletteWrite.dstArrayElement = 0;
            voxelPaletteWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            voxelPaletteWrite.descriptorCount = 1;
            voxelPaletteWrite.pImageInfo = &inputVoxelPaletteInfo;
        VkWriteDescriptorSet stepCount = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            stepCount.dstSet = doLightDescriptorSets[i];
            stepCount.dstBinding = 5;
            stepCount.dstArrayElement = 0;
            stepCount.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            stepCount.descriptorCount = 1;
            stepCount.pImageInfo = &stepCountInfo;
        VkWriteDescriptorSet outNewRaytracedWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            outNewRaytracedWrite.dstSet = doLightDescriptorSets[i];
            outNewRaytracedWrite.dstBinding = 6;
            outNewRaytracedWrite.dstArrayElement = 0;
            outNewRaytracedWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            outNewRaytracedWrite.descriptorCount = 1;
            outNewRaytracedWrite.pImageInfo = &outputFrameInfo;
        VkWriteDescriptorSet radianceCacheWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            radianceCacheWrite.dstSet = doLightDescriptorSets[i];
            radianceCacheWrite.dstBinding = 7;
            radianceCacheWrite.dstArrayElement = 0;
            radianceCacheWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            radianceCacheWrite.descriptorCount = 1;
            radianceCacheWrite.pImageInfo = &radianceCacheInfo;
                
        vector<VkWriteDescriptorSet> descriptorWrites = {matNormWrite, depthWrite, blockWrite, blockPaletteWrite, voxelPaletteWrite, stepCount, outNewRaytracedWrite, radianceCacheWrite};
        vkUpdateDescriptorSets(device, descriptorWrites.size(), descriptorWrites.data(), 0, NULL);
    }
}

void Renderer::setup_Denoise_Descriptors(){
    for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        //setting up higres descriptors
        VkDescriptorImageInfo depthInfo = {};
            depthInfo.imageView = is_scaled? depthBuffers_highres[0].view : depthBuffers_highres[i].view;
            depthInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
            depthInfo.sampler = nearestSampler;
        VkDescriptorImageInfo matNormInfo = {};
            matNormInfo.imageView = is_scaled? matNorm_highres[0].view : matNorm_highres[i].view;
            matNormInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        VkDescriptorImageInfo readFrameInfo = {};
            readFrameInfo.imageView = is_scaled? highres_frames[0].view : highres_frames[i].view;
            readFrameInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        VkDescriptorImageInfo outputFrameInfo = {};
            outputFrameInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            outputFrameInfo.imageView = is_scaled? highres_frames[0].view : highres_frames[i].view;
        VkDescriptorImageInfo mixratioInfo = {};
            mixratioInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            mixratioInfo.imageView = mix_ratio[currentFrame].view;
            mixratioInfo.sampler = nearestSampler;
        VkDescriptorImageInfo matPaletteInfo = {};
            matPaletteInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            matPaletteInfo.imageView = material_palette[i].view;

        VkWriteDescriptorSet matNormWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            matNormWrite.dstSet = denoiseDescriptorSets_highres[i];
            matNormWrite.dstBinding = 0;
            matNormWrite.dstArrayElement = 0;
            matNormWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            matNormWrite.descriptorCount = 1;
            matNormWrite.pImageInfo = &matNormInfo;
        VkWriteDescriptorSet depthWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            depthWrite.dstSet = denoiseDescriptorSets_highres[i];
            depthWrite.dstBinding = 1;
            depthWrite.dstArrayElement = 0;
            depthWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            depthWrite.descriptorCount = 1;
            depthWrite.pImageInfo = &depthInfo;
        VkWriteDescriptorSet raytracedFrameWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
           raytracedFrameWrite.dstSet = denoiseDescriptorSets_highres[i];
           raytracedFrameWrite.dstBinding = 2;
           raytracedFrameWrite.dstArrayElement = 0;
           raytracedFrameWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
           raytracedFrameWrite.descriptorCount = 1;
           raytracedFrameWrite.pImageInfo = &readFrameInfo;
        VkWriteDescriptorSet denoisedFrameWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            denoisedFrameWrite.dstSet = denoiseDescriptorSets_highres[i];
            denoisedFrameWrite.dstBinding = 3;
            denoisedFrameWrite.dstArrayElement = 0;
            denoisedFrameWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            denoisedFrameWrite.descriptorCount = 1;
            denoisedFrameWrite.pImageInfo = &outputFrameInfo;
        VkWriteDescriptorSet mixRatioWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            mixRatioWrite.dstSet = denoiseDescriptorSets_highres[i];
            mixRatioWrite.dstBinding = 4;
            mixRatioWrite.dstArrayElement = 0;
            mixRatioWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            mixRatioWrite.descriptorCount = 1;
            mixRatioWrite.pImageInfo = &mixratioInfo;
        VkWriteDescriptorSet matPaletteWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            matPaletteWrite.dstSet = denoiseDescriptorSets_highres[i];
            matPaletteWrite.dstBinding = 5;
            matPaletteWrite.dstArrayElement = 0;
            matPaletteWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            matPaletteWrite.descriptorCount = 1;
            matPaletteWrite.pImageInfo = &matPaletteInfo;

        vector<VkWriteDescriptorSet> descriptorWrites = {matNormWrite, depthWrite, raytracedFrameWrite, denoisedFrameWrite, mixRatioWrite, matPaletteWrite/*blockPaletteSampler*/};
        vkUpdateDescriptorSets(device, descriptorWrites.size(), descriptorWrites.data(), 0, NULL);

        if(is_scaled){
            matNormInfo.imageView = matNorm_lowres[i].view;
            depthInfo.imageView = depthBuffers_lowres[i].view;
            readFrameInfo.imageView   = lowres_frames[i].view;
            outputFrameInfo.imageView = lowres_frames[i].view;

            matNormWrite.dstSet = denoiseDescriptorSets_lowres[i];
            depthWrite.dstSet = denoiseDescriptorSets_lowres[i];
            raytracedFrameWrite.dstSet = denoiseDescriptorSets_lowres[i];
            denoisedFrameWrite.dstSet = denoiseDescriptorSets_lowres[i];
            mixRatioWrite.dstSet = denoiseDescriptorSets_lowres[i];
            matPaletteWrite.dstSet = denoiseDescriptorSets_lowres[i];
            descriptorWrites = {matNormWrite, depthWrite, raytracedFrameWrite, denoisedFrameWrite, mixRatioWrite, matPaletteWrite/*blockPaletteSampler*/};
            vkUpdateDescriptorSets(device, descriptorWrites.size(), descriptorWrites.data(), 0, NULL);
        }
    }
}
void Renderer::setup_Accumulate_Descriptors(){
    for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        int previous_frame = i - 1;
        if (previous_frame < 0) previous_frame = MAX_FRAMES_IN_FLIGHT-1; // so frames do not intersect

        VkDescriptorImageInfo newFrameInfo = {};
            newFrameInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        VkDescriptorImageInfo oldFrameInfo = {};
            oldFrameInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        VkDescriptorImageInfo newMatNormInfo = {};
            newMatNormInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        VkDescriptorImageInfo oldMatNormInfo = {};
            oldMatNormInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        VkDescriptorImageInfo uvShiftInfo = {};
            uvShiftInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        VkDescriptorImageInfo newMixRatioInfo = {};
            newMixRatioInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        VkDescriptorImageInfo oldMixRatioInfo = {};
            oldMixRatioInfo.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
            oldMixRatioInfo.sampler = nearestSampler;
        VkDescriptorImageInfo oldDepthInfo = {};
            oldDepthInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
            oldDepthInfo.sampler = nearestSampler;
        VkDescriptorImageInfo newDepthInfo = {};
            newDepthInfo.imageLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL;
            newDepthInfo.sampler = nearestSampler;

        if(is_scaled){
             newMatNormInfo.imageView = matNorm_lowres[i].view;
             oldMatNormInfo.imageView = matNorm_lowres[previous_frame].view;
                uvShiftInfo.imageView = oldUv_lowres.view;
            newMixRatioInfo.imageView = mix_ratio[i].view;
            oldMixRatioInfo.imageView = mix_ratio[previous_frame].view;
               oldDepthInfo.imageView = depthBuffers_lowres[i].view;
               newDepthInfo.imageView = depthBuffers_lowres[previous_frame].view;
               newFrameInfo.imageView = lowres_frames[i].view;
               oldFrameInfo.imageView = lowres_frames[previous_frame].view;
        } else {
             newMatNormInfo.imageView = matNorm_highres[i].view;
             oldMatNormInfo.imageView = matNorm_highres[previous_frame].view;
                uvShiftInfo.imageView = oldUv_highres.view;
            newMixRatioInfo.imageView = mix_ratio[i].view;
            oldMixRatioInfo.imageView = mix_ratio[previous_frame].view;
               oldDepthInfo.imageView = depthBuffers_highres[i].view;
               newDepthInfo.imageView = depthBuffers_highres[previous_frame].view;
               newFrameInfo.imageView = highres_frames[i].view;
               oldFrameInfo.imageView = highres_frames[previous_frame].view;
        }
            
        VkDescriptorImageInfo matPaletteInfo = {};
            matPaletteInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
            matPaletteInfo.imageView = material_palette[i].view;
        

        VkWriteDescriptorSet newFrameWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            newFrameWrite.dstSet = accumulateDescriptorSets[i];
            newFrameWrite.dstBinding = 0;
            newFrameWrite.dstArrayElement = 0;
            newFrameWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            newFrameWrite.descriptorCount = 1;
            newFrameWrite.pImageInfo = &newFrameInfo;
        VkWriteDescriptorSet oldFrameWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            oldFrameWrite.dstSet = accumulateDescriptorSets[i];
            oldFrameWrite.dstBinding = 1;
            oldFrameWrite.dstArrayElement = 0;
            oldFrameWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            oldFrameWrite.descriptorCount = 1;
            oldFrameWrite.pImageInfo = &oldFrameInfo;
        VkWriteDescriptorSet newMatNormWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
           newMatNormWrite.dstSet = accumulateDescriptorSets[i];
           newMatNormWrite.dstBinding = 2;
           newMatNormWrite.dstArrayElement = 0;
           newMatNormWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
           newMatNormWrite.descriptorCount = 1;
           newMatNormWrite.pImageInfo = &newMatNormInfo;
        VkWriteDescriptorSet oldMatNormWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            oldMatNormWrite.dstSet = accumulateDescriptorSets[i];
            oldMatNormWrite.dstBinding = 3;
            oldMatNormWrite.dstArrayElement = 0;
            oldMatNormWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            oldMatNormWrite.descriptorCount = 1;
            oldMatNormWrite.pImageInfo = &oldMatNormInfo;
        VkWriteDescriptorSet uvShiftWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            uvShiftWrite.dstSet = accumulateDescriptorSets[i];
            uvShiftWrite.dstBinding = 4;
            uvShiftWrite.dstArrayElement = 0;
            uvShiftWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            uvShiftWrite.descriptorCount = 1;
            uvShiftWrite.pImageInfo = &uvShiftInfo;
        VkWriteDescriptorSet newMixRatioWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            newMixRatioWrite.dstSet = accumulateDescriptorSets[i];
            newMixRatioWrite.dstBinding = 5;
            newMixRatioWrite.dstArrayElement = 0;
            newMixRatioWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            newMixRatioWrite.descriptorCount = 1;
            newMixRatioWrite.pImageInfo = &newMixRatioInfo;
        VkWriteDescriptorSet oldMixRatioWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            oldMixRatioWrite.dstSet = accumulateDescriptorSets[i];
            oldMixRatioWrite.dstBinding = 6;
            oldMixRatioWrite.dstArrayElement = 0;
            oldMixRatioWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            oldMixRatioWrite.descriptorCount = 1;
            oldMixRatioWrite.pImageInfo = &oldMixRatioInfo;
        VkWriteDescriptorSet oldDepthWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            oldDepthWrite.dstSet = accumulateDescriptorSets[i];
            oldDepthWrite.dstBinding = 7;
            oldDepthWrite.dstArrayElement = 0;
            oldDepthWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            oldDepthWrite.descriptorCount = 1;
            oldDepthWrite.pImageInfo = &oldDepthInfo;
        VkWriteDescriptorSet newDepthWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            newDepthWrite.dstSet = accumulateDescriptorSets[i];
            newDepthWrite.dstBinding = 8;
            newDepthWrite.dstArrayElement = 0;
            newDepthWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            newDepthWrite.descriptorCount = 1;
            newDepthWrite.pImageInfo = &newDepthInfo;
        VkWriteDescriptorSet matPaletteWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            matPaletteWrite.dstSet = accumulateDescriptorSets[i];
            matPaletteWrite.dstBinding = 9;
            matPaletteWrite.dstArrayElement = 0;
            matPaletteWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            matPaletteWrite.descriptorCount = 1;
            matPaletteWrite.pImageInfo = &matPaletteInfo;
        
        vector<VkWriteDescriptorSet> descriptorWrites = {newFrameWrite, oldFrameWrite, newMatNormWrite, oldMatNormWrite, uvShiftWrite, newMixRatioWrite, oldMixRatioWrite, oldDepthWrite, newDepthWrite, matPaletteWrite};
        vkUpdateDescriptorSets(device, descriptorWrites.size(), descriptorWrites.data(), 0, NULL);
    }
}
void Renderer::setup_Upscale_Descriptors(){
    for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorImageInfo lowres_matNormInfo = {};
            lowres_matNormInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        VkDescriptorImageInfo highres_matNormInfo = {};
            highres_matNormInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        VkDescriptorImageInfo denoisedFrameInfo = {};
            denoisedFrameInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        VkDescriptorImageInfo upscaledFrameInfo = {};
            upscaledFrameInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        
        if(is_scaled){
             lowres_matNormInfo.imageView = matNorm_lowres[i].view;
            highres_matNormInfo.imageView = matNorm_highres[0].view; //will be NULL_HANDLE if not scaled, but this function will not be called anyways
              denoisedFrameInfo.imageView = lowres_frames[i].view;
              upscaledFrameInfo.imageView = highres_frames[0].view;
        } else {
            crash(wtf is upscaler doing then?);
        }
            
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
            
        vector<VkWriteDescriptorSet> descriptorWrites = {lowres_matNormWrite, highres_matNormWrite, denoisedFrameWrite, upscaledFrameWrite};
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
        samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_BLACK;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;
        samplerInfo.compareEnable = VK_FALSE;
        samplerInfo.compareOp = VK_COMPARE_OP_LESS_OR_EQUAL;
        samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
        samplerInfo.mipLodBias = 0.0f;
        samplerInfo.minLod = 0.0f;
        samplerInfo.maxLod = 0.0f;
    
    VK_CHECK(vkCreateSampler(device, &samplerInfo, NULL, &nearestSampler));
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    VK_CHECK(vkCreateSampler(device, &samplerInfo, NULL, &linearSampler));

        samplerInfo.magFilter = VK_FILTER_NEAREST;
        samplerInfo.minFilter = VK_FILTER_NEAREST;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    VK_CHECK(vkCreateSampler(device, &samplerInfo, NULL, &uiSampler));
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
void Renderer::setup_RayGen_Particles_Descriptors(){
    for (u32 i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        VkDescriptorBufferInfo transInfo = {};
            transInfo.offset = 0;
            transInfo.buffer = uniform[i].buffer;
            transInfo.range = VK_WHOLE_SIZE;
        VkDescriptorImageInfo inputBlockInfo = {};
            inputBlockInfo.imageView = world.view;
            inputBlockInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        VkDescriptorImageInfo inputBlockPaletteInfo = {};
            inputBlockPaletteInfo.imageView = origin_block_palette[i].view;
            inputBlockPaletteInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        
        VkWriteDescriptorSet uniformWrite = {};
            uniformWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            uniformWrite.dstSet = RayGenParticlesDescriptorSets[i];
            uniformWrite.dstBinding = 0;
            uniformWrite.dstArrayElement = 0;
            uniformWrite.descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            uniformWrite.descriptorCount = 1;
            uniformWrite.pBufferInfo = &transInfo;
        VkWriteDescriptorSet blockWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            blockWrite.dstSet = RayGenParticlesDescriptorSets[i];
            blockWrite.dstBinding = 1;
            blockWrite.dstArrayElement = 0;
            blockWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            blockWrite.descriptorCount = 1;
            blockWrite.pImageInfo = &inputBlockInfo;
        VkWriteDescriptorSet blockPaletteWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            blockPaletteWrite.dstSet = RayGenParticlesDescriptorSets[i];
            blockPaletteWrite.dstBinding = 2;
            blockPaletteWrite.dstArrayElement = 0;
            blockPaletteWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            blockPaletteWrite.descriptorCount = 1;
            blockPaletteWrite.pImageInfo = &inputBlockPaletteInfo;

        vector<VkWriteDescriptorSet> descriptorWrites = {uniformWrite, blockWrite, blockPaletteWrite};

        vkUpdateDescriptorSets(device, descriptorWrites.size(), descriptorWrites.data(), 0, NULL);
    }
}

vector<char> read_Shader(const char* filename);

void Renderer::create_compute_pipelines_helper(const char* name, VkDescriptorSetLayout  descriptorSetLayout, VkPipelineLayout* pipelineLayout, VkPipeline* pipeline, u32 push_size, u32 flags){
    auto compShaderCode = read_Shader(name);

    VkShaderModule module = create_Shader_Module(&compShaderCode);
    
    VkSpecializationInfo specConstants = {};
    // specConstants.
    //single stage
    VkPipelineShaderStageCreateInfo compShaderStageInfo = {};
        compShaderStageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        compShaderStageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        compShaderStageInfo.module = module;
        compShaderStageInfo.pName = "main";
        // compShaderStageInfo.pSpecializationInfo = 

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
        pipelineInfo.flags = flags;

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
    create_compute_pipelines_helper("shaders/compiled/radiance.spv"    , updateRadianceDescriptorSetLayout, &radianceLayout, &radiancePipeline, sizeof(int), VK_PIPELINE_CREATE_DISPATCH_BASE_BIT);
    create_compute_pipelines_helper("shaders/compiled/dolight.spv"    , doLightDescriptorSetLayout, &doLightLayout, &doLightPipeline, sizeof(ivec4) + sizeof(vec4)*4);

    create_compute_pipelines_helper("shaders/compiled/denoise.spv" ,  denoiseDescriptorSetLayout,  &denoiseLayout,  &denoisePipeline, sizeof(int)*2);
    create_compute_pipelines_helper("shaders/compiled/accumulate.spv" ,  accumulateDescriptorSetLayout,  &accumulateLayout,  &accumulatePipeline, 0);
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

// #undef copy_Buffer
void Renderer::copy_Buffer(VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
    VkCommandBuffer commandBuffer= begin_Single_Time_Commands();

    VkBufferCopy copyRegion{};
    copyRegion.size = size;
    vkCmdCopyBuffer(commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);

    end_Single_Time_Commands(commandBuffer);
}

void Renderer::copy_Buffer(VkBuffer srcBuffer, Image* image, uvec3 size) {
    VkCommandBuffer commandBuffer= begin_Single_Time_Commands();

    VkImageLayout initial = (*image).layout;
    assert(initial != VK_IMAGE_LAYOUT_UNDEFINED);

    cmdTransLayoutBarrier(commandBuffer, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 
        VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT, image);
    VkBufferImageCopy copyRegion = {};
    // copyRegion.size = size;
        copyRegion.imageExtent.width  = size.x;
        copyRegion.imageExtent.height = size.y;
        copyRegion.imageExtent.depth  = size.z;
        copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copyRegion.imageSubresource.layerCount = 1;
    vkCmdCopyBufferToImage(commandBuffer, srcBuffer, (*image).image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copyRegion);
    cmdTransLayoutBarrier(commandBuffer, initial, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 
        VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT, image);
        
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

void Renderer::transition_Image_Layout_Singletime(Image* image, VkImageLayout newLayout) {
    VkCommandBuffer commandBuffer = begin_Single_Time_Commands();

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = (*image).layout;
    barrier.newLayout = newLayout;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = (*image).image;
    barrier.subresourceRange.aspectMask = (*image).aspect;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;

    barrier.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT;

    vkCmdPipelineBarrier(
        commandBuffer,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        0,
        0, NULL,
        0, NULL,
        1, &barrier
    );
    (*image).layout = newLayout;

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

//TODO: need to find memory, verify flags
// tuple<vector<Buffer>, vector<Buffer>> Renderer::create_RayGen_VertexBuffers(vector<Vertex> vertices, vector<u32> indices){
//     return create_RayGen_VertexBuffers(vertices.data(), vertices.size(), indices.data(), indices.size());
// }

// tuple<vector<Buffer>, vector<Buffer>> Renderer::create_RayGen_VertexBuffers(Vertex* vertices, u32 vcount, u32* indices, u32 icount){
//     VkDeviceSize bufferSizeV = sizeof(Vertex)*vcount;
//     // cout << bufferSizeV;
//     VkDeviceSize bufferSizeI = sizeof(u32   )*icount;

//     vector<Buffer> vertexes (MAX_FRAMES_IN_FLIGHT);
//     vector<Buffer>  indexes (MAX_FRAMES_IN_FLIGHT);

//     VkBufferCreateInfo stagingBufferInfo = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
//         stagingBufferInfo.size = bufferSizeV;
//         stagingBufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
//     VmaAllocationCreateInfo stagingAllocInfo = {};
//         stagingAllocInfo.usage = VMA_MEMORY_USAGE_AUTO;
//         stagingAllocInfo.flags = VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT;
//         stagingAllocInfo.requiredFlags = VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
//     VkBuffer stagingBufferV = {};
//     VmaAllocation stagingAllocationV = {};
//     VkBuffer stagingBufferI = {};
//     VmaAllocation stagingAllocationI = {};
//     vmaCreateBuffer(VMAllocator, &stagingBufferInfo, &stagingAllocInfo, &stagingBufferV, &stagingAllocationV, NULL);
//     stagingBufferInfo.size = bufferSizeI;
//     vmaCreateBuffer(VMAllocator, &stagingBufferInfo, &stagingAllocInfo, &stagingBufferI, &stagingAllocationI, NULL);

//     void* data;
// assert (VMAllocator);
// assert (stagingAllocationV);
// assert (&data);
//     vmaMapMemory(VMAllocator, stagingAllocationV, &data);
//         memcpy(data, vertices, bufferSizeV);
//     vmaUnmapMemory(VMAllocator, stagingAllocationV);

//     vmaMapMemory(VMAllocator, stagingAllocationI, &data);
//         memcpy(data, indices, bufferSizeI);
//     vmaUnmapMemory(VMAllocator, stagingAllocationI);

//     for(i32 i=0; i<MAX_FRAMES_IN_FLIGHT; i++){
//         VkBufferCreateInfo bufferInfo = {VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO};
//             bufferInfo.size = bufferSizeV;
//             bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
//         VmaAllocationCreateInfo allocInfo = {};
//             allocInfo.usage = VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE;
//         vmaCreateBuffer(VMAllocator, &bufferInfo, &allocInfo, &vertexes[i].buffer, &vertexes[i].alloc, NULL);
//             bufferInfo.size = bufferSizeI;
//             bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
//         vmaCreateBuffer(VMAllocator, &bufferInfo, &allocInfo, &indexes[i].buffer, &indexes[i].alloc, NULL);
               
//         copy_Buffer(stagingBufferV, vertexes[i].buffer, bufferSizeV);
//         copy_Buffer(stagingBufferI,  indexes[i].buffer, bufferSizeI);
//     }

//     vmaDestroyBuffer(VMAllocator, stagingBufferV, stagingAllocationV);
//     vmaDestroyBuffer(VMAllocator, stagingBufferI, stagingAllocationI);

//     return {vertexes, indexes};
// }