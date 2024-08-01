#define VMA_IMPLEMENTATION
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#include "render.hpp" 

using namespace std;
using namespace glm;



tuple<int, int> get_block_xy(int N);

void Renderer::init(int xSize, int ySize, int zSize, int staticBlockPaletteSize, int maxParticleCount, float ratio, bool vsync, bool fullscreen) {
    this->world_size = ivec3(xSize, ySize, zSize);
     origin_world.allocate(world_size);
    current_world.allocate(world_size);
    this->_ratio = ratio;
    // fratio = _rat
    this->maxParticleCount = maxParticleCount;
    this->scaled = (ratio != float(1));
    this->vsync = vsync;
    this->fullscreen = fullscreen;
    this->static_block_palette_size = staticBlockPaletteSize;

    ui_render_interface = new(MyRenderInterface);
    ui_render_interface->render = this;

    createWindow();
    VK_CHECK(volkInitialize());
    get_Instance_Extensions();
    createInstance();
    volkLoadInstance(instance);   

    #ifndef VKNDEBUG
        setupDebug_Messenger();
    #endif

    createSurface();
    pickPhysicalDevice();
    createLogicalDevice();

    createAllocator();

    createSwapchain(); //should be before anything related to its size and format
    createSwapchainImageViews();

    // createRenderPassGraphical();
    createRenderPass1();
    createRenderPass2();
println

    createSamplers();

    printl(VK_FORMAT_UNDEFINED)
    printl(swapChainImageFormat)
    
    create_Command_Pool();
    create_Command_Buffers(&overlayCommandBuffers, MAX_FRAMES_IN_FLIGHT);
    create_Command_Buffers(&   rayGenCommandBuffers, MAX_FRAMES_IN_FLIGHT);
    create_Command_Buffers(&  computeCommandBuffers, MAX_FRAMES_IN_FLIGHT);

    VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = MAX_FRAMES_IN_FLIGHT;
    copyOverlayCommandBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    VK_CHECK(vkAllocateCommandBuffers(device, &allocInfo, copyOverlayCommandBuffers.data())); 
    // renderOverlayCommandBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    // VK_CHECK(vkAllocateCommandBuffers(device, &allocInfo, renderOverlayCommandBuffers.data())); 
    
    createSwapchainDependent();
println

    create_Buffer_Storages(&stagingWorld,
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
    create_Image_Storages(&radianceCache,
        VK_IMAGE_TYPE_3D,
        VK_FORMAT_R16G16B16A16_UNORM,
        VK_IMAGE_USAGE_STORAGE_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT,
        {RCACHE_RAYS_PER_PROBE * world_size.x, world_size.y, world_size.z}); //TODO: dynamic
    create_Image_Storages(&originBlockPalette,
        VK_IMAGE_TYPE_3D,
        VK_FORMAT_R8_UINT,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT,
        // {16*BLOCK_PALETTE_SIZE_X, 16*BLOCK_PALETTE_SIZE_Y, 32}); //TODO: dynamic
        {16*BLOCK_PALETTE_SIZE_X, 16*BLOCK_PALETTE_SIZE_Y, 16}); //TODO: dynamic
    create_Image_Storages(&materialPalette,
        VK_IMAGE_TYPE_2D,
        VK_FORMAT_R32_SFLOAT, //try R32G32
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT,
        {6, 256, 1}); //TODO: dynamic, text formats, pack Material into smth other than 6 floats :)

    create_Buffer_Storages(&gpuParticles,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        maxParticleCount*sizeof(Particle), true);
    // create_Buffer_Storages(&staging_uniform,
    //     VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
    //     sizeof(mat4)*2, true);
    create_Buffer_Storages(&uniform,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        sizeof(mat4)*2, false);
    // staging_uniform_mapped.resize(MAX_FRAMES_IN_FLIGHT);
    stagingWorldMapped.resize(MAX_FRAMES_IN_FLIGHT);
    gpuParticlesMapped.resize(MAX_FRAMES_IN_FLIGHT);
    for (int i=0; i<MAX_FRAMES_IN_FLIGHT; i++) {
        vmaMapMemory(VMAllocator, gpuParticles[i].alloc, &gpuParticlesMapped[i]);
        vmaMapMemory(VMAllocator, stagingWorld[i].alloc, &stagingWorldMapped[i]);
    }
    transition_Image_Layout_Singletime(&world, VK_IMAGE_LAYOUT_GENERAL);
    transition_Image_Layout_Singletime(&radianceCache, VK_IMAGE_LAYOUT_GENERAL);
    for (int i=0; i<MAX_FRAMES_IN_FLIGHT; i++) {
        transition_Image_Layout_Singletime(&originBlockPalette[i], VK_IMAGE_LAYOUT_GENERAL);
        transition_Image_Layout_Singletime(&originBlockPalette[i], VK_IMAGE_LAYOUT_GENERAL);
        transition_Image_Layout_Singletime(&materialPalette[i], VK_IMAGE_LAYOUT_GENERAL);
    }
    //this have to sets allocated
    create_DescriptorSetLayout({
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, // modelVoxels
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, // blocks
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, // blockPalette
        }, 
        VK_SHADER_STAGE_COMPUTE_BIT, &mapPipe.setLayout, 
        VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR);
    create_DescriptorSetLayout({
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, //in texture (ui)
        }, 
        VK_SHADER_STAGE_FRAGMENT_BIT, &overlayPipe.setLayout,
        VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR);

    // deferDescriptorsetup(&raytracePipe.setLayout, &raytracePipe.sets, {
    //     {RD_CURRENT, {/*empty*/}, (matNormHighres),NO_SAMPLER, VK_IMAGE_LAYOUT_GENERAL},
    //     {RD_CURRENT, {/*empty*/}, (depthHighres), nearestSampler, VK_IMAGE_LAYOUT_GENERAL},
    //     {RD_FIRST  , {/*empty*/}, {world},                                      NO_SAMPLER, VK_IMAGE_LAYOUT_GENERAL},
    //     {RD_CURRENT, {/*empty*/}, (originBlockPalette),                       NO_SAMPLER, VK_IMAGE_LAYOUT_GENERAL},
    //     {RD_CURRENT, {/*empty*/}, (materialPalette),                           NO_SAMPLER, VK_IMAGE_LAYOUT_GENERAL},
    //     {RD_CURRENT, {/*empty*/}, (highresFrames),  NO_SAMPLER, VK_IMAGE_LAYOUT_GENERAL},
    // }, VK_SHADER_STAGE_COMPUTE_BIT);        
println
    deferDescriptorsetup(&radiancePipe.setLayout, &radiancePipe.sets, {
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, RD_FIRST  , {/*empty*/}, {world},              NO_SAMPLER, VK_IMAGE_LAYOUT_GENERAL},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, RD_CURRENT, {/*empty*/}, (originBlockPalette), NO_SAMPLER, VK_IMAGE_LAYOUT_GENERAL},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, RD_CURRENT, {/*empty*/}, (materialPalette),    NO_SAMPLER, VK_IMAGE_LAYOUT_GENERAL},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, RD_FIRST  , {/*empty*/}, {radianceCache},      NO_SAMPLER, VK_IMAGE_LAYOUT_GENERAL},
    }, VK_SHADER_STAGE_COMPUTE_BIT);
println
    deferDescriptorsetup(&diffusePipe.setLayout, &diffusePipe.sets, {
        {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, RD_CURRENT, {/*empty*/}, (matNormHighres), NO_SAMPLER,     VK_IMAGE_LAYOUT_GENERAL},
        {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, RD_CURRENT, {/*empty*/}, (depthHighres),   NO_SAMPLER, VK_IMAGE_LAYOUT_GENERAL},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE         , RD_FIRST  , {/*empty*/}, {world},              NO_SAMPLER,     VK_IMAGE_LAYOUT_GENERAL},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE         , RD_CURRENT, {/*empty*/}, (originBlockPalette), NO_SAMPLER,     VK_IMAGE_LAYOUT_GENERAL},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE         , RD_CURRENT, {/*empty*/}, (materialPalette),    NO_SAMPLER,     VK_IMAGE_LAYOUT_GENERAL},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE         , RD_FIRST  , {/*empty*/}, {radianceCache},      NO_SAMPLER,     VK_IMAGE_LAYOUT_GENERAL},
    }, VK_SHADER_STAGE_FRAGMENT_BIT);
println
    deferDescriptorsetup(&glossyPipe.setLayout, &glossyPipe.sets, { 
        {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, RD_CURRENT, {/*empty*/}, (matNormHighres), NO_SAMPLER,     VK_IMAGE_LAYOUT_GENERAL},
        // {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, RD_CURRENT, {/*empty*/}, (highresFrames),  NO_SAMPLER, VK_IMAGE_LAYOUT_GENERAL},
        {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, RD_CURRENT, {/*empty*/}, (depthHighres),   NO_SAMPLER, VK_IMAGE_LAYOUT_GENERAL},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, RD_FIRST  , {/*empty*/}, {world},              NO_SAMPLER,     VK_IMAGE_LAYOUT_GENERAL},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, RD_CURRENT, {/*empty*/}, (originBlockPalette), NO_SAMPLER,     VK_IMAGE_LAYOUT_GENERAL},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, RD_CURRENT, {/*empty*/}, (materialPalette),    NO_SAMPLER,     VK_IMAGE_LAYOUT_GENERAL},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, RD_FIRST  , {/*empty*/}, {radianceCache},      NO_SAMPLER,     VK_IMAGE_LAYOUT_GENERAL},
    }, VK_SHADER_STAGE_FRAGMENT_BIT);
println
    deferDescriptorsetup(&blurPipe.setLayout, &blurPipe.sets, { 
        {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, RD_CURRENT, {/*empty*/}, (matNormHighres), NO_SAMPLER,     VK_IMAGE_LAYOUT_GENERAL},
        {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, RD_CURRENT, {/*empty*/}, (highresFrames),  NO_SAMPLER,     VK_IMAGE_LAYOUT_GENERAL},
    }, VK_SHADER_STAGE_FRAGMENT_BIT);
println
println
    deferDescriptorsetup(&raygenBlocksPipe.setLayout, &raygenBlocksPipe.sets, {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, RD_CURRENT, (uniform), {/*empty*/}, NO_SAMPLER, NO_LAYOUT},
    }, VK_SHADER_STAGE_VERTEX_BIT);
    // setup_RayGen_Particles_Descriptors();
println
    deferDescriptorsetup(&raygenParticlesPipe.setLayout, &raygenParticlesPipe.sets, {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, RD_CURRENT , (uniform),   {/*empty*/},            NO_SAMPLER, NO_LAYOUT},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, RD_FIRST   , {/*empty*/}, {world},                NO_SAMPLER, VK_IMAGE_LAYOUT_GENERAL},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, RD_CURRENT , {/*empty*/}, (originBlockPalette), NO_SAMPLER, VK_IMAGE_LAYOUT_GENERAL},
    }, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT);

    deferDescriptorsetup(&raygenGrassPipe.setLayout, &raygenGrassPipe.sets, {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, RD_CURRENT , (uniform),   {/*empty*/},            NO_SAMPLER, NO_LAYOUT},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, RD_FIRST   , {/*empty*/}, {world},                NO_SAMPLER, VK_IMAGE_LAYOUT_GENERAL},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, RD_CURRENT , {/*empty*/}, (originBlockPalette), NO_SAMPLER, VK_IMAGE_LAYOUT_GENERAL},
    }, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT);

println
    flushDescriptorSetup();
println

    //not worth abstracting
    createRenderPass1();
    createRenderPass2();

    raygenBlocksPipe.renderPass = raygen2glossyRpass;
    raygenParticlesPipe.renderPass = raygen2glossyRpass;
    raygenGrassPipe.renderPass = raygen2glossyRpass;
    diffusePipe.renderPass = raygen2glossyRpass;
    glossyPipe.renderPass = raygen2glossyRpass;
    blurPipe.renderPass = blur2presentRpass;
    overlayPipe.renderPass = blur2presentRpass;

    //that is why NOT abstracting vulkan is also an option
    //if you cannot guess what things mean by just looking at them maybe read old (0.0.3) release src
println
    raygenBlocksPipe.subpassId = 0;
    create_Raster_Pipeline(&raygenBlocksPipe, {
            {"shaders/compiled/rayGenVert.spv", VK_SHADER_STAGE_VERTEX_BIT}, 
            {"shaders/compiled/rayGenFrag.spv", VK_SHADER_STAGE_FRAGMENT_BIT},
        },{
            {VK_FORMAT_R8G8B8_UINT, offsetof(VoxelVertex, pos)},
            {VK_FORMAT_R8G8B8_SINT, offsetof(VoxelVertex, norm)},
            {VK_FORMAT_R8_UINT, offsetof(VoxelVertex, matID)},
        }, 
        sizeof(VoxelVertex), VK_VERTEX_INPUT_RATE_VERTEX, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        swapChainExtent, {NO_BLEND}, (sizeof(quat) + sizeof(vec4))*2, DO_TEST);

println
    raygenParticlesPipe.subpassId = 1;
    create_Raster_Pipeline(&raygenParticlesPipe, {
            {"shaders/compiled/rayGenParticlesVert.spv", VK_SHADER_STAGE_VERTEX_BIT}, 
            {"shaders/compiled/rayGenParticlesGeom.spv", VK_SHADER_STAGE_GEOMETRY_BIT},
            {"shaders/compiled/rayGenFrag.spv", VK_SHADER_STAGE_FRAGMENT_BIT},
        },{
            {VK_FORMAT_R32G32B32_SFLOAT, offsetof(Particle, pos)},
            {VK_FORMAT_R32G32B32_SFLOAT, offsetof(Particle, vel)},
            {VK_FORMAT_R32_SFLOAT, offsetof(Particle, lifeTime)},
            {VK_FORMAT_R8_UINT, offsetof(Particle, matID)},
        }, 
        sizeof(Particle), VK_VERTEX_INPUT_RATE_VERTEX, VK_PRIMITIVE_TOPOLOGY_POINT_LIST,
        swapChainExtent, {NO_BLEND}, 0, DO_TEST);
    raygenGrassPipe.subpassId = 2;
    create_Raster_Pipeline(&raygenGrassPipe, {
            {"shaders/compiled/grass.spv", VK_SHADER_STAGE_VERTEX_BIT}, 
            {"shaders/compiled/rayGenFrag.spv", VK_SHADER_STAGE_FRAGMENT_BIT},
        },{/*empty*/}, 
        0, VK_VERTEX_INPUT_RATE_VERTEX, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        swapChainExtent, {NO_BLEND}, sizeof(vec4) + sizeof(int)*2, DO_TEST);
    
println
    diffusePipe.subpassId = 3;
    create_Raster_Pipeline(&diffusePipe, {
            {"shaders/compiled/diffuseVert.spv", VK_SHADER_STAGE_VERTEX_BIT}, 
            {"shaders/compiled/diffuseFrag.spv", VK_SHADER_STAGE_FRAGMENT_BIT},
        },{/*fullscreen pass*/}, 
        0, VK_VERTEX_INPUT_RATE_VERTEX, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        swapChainExtent, {NO_BLEND}, sizeof(ivec4) + sizeof(vec4)*4, NO_TEST);

println
    glossyPipe.subpassId = 4;
    create_Raster_Pipeline(&glossyPipe, {
            {"shaders/compiled/glossyVert.spv", VK_SHADER_STAGE_VERTEX_BIT}, 
            {"shaders/compiled/glossyFrag.spv", VK_SHADER_STAGE_FRAGMENT_BIT},
        },{/*fullscreen pass*/}, 
        0, VK_VERTEX_INPUT_RATE_VERTEX, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        swapChainExtent, {DO_BLEND}, sizeof(vec4) + sizeof(vec4), NO_TEST);

println
    blurPipe.subpassId = 0;
    create_Raster_Pipeline(&blurPipe, {
            {"shaders/compiled/blurVert.spv", VK_SHADER_STAGE_VERTEX_BIT}, 
            {"shaders/compiled/blurFrag.spv", VK_SHADER_STAGE_FRAGMENT_BIT},
        },{/*fullscreen pass*/}, 
        0, VK_VERTEX_INPUT_RATE_VERTEX, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        swapChainExtent, {NO_BLEND}, 0, NO_TEST);

println
    overlayPipe.subpassId = 1;
    create_Raster_Pipeline(&overlayPipe, {
            {"shaders/compiled/overlayVert.spv", VK_SHADER_STAGE_VERTEX_BIT}, 
            {"shaders/compiled/overlayFrag.spv", VK_SHADER_STAGE_FRAGMENT_BIT}, 
        },{
            {VK_FORMAT_R32G32_SFLOAT, offsetof(Rml::Vertex, position)},
            {VK_FORMAT_R8G8B8A8_UNORM, offsetof(Rml::Vertex, colour)},
            {VK_FORMAT_R32G32_SFLOAT, offsetof(Rml::Vertex, tex_coord)},
        }, 
        sizeof(Rml::Vertex), VK_VERTEX_INPUT_RATE_VERTEX, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        swapChainExtent, {DO_BLEND}, sizeof(vec4)+sizeof(mat4), NO_TEST);

println
    // create_Compute_Pipeline(

    // );
    // create_compute_pipelines();
println
    create_Compute_Pipeline(&mapPipe,        "shaders/compiled/map.spv",        sizeof(mat4) + sizeof(ivec4),   0);
    // create_Compute_Pipeline(&mipmapPipe,     "shaders/compiled/mipmap.spv",     0,                              VK_PIPELINE_CREATE_DISPATCH_BASE_BIT);
println
    // create_Compute_Pipeline(&raytracePipe,   "shaders/compiled/raytrace.spv",   sizeof(ivec4) + sizeof(vec4)*4, 0);
println
    create_Compute_Pipeline(&radiancePipe,   "shaders/compiled/radiance.spv",   sizeof(int)*2,                  VK_PIPELINE_CREATE_DISPATCH_BASE_BIT);
    // create_Compute_Pipeline(&diffusePipe,    "shaders/compiled/diffuse.spv",    sizeof(ivec4) + sizeof(vec4)*4, 0);
    // create_Compute_Pipeline(&glossyPipe,     "shaders/compiled/glossy.spv",     sizeof(vec4) + sizeof(vec4),    0);
println
    // create_Compute_Pipeline(&denoisePipe,    "shaders/compiled/denoise.spv",    sizeof(int)*2,                  0);
    // create_Compute_Pipeline(&accumulatePipe, "shaders/compiled/accumulate.spv", 0,                              0);
    // if(scaled) {
        // create_Compute_Pipeline(&upscalePipe, "shaders/compiled/upscale.spv", 0, 0);
    // }
    create_Sync_Objects();
println
}
void Renderer::deleteImages(vector<Image>* images) {
    for (int i=0; i<MAX_FRAMES_IN_FLIGHT; i++) {
         vkDestroyImageView(device,  (*images)[i].view, NULL);
        vmaDestroyImage(VMAllocator, (*images)[i].image, (*images)[i].alloc);
    }
}
void Renderer::deleteImages(Image* image) {
    // for (int i=0; i<MAX_FRAMES_IN_FLIGHT; i++) {
         vkDestroyImageView(device,  (*image).view, NULL);
        vmaDestroyImage(VMAllocator, (*image).image, (*image).alloc);
    // }
}
void Renderer::cleanup() {
    delete ui_render_interface;
	
    deleteImages(&world);
    deleteImages(&radianceCache);
    deleteImages(&originBlockPalette);
    deleteImages(&materialPalette);

    vkDestroySampler(device, nearestSampler, NULL);
    vkDestroySampler(device,  linearSampler, NULL);
    vkDestroySampler(device,      overlaySampler, NULL);

    for (int i=0; i<MAX_FRAMES_IN_FLIGHT; i++) {
            vmaUnmapMemory(VMAllocator,                           stagingWorld[i].alloc);
            vmaUnmapMemory(VMAllocator,                           gpuParticles[i].alloc);
        vmaDestroyBuffer(VMAllocator,  stagingWorld[i].buffer, stagingWorld[i].alloc);
        vmaDestroyBuffer(VMAllocator,  gpuParticles[i].buffer, gpuParticles[i].alloc);
        vmaDestroyBuffer(VMAllocator,  uniform[i].buffer, uniform[i].alloc);
    }

    vkDestroyDescriptorPool(device, descriptorPool, NULL);

    for (int i=0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroySemaphore(device,  imageAvailableSemaphores[i], NULL);
        vkDestroySemaphore(device,  renderFinishedSemaphores[i], NULL);
        vkDestroySemaphore(device, raytraceFinishedSemaphores[i], NULL);
        vkDestroySemaphore(device,  rayGenFinishedSemaphores[i], NULL);
        vkDestroyFence(device, graphicalInFlightFences[i], NULL);
        vkDestroyFence(device,   raytraceInFlightFences[i], NULL);
        vkDestroyFence(device,    frameInFlightFences[i], NULL);
    }
    vkDestroyCommandPool(device, commandPool, NULL);
    vkDestroyRenderPass(device, raygen2glossyRpass, NULL);
    vkDestroyRenderPass(device, blur2presentRpass, NULL);
    // vkDestroyRenderPass(device, rayGenParticlesRenderPass, NULL);

    destroy_Compute_Pipeline(       &mapPipe);
    destroy_Compute_Pipeline(  &raytracePipe);
    destroy_Compute_Pipeline(  &radiancePipe);
    // destroy_Compute_Pipeline(   &diffusePipe);
    // destroy_Compute_Pipeline(    &glossyPipe);
    // destroy_Compute_Pipeline(   &denoisePipe);
    // destroy_Compute_Pipeline(   &upscalePipe);
    // destroy_Compute_Pipeline(&accumulatePipe);

    destroy_Raster_Pipeline(&raygenBlocksPipe);
    destroy_Raster_Pipeline(&raygenParticlesPipe);
    destroy_Raster_Pipeline(&overlayPipe);

    cleanupSwapchainDependent();

    for(int i=0; i<MAX_FRAMES_IN_FLIGHT+1; i++) {
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

void Renderer::createSwapchain() {
    SwapChainSupportDetails swapChainSupport = querySwapchainSupport(physicalDevice);

    VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
    VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
    VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities);

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

    QueueFamilyIndices indices = findQueueFamilies(physicalDevice);
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
    swapchainImages.resize(imageCount);
    vector<VkImage> imgs (imageCount);

    vkGetSwapchainImagesKHR(device, swapchain, &imageCount, imgs.data());
    for(int i=0; i<imageCount; i++) {
        swapchainImages[i].image = imgs[i];
    }


    swapChainImageFormat = surfaceFormat.format;
    swapChainExtent = extent;
    
    raytraceExtent.width  = swapChainExtent.width  / _ratio;
    raytraceExtent.height = swapChainExtent.height / _ratio;
}

void Renderer::cleanupSwapchainDependent() {
    deleteImages(&depthHighres);
    deleteImages(&highresFrames);    
    deleteImages(&matNormHighres);

    
    for (auto framebuffer : rayGenFramebuffers) {
        vkDestroyFramebuffer(device, framebuffer, NULL);
    }
    for (auto framebuffer : overlayFramebuffers) {
        vkDestroyFramebuffer(device, framebuffer, NULL);
    }
    for (auto img : swapchainImages) {
        vkDestroyImageView(device, img.view, NULL);
    }
    vkDestroySwapchainKHR(device, swapchain, NULL);
}
void Renderer::createSwapchainDependent() {
    // int mipmaps;
    create_Image_Storages(&matNormHighres,
        VK_IMAGE_TYPE_2D,
        VK_FORMAT_R8G8B8A8_SNORM,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT,
        {swapChainExtent.width, swapChainExtent.height, 1});
    create_Image_Storages(&depthHighres,
        VK_IMAGE_TYPE_2D,
        DEPTH_FORMAT,
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        VK_IMAGE_ASPECT_DEPTH_BIT,
        {swapChainExtent.width, swapChainExtent.height, 1});
    create_Image_Storages(&highresFrames,
        VK_IMAGE_TYPE_2D,
        RAYTRACED_IMAGE_FORMAT,
        // VK_FORMAT_R8G8B8A8_UNORM,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT,
        {swapChainExtent.width, swapChainExtent.height, 1});

    vector<vector<VkImageView>> rayGenVeiws(3);
    for(int i=0; i<MAX_FRAMES_IN_FLIGHT; i++) {
        rayGenVeiws[0].push_back(matNormHighres[i].view);
        rayGenVeiws[1].push_back(highresFrames[i].view);
        rayGenVeiws[2].push_back(depthHighres[i].view);
    }
    vector<vector<VkImageView>> blurVeiws(2);
    for(int i=0; i<MAX_FRAMES_IN_FLIGHT; i++) {
        blurVeiws[0].push_back(matNormHighres[i].view);
        blurVeiws[1].push_back(highresFrames[i].view);
    }
    
println
    create_N_Framebuffers(&rayGenFramebuffers, &rayGenVeiws, raygen2glossyRpass, MAX_FRAMES_IN_FLIGHT, swapChainExtent.width, swapChainExtent.height);
    
println
    create_N_Framebuffers(&overlayFramebuffers, &blurVeiws, blur2presentRpass, MAX_FRAMES_IN_FLIGHT, swapChainExtent.width, swapChainExtent.height);
println
}
void Renderer::recreateSwapchainDependent() {
    int width = 0, height = 0;
    glfwGetFramebufferSize(window.pointer, &width, &height);
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize(window.pointer, &width, &height);
        glfwWaitEvents();
    }
    
    vkDeviceWaitIdle(device);
    cleanupSwapchainDependent();
    vkDeviceWaitIdle(device);

    createSwapchain();
    createSwapchainImageViews();

    vkDestroyCommandPool(device, commandPool, NULL);

    create_Command_Pool();
    create_Command_Buffers(&overlayCommandBuffers, MAX_FRAMES_IN_FLIGHT);
    create_Command_Buffers(&   rayGenCommandBuffers, MAX_FRAMES_IN_FLIGHT);
    create_Command_Buffers(&  computeCommandBuffers, MAX_FRAMES_IN_FLIGHT);

    VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = MAX_FRAMES_IN_FLIGHT;
    copyOverlayCommandBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    VK_CHECK(vkAllocateCommandBuffers(device, &allocInfo, copyOverlayCommandBuffers.data())); 
    // renderOverlayCommandBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    // VK_CHECK(vkAllocateCommandBuffers(device, &allocInfo, renderOverlayCommandBuffers.data())); 

    createSwapchainDependent();

    // setup_Raytrace_Descriptors();
    // setup_Denoise_Descriptors();
    // setup_Radiance_Cache_Descriptors();
    // setup_Diffuse_Descriptors();
    if(scaled) {
        // setup_Upscale_Descriptors();
    }
    // setup_Accumulate_Descriptors();
    // setup_RayGen_Descriptors();
    // setup_RayGen_Particles_Descriptors();

    
    vkDeviceWaitIdle(device);
} 

void Renderer::createSwapchainImageViews() {
    for(i32 i=0; i<swapchainImages.size(); i++) {
        VkImageViewCreateInfo createInfo = {};
        createInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        createInfo.image = swapchainImages[i].image;
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
        swapchainImages[i].aspect = VK_IMAGE_ASPECT_COLOR_BIT;
        
        VK_CHECK(vkCreateImageView(device, &createInfo, NULL, &swapchainImages[i].view));
    }
}

void Renderer::create_N_Framebuffers(vector<VkFramebuffer>* framebuffers, vector<vector<VkImageView>>* views, VkRenderPass renderPass, u32 N, u32 Width, u32 Height) {
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

void Renderer::update_particles() {
    int write_index = 0;
    for (int i=0; i<particles.size(); i++) {
        bool should_keep = particles[i].lifeTime > 0;
        
        if(should_keep) {
            particles[write_index] = particles[i];
            // delta_time = 1/75.0;
            particles[write_index].pos += particles[write_index].vel * float(deltaTime);
            particles[write_index].lifeTime -= deltaTime;
            
            write_index++;
        }        
    }
    particles.resize(write_index);

    int capped_particle_count = glm::clamp(int(particles.size()), 0, maxParticleCount);
    
    memcpy(gpuParticlesMapped[currentFrame], particles.data(), sizeof(Particle)*capped_particle_count);
    vmaFlushAllocation(VMAllocator, gpuParticles[currentFrame].alloc, 0, sizeof(Particle)*capped_particle_count);
}

void Renderer::process_ui_deletion_queue() {
    int write_index = 0;
    for (int i=0; i<uiMeshDeletionQueue.size(); i++) {
        bool should_keep = uiMeshDeletionQueue[i].life_counter > 0;
        if(should_keep) {
            uiMeshDeletionQueue[write_index] = uiMeshDeletionQueue[i];
            uiMeshDeletionQueue[write_index].life_counter -= 1;
            write_index++;
        }
        else {
            //free buffer

            vmaDestroyBuffer(VMAllocator, uiMeshDeletionQueue[i].mesh.vertexes.buffer, uiMeshDeletionQueue[i].mesh.vertexes.alloc);
            vmaDestroyBuffer(VMAllocator, uiMeshDeletionQueue[i].mesh.indexes.buffer, uiMeshDeletionQueue[i].mesh.indexes.alloc);
        }
    }
    uiMeshDeletionQueue.resize(write_index);

    write_index = 0;
    for (int i=0; i<uiImageDeletionQueue.size(); i++) {
        bool should_keep = uiImageDeletionQueue[i].life_counter > 0;
        if(should_keep) {
            uiImageDeletionQueue[write_index] = uiImageDeletionQueue[i];
            uiImageDeletionQueue[write_index].life_counter -= 1;
            write_index++;
        }
        else {
            //free image
            vmaDestroyImage(VMAllocator, uiImageDeletionQueue[i].image.image, uiImageDeletionQueue[i].image.alloc);
            vkDestroyImageView(device, uiImageDeletionQueue[i].image.view, NULL);
        }
    }
    uiImageDeletionQueue.resize(write_index);

    write_index = 0;
    for (int i=0; i<uiBufferDeletionQueue.size(); i++) {
        bool should_keep = uiBufferDeletionQueue[i].life_counter > 0;
        if(should_keep) {
            uiBufferDeletionQueue[write_index] = uiBufferDeletionQueue[i];
            uiBufferDeletionQueue[write_index].life_counter -= 1;
            write_index++;
        }
        else {
            //free image
            vmaDestroyBuffer(VMAllocator, uiBufferDeletionQueue[i].buffer.buffer, uiBufferDeletionQueue[i].buffer.alloc);
        }
    }
    uiBufferDeletionQueue.resize(write_index);
}

// #include <glm/gtx/string_cast.hpp>
void Renderer::start_frame() {
    dvec3 up = glm::dvec3(0.0f, 0.0f, 1.0f); // Up vector

    dmat4 view = glm::lookAt(cameraPos, cameraPos+cameraDir, up);
    const double voxel_in_pixels = 5.0; //we want voxel to be around 10 pixels in width / height
    const double  view_width_in_voxels = 1920.0 / voxel_in_pixels; //todo make dynamic and use spec constnats
    const double view_height_in_voxels = 1080.0 / voxel_in_pixels;
    dmat4 projection = glm::ortho(-view_width_in_voxels/2.0, view_width_in_voxels/2.0, view_height_in_voxels/2.0, -view_height_in_voxels/2.0, -0.0, +2000.0); // => *100.0 decoding
    dmat4     worldToScreen = projection * view;

        cameraTransform_OLD = cameraTransform;
    cameraTransform = worldToScreen;
    vkWaitForFences(device, 1, &frameInFlightFences[currentFrame], VK_TRUE, UINT32_MAX);
    vkResetFences  (device, 1, &frameInFlightFences[currentFrame]);
}

void Renderer::start_raygen() {
    VkCommandBuffer &commandBuffer = rayGenCommandBuffers[currentFrame];
    
    struct unicopy {mat4 trans; mat4 old_trans;} unicopy = {cameraTransform, cameraTransform_OLD};
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
    cmdTransLayoutBarrier(commandBuffer, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT|VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT|VK_PIPELINE_STAGE_TRANSFER_BIT, VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT, VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT, &originBlockPalette[currentFrame]);

    vector<VkClearValue> clearColors = {
        {}, 
        {}, 
        {}
    };
    VkRenderPassBeginInfo renderPassInfo = {};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = raygen2glossyRpass;
        renderPassInfo.framebuffer = rayGenFramebuffers[currentFrame];
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = swapChainExtent;
        renderPassInfo.clearValueCount = clearColors.size();
        renderPassInfo.pClearValues    = clearColors.data();

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, raygenBlocksPipe.line);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, raygenBlocksPipe.lineLayout, 0, 1, &raygenBlocksPipe.sets[currentFrame], 0, 0);

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
static bool is_face_visible(vec3 normal, vec3 camera_dir) {
    return (dot(normal, camera_dir) < 0.0f);
}
void Renderer::raygen_mesh(Mesh *mesh) {
    VkCommandBuffer &commandBuffer = rayGenCommandBuffers[currentFrame];

        VkBuffer vertexBuffers[] = {(*mesh).triangles.vertexes[currentFrame].buffer};
        VkDeviceSize offsets[] = {0};
    
    struct {quat r1,r2; vec4 s1,s2;} raygen_pushconstant = {(*mesh).rot, (*mesh).old_rot, vec4((*mesh).shift,0), vec4((*mesh).old_shift,0)};
    //TODO:
    vkCmdPushConstants(commandBuffer, raygenBlocksPipe.lineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(raygen_pushconstant), &raygen_pushconstant);

    (*mesh).old_rot   = (*mesh).rot;
    (*mesh).old_shift = (*mesh).shift;

    // glm::mult
    // if(old_buff != (*mesh).indexes[currentFrame].buffer) {
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
    // printl((*mesh).triangles.Pzz.icount);
    if(is_face_visible(mesh->rot*vec3(+1,0,0), cameraDir)) {
        vkCmdBindIndexBuffer(commandBuffer, (*mesh).triangles.Pzz.indexes[currentFrame].buffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(commandBuffer, (*mesh).triangles.Pzz.icount, 1, 0, 0, 0);
    }
    if(is_face_visible(mesh->rot*vec3(-1,0,0), cameraDir)) {
        vkCmdBindIndexBuffer(commandBuffer, (*mesh).triangles.Nzz.indexes[currentFrame].buffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(commandBuffer, (*mesh).triangles.Nzz.icount, 1, 0, 0, 0);
    }
    if(is_face_visible(mesh->rot*vec3(0,+1,0), cameraDir)) {
        vkCmdBindIndexBuffer(commandBuffer, (*mesh).triangles.zPz.indexes[currentFrame].buffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(commandBuffer, (*mesh).triangles.zPz.icount, 1, 0, 0, 0);
    }
    if(is_face_visible(mesh->rot*vec3(0,-1,0), cameraDir)) {
        vkCmdBindIndexBuffer(commandBuffer, (*mesh).triangles.zNz.indexes[currentFrame].buffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(commandBuffer, (*mesh).triangles.zNz.icount, 1, 0, 0, 0);
    }
    if(is_face_visible(mesh->rot*vec3(0,0,+1), cameraDir)) {
        vkCmdBindIndexBuffer(commandBuffer, (*mesh).triangles.zzP.indexes[currentFrame].buffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(commandBuffer, (*mesh).triangles.zzP.icount, 1, 0, 0, 0);
    }
    if(is_face_visible(mesh->rot*vec3(0,0,-1), cameraDir)) {
        vkCmdBindIndexBuffer(commandBuffer, (*mesh).triangles.zzN.indexes[currentFrame].buffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdDrawIndexed(commandBuffer, (*mesh).triangles.zzN.icount, 1, 0, 0, 0);
    }
        // old_buff = (*mesh).indexes[currentFrame].buffer;
    // }
}
void Renderer::raygen_map_particles() {
    VkCommandBuffer &commandBuffer = rayGenCommandBuffers[currentFrame];

    //go to next no matter what
    vkCmdNextSubpass(commandBuffer, VK_SUBPASS_CONTENTS_INLINE);
    
    if(particles.size() > 0) { //just for safity
        vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, raygenParticlesPipe.line);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, raygenParticlesPipe.lineLayout, 0, 1, &raygenParticlesPipe.sets[currentFrame], 0, 0);

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
        vkCmdBindVertexBuffers(commandBuffer, 0, 1, &gpuParticles[currentFrame].buffer, offsets);
        // assert(particles.size() != 0);
        vkCmdDraw(commandBuffer, particles.size(), 1, 0, 0);
    }
}
void Renderer::raygen_start_grass(){
    VkCommandBuffer &commandBuffer = rayGenCommandBuffers[currentFrame];

    vkCmdNextSubpass(commandBuffer, VK_SUBPASS_CONTENTS_INLINE);
    
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, raygenParticlesPipe.line);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, raygenParticlesPipe.lineLayout, 0, 1, &raygenParticlesPipe.sets[currentFrame], 0, 0);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, raygenGrassPipe.line);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, raygenGrassPipe.lineLayout, 0, 1, &raygenGrassPipe.sets[currentFrame], 0, 0);

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
void Renderer::raygen_map_grass(vec4 shift, int size){
    VkCommandBuffer &commandBuffer = rayGenCommandBuffers[currentFrame];

    struct {vec4 _shift; int _size, _time;} raygen_pushconstant = {shift, size, itime};
    vkCmdPushConstants(commandBuffer, raygenGrassPipe.lineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(raygen_pushconstant), &raygen_pushconstant);

    const int verts_per_blade = 4*6 + 3; 
    vkCmdDraw(commandBuffer, verts_per_blade*size*size, 1, 0, 0);
}
void Renderer::end_raygen() {
    VkCommandBuffer &commandBuffer = rayGenCommandBuffers[currentFrame];
 }
void Renderer::start_compute() {
    VkCommandBuffer &commandBuffer = rayGenCommandBuffers[currentFrame];

    vkResetCommandBuffer(commandBuffer, /*VkCommandBufferResetFlagBits*/ 0);
    VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = 0;
        beginInfo.pInheritanceInfo = NULL;
    VK_CHECK(vkBeginCommandBuffer(commandBuffer, &beginInfo));
  
    cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
}
void Renderer::start_blockify() {
    VkCommandBuffer &commandBuffer = rayGenCommandBuffers[currentFrame];

    copyQueue.clear();

    paletteCounter = static_block_palette_size; //TODO: change to static block count. Currently its size of 1 cause of air
    size_t size_to_copy = world_size.x * world_size.y * world_size.z * sizeof(BlockID_t);
    memcpy(current_world.data(), origin_world.data(), size_to_copy);
}
struct AABB {
  vec3 min;
  vec3 max;
};
static AABB get_shift(dmat4 trans, ivec3 size) {
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
void Renderer::blockify_mesh(Mesh* mesh) {
    mat4 rotate = toMat4((*mesh).rot);
    mat4 shift = translate(identity<mat4>(), (*mesh).shift);

    struct AABB border_in_voxel = get_shift(shift*rotate, (*mesh).size);
    
    struct {ivec3 min; ivec3 max;} border; 
    border.min = (ivec3(border_in_voxel.min)-ivec3(1)) / ivec3(16);
    border.max = (ivec3(border_in_voxel.max)+ivec3(1)) / ivec3(16);

    border.min = glm::clamp(border.min, ivec3(0), ivec3(world_size-1));
    border.max = glm::clamp(border.max, ivec3(0), ivec3(world_size-1));
    
    for(int xx = border.min.x; xx <= border.max.x; xx++) {
    for(int yy = border.min.y; yy <= border.max.y; yy++) {
    for(int zz = border.min.z; zz <= border.max.z; zz++) {
        //if inside model TODO
        // if(
        //     any(greaterThanEqual(ivec3(xx,yy,zz), ivec3(8))) ||
        //     any(   lessThan     (ivec3(xx,yy,zz), ivec3(0)))) {
        //         continue;
        //     }
        //static blocks TODO
        int current_block = current_world(xx,yy,zz); 
        // if (current_block == 0) { //if not allocated
        //     current_world(xx,yy,zz) = palette_counter;
        //     palette_counter++;
        // } else {
            if(current_block < static_block_palette_size) { // static and non-empty
                //add to copy queue
                ivec2 src_block = {}; 
                tie(src_block.x, src_block.y) = get_block_xy(current_block);
                ivec2 dst_block = {}; 
                tie(dst_block.x, dst_block.y) = get_block_xy(paletteCounter);
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
                    copyQueue.push_back(static_block_copy);

                    // printf("pc %d ", palette_counter);
                    current_world(xx,yy,zz) = paletteCounter;
                    paletteCounter++; // yeah i dont trust myself in this  
            } else {
                //already new block, just leave it
            }
        // } //TODO copy empty instead of whole image
    }}}
    // printf("palette_counter %d ", palette_counter);
    // printl(world_size.z);
    // printl(world_size.y);
    // printl(world_size.x);
}
void Renderer::end_blockify() {
    // vkDeviceWaitIdle(device);
    size_t size_to_copy = world_size.x * world_size.y * world_size.z * sizeof(BlockID_t);
    assert(size_to_copy != 0);
    memcpy(stagingWorldMapped[currentFrame], current_world.data(), size_to_copy);
    vmaFlushAllocation(VMAllocator, stagingWorld[currentFrame].alloc, 0, size_to_copy);
    // printf("static_block_palette_size %d\n", static_block_palette_size);
    for(int zz = 0; zz < world_size.z; zz++) {
    for(int yy = 0; yy < world_size.y; yy++) {
    for(int xx = 0; xx < world_size.x; xx++) {
        int block_id = current_world(xx,yy,zz);

        assert(block_id <= paletteCounter);
        assert(block_id >= 0);
        // printf("%d ", block_id);
    }
    // printf("\n");
    }
    // printf("\n");
    }
    
}

void Renderer::exec_copies() {
    VkCommandBuffer &commandBuffer = rayGenCommandBuffers[currentFrame];

    cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

    if(copyQueue.size() != 0) {
        //we can copy from previous image, cause static blocks are same in both palettes. Additionaly, it gives src and dst optimal layouts
        cmdTransLayoutBarrier(commandBuffer, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT , VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT, &originBlockPalette[previousFrame]);
        cmdTransLayoutBarrier(commandBuffer, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT , VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT, &originBlockPalette[ currentFrame]);

        vkCmdCopyImage(commandBuffer, originBlockPalette[previousFrame].image, VK_IMAGE_LAYOUT_GENERAL, originBlockPalette[currentFrame].image, VK_IMAGE_LAYOUT_GENERAL, copyQueue.size(), copyQueue.data());

        cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

        cmdTransLayoutBarrier(commandBuffer, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT, &originBlockPalette[previousFrame]);
        cmdTransLayoutBarrier(commandBuffer, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT, &originBlockPalette[ currentFrame]);
    }
    VkBufferImageCopy copyRegion = {};
    // copyRegion.size = size;
        copyRegion.imageExtent.width  = world_size.x;
        copyRegion.imageExtent.height = world_size.y;
        copyRegion.imageExtent.depth  = world_size.z;
        copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copyRegion.imageSubresource.layerCount = 1;
    cmdTransLayoutBarrier(commandBuffer, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_MEMORY_WRITE_BIT|VK_ACCESS_MEMORY_READ_BIT, VK_ACCESS_MEMORY_WRITE_BIT|VK_ACCESS_MEMORY_READ_BIT, &world);
    vkCmdCopyBufferToImage(commandBuffer, stagingWorld[currentFrame].buffer, world.image, VK_IMAGE_LAYOUT_GENERAL, 1, &copyRegion);
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
void Renderer::blockify_custom(void* ptr) {
    size_t size_to_copy = world_size.x * world_size.y * world_size.z * sizeof(BlockID_t);
    memcpy(stagingWorldMapped[currentFrame], ptr, size_to_copy);
    vmaFlushAllocation(VMAllocator, stagingWorld[currentFrame].alloc, 0, size_to_copy);
}

void Renderer::start_map() {
    VkCommandBuffer &commandBuffer = rayGenCommandBuffers[currentFrame];
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, mapPipe.line);
}

void Renderer::map_mesh(Mesh* mesh) {
    VkCommandBuffer &commandBuffer = rayGenCommandBuffers[currentFrame];
    
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
            blockPaletteInfo.imageView = originBlockPalette[currentFrame].view;
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

    vkCmdPushDescriptorSetKHR(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, mapPipe.lineLayout, 0, descriptorWrites.size(), descriptorWrites.data());

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
    vkCmdPushConstants(commandBuffer, mapPipe.lineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(itrans_shift), &itrans_shift);
    // ivec3 adjusted_size = ivec3(vec3(mesh.size) * vec3(2.0));
    vkCmdDispatch(commandBuffer, (map_area.x*3+3) / 4, (map_area.y*3+3) / 4, (map_area.z*3+3) / 4);   
}

void Renderer::end_map() {
    VkCommandBuffer &commandBuffer = rayGenCommandBuffers[currentFrame];

    cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
    cmdTransLayoutBarrier(commandBuffer, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT, VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT, &originBlockPalette[currentFrame]);
    cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

    //submit lod update for every block
    // vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, mipmapPipe.pipe);
    //     vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, mipmapPipe.pipeLayout, 0, 1, &mipmapPipe.descriptors[currentFrame], 0, 0);
    
    // vkCmdDispatch(commandBuffer, (raytraceExtent.width+7)/8, (raytraceExtent.height+7)/8, 1);
    // int nearest_sq_size =  ceil(sqrt((double)palette_counter));
    // int bounding_x_size = 16*BLOCK_PALETTE_SIZE_X;
    // int bounding_y_size = 16*((palette_counter+BLOCK_PALETTE_SIZE_X-1) / BLOCK_PALETTE_SIZE_X);

    // VkImageMemoryBarrier mipmap_barrier = {};
    //     mipmap_barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    //     mipmap_barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    //     mipmap_barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    //     mipmap_barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    //     mipmap_barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    //     mipmap_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    //     mipmap_barrier.subresourceRange.baseMipLevel = 0;
    //     mipmap_barrier.subresourceRange.levelCount = 1;
    //     mipmap_barrier.subresourceRange.baseArrayLayer = 0;
    //     mipmap_barrier.subresourceRange.layerCount = 1;
    //     mipmap_barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT;
    //     mipmap_barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;    
    //     mipmap_barrier.image = origin_block_palette[currentFrame].image;
    // //0 to 1
    // cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, &mipmap_barrier);
    // vkCmdDispatchBase(
    //     commandBuffer,
    //     0,0,16,
    //     bounding_x_size/2,bounding_y_size/2,16/2);
    // //1 to 2
    // cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, &mipmap_barrier);
    // vkCmdDispatchBase(
    //     commandBuffer,
    //     0,0,16+8,
    //     bounding_x_size/4,bounding_y_size/4,16/4);
    // //2 to 3
    // cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, &mipmap_barrier);
    // vkCmdDispatchBase(
    //     commandBuffer,
    //     0,0,16+8+4,
    //     bounding_x_size/8,bounding_y_size/8,16/8);
    // //3 to 4
    // cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, &mipmap_barrier);
    // vkCmdDispatchBase(
    //     commandBuffer,
    //     0,0,16+8+4+2,
    //     bounding_x_size/16,bounding_y_size/16,16/16);
    // cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, &mipmap_barrier);
    VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = originBlockPalette[currentFrame].image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;    
    cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT|VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT|VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, &barrier); 
    cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT|VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT|VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT);

    VkImageMemoryBarrier imageMemoryBarrier = {};
        imageMemoryBarrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        imageMemoryBarrier.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;    // Access mask for the vertex shader write
        imageMemoryBarrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;     // Access mask for the fragment shader read
        imageMemoryBarrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;           // The layout used during write
        imageMemoryBarrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;           // The layout used during read
        imageMemoryBarrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        imageMemoryBarrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        imageMemoryBarrier.image = originBlockPalette[currentFrame].image;  // The image being synchronized
        imageMemoryBarrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        imageMemoryBarrier.subresourceRange.baseMipLevel = 0;
        imageMemoryBarrier.subresourceRange.levelCount = 1;
        imageMemoryBarrier.subresourceRange.baseArrayLayer = 0;
        imageMemoryBarrier.subresourceRange.layerCount = 1;

    vkCmdPipelineBarrier(
        commandBuffer,
        VK_PIPELINE_STAGE_VERTEX_SHADER_BIT, // srcStageMask
        VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, // dstStageMask
        0,                                     // dependencyFlags
        0, NULL,                               // memoryBarrierCount & pMemoryBarriers
        0, NULL,                               // bufferMemoryBarrierCount & pBufferMemoryBarriers
        1, &imageMemoryBarrier                 // imageMemoryBarrierCount & pImageMemoryBarriers
    );
}

// void Renderer::raytrace() {
//     VkCommandBuffer &commandBuffer = computeCommandBuffers[currentFrame];
    
//     cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

//     cmdTransLayoutBarrier(commandBuffer, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT, VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT, 
//         &lowresFrames[currentFrame]);
    
//     vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, raytracePipe.line);

//         vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, raytracePipe.lineLayout, 0, 1, &raytracePipe.sets[currentFrame], 0, 0);

//         cameraPos_OLD = cameraPos;
//         cameraDir_OLD = cameraDir;
//         // struct rtpc {vec4 v1, v2;} raytrace_pushconstant = {vec4(camera_pos, float(0)), vec4(camera_dir,0)};
//         struct rtpc {vec4 v1, v2;} raytrace_pushconstant = {vec4(cameraPos, float(itime + 1)/100.0), vec4(normalize(cameraDir),0)};
//         vkCmdPushConstants(commandBuffer, raytracePipe.lineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(raytrace_pushconstant), &raytrace_pushconstant);
//         vkCmdDispatch(commandBuffer, (raytraceExtent.width+7)/8, (raytraceExtent.height+7)/8, 1);

//     VkImageMemoryBarrier barrier{};
//         barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
//         barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
//         barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
//         barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
//         barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
//         barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
//         barrier.subresourceRange.baseMipLevel = 0;
//         barrier.subresourceRange.levelCount = 1;
//         barrier.subresourceRange.baseArrayLayer = 0;
//         barrier.subresourceRange.layerCount = 1;
//         barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT;
//         barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;    

//     // cmdTransLayoutBarrier(VkCommandBuffer commandBuffer, VkImageLayout targetLayout, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask, Image *image)
//         barrier.image = scaled? lowresFrames[currentFrame].image : highresFrames[currentFrame].image;
//     cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, &barrier);    
//         cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, &barrier);   
//         cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
//         barrier.image = scaled? lowresFrames[currentFrame].image : highresFrames[currentFrame].image;
//     cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, &barrier);   
//     cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
// }
void Renderer::updade_radiance() {
    VkCommandBuffer &commandBuffer = rayGenCommandBuffers[currentFrame];
    
    cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

    
    int magic_number = itime % 2;
        
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, radiancePipe.line);

        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, radiancePipe.lineLayout, 0, 1, &radiancePipe.sets[currentFrame], 0, 0);

        cameraPos_OLD = cameraPos;
        cameraDir_OLD = cameraDir;
        struct rtpc {int time; int iters;} pushconstant = {itime, 0};
        vkCmdPushConstants(commandBuffer, radiancePipe.lineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pushconstant), &pushconstant);
        
        // vkCmdDispatch(commandBuffer, RCACHE_RAYS_PER_PROBE*world_size.x, world_size.y, world_size.z);
        // vkCmdDispatch(commandBuffer, RCACHE_RAYS_PER_PROBE*world_size.x, world_size.y, 7);
        // ivec3 fullsize = ivec3(RCACHE_RAYS_PER_PROBE*world_size.x, world_size.y, 7);
        // ivec3 partial_dispatch_size;
        //       partial_dispatch_size.x = (fullsize.x)/2;
        //       partial_dispatch_size.y = fullsize.y;
        //       partial_dispatch_size.z = fullsize.z;

        // ivec3 dispatch_shift = ivec3(partial_dispatch_size.x, 0, 0) * magic_number;
        VkImageMemoryBarrier _barrier = {};
                _barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
                _barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
                _barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
                _barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                _barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
                _barrier.image = radianceCache.image;
                _barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                _barrier.subresourceRange.baseMipLevel = 0;
                _barrier.subresourceRange.levelCount = 1;
                _barrier.subresourceRange.baseArrayLayer = 0;
                _barrier.subresourceRange.layerCount = 1;
                _barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT;
                _barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT; 
        // for(int i=0; i<4; i++) {
            for(int xx = 0; xx <= world_size.x; xx++) {
            for(int yy = 0; yy <= world_size.y; yy++) {
            for(int zz = 0; zz <= world_size.z; zz++) {
                int block_id = current_world(xx,yy,zz);

                //yeah kinda slow... but who cares on less then a million blocks?
                int sum_of_neighbours = 0;
                for(int dx=-1; dx<=+1; dx++) {
                for(int dy=-1; dy<=+1; dy++) {
                for(int dz=-1; dz<=+1; dz++) {
                    sum_of_neighbours += current_world(xx+dx,yy+dy,zz+dz); 
                }}}

                //TODO: finish dynamic update system, integrate with RaVE
                if(((block_id > static_block_palette_size) || (sum_of_neighbours != 0)) && ((xx+yy+zz) % 2 == magic_number)) {
                    vkCmdDispatchBase(
                        commandBuffer,
                        xx  ,yy  ,zz  ,
                        1,1,1);
                        //lol it was xx+1,yy+1,zz+1);
                }
            }}}
        
                    // vkCmdDispatchBase(
                    //     commandBuffer,
                    //     // (world_size.x /2) * magic_number, 0, 0,
                    //     0, 0, 0,
                    //     (world_size.x /2), world_size.y/2, 7);
            cmdPipelineBarrier(commandBuffer, 
                VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                &_barrier);
        // }
            // dispatch_shift.x, dispatch_shift.y, dispatch_shift.z,
            // fullsize.x, fullsize.y, fullsize.z);
            // partial_dispatch_size.x, partial_dispatch_size.y, partial_dispatch_size.z);

    VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = highresFrames[currentFrame].image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;    

    // cmdTransLayoutBarrier(VkCommandBuffer commandBuffer, VkImageLayout targetLayout, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask, Image *image)
    barrier.image = radianceCache.image;
    cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, &barrier);   
    cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
}
void Renderer::diffuse() {
    VkCommandBuffer &commandBuffer = rayGenCommandBuffers[currentFrame];
    
    // cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

    // cmdTransLayoutBarrier(commandBuffer, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT, VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT, 
    //     &lowresFrames[currentFrame]);
    vkCmdNextSubpass(commandBuffer, VK_SUBPASS_CONTENTS_INLINE);
    
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, diffusePipe.line);

        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, diffusePipe.lineLayout, 0, 1, &diffusePipe.sets[currentFrame], 0, 0);

        cameraPos_OLD = cameraPos;
        cameraDir_OLD = cameraDir;
        struct rtpc {vec4 v1, v2;} pushconstant = {vec4(cameraPos,intBitsToFloat(itime)), vec4(cameraDir,0)};
        vkCmdPushConstants(commandBuffer, diffusePipe.lineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pushconstant), &pushconstant);
        
        vkCmdDraw(commandBuffer, 3, 1, 0, 0);
}
void Renderer::glossy() {
    VkCommandBuffer &commandBuffer = rayGenCommandBuffers[currentFrame];

    vkCmdNextSubpass(commandBuffer, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, glossyPipe.line);

        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, glossyPipe.lineLayout, 0, 1, &glossyPipe.sets[currentFrame], 0, 0);

        cameraPos_OLD = cameraPos;
        cameraDir_OLD = cameraDir;
        struct rtpc {vec4 v1, v2;} pushconstant = {vec4(cameraPos,0), vec4(cameraDir,0)};
        vkCmdPushConstants(commandBuffer, glossyPipe.lineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pushconstant), &pushconstant);

        vkCmdDraw(commandBuffer, 3, 1, 0, 0);
        // vkCmdDispatch(commandBuffer, (raytraceExtent.width+7)/8, (raytraceExtent.height+7)/8, 1);

}
void Renderer::blur() {
    VkCommandBuffer &commandBuffer = rayGenCommandBuffers[currentFrame];

    
    vkCmdEndRenderPass(commandBuffer);

    vector<VkClearValue> clearColors = {
        {}, 
        {}, 
        {}
    };
    VkRenderPassBeginInfo renderPassInfo = {};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = blur2presentRpass;
        renderPassInfo.framebuffer = overlayFramebuffers[currentFrame];
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = swapChainExtent;
        renderPassInfo.clearValueCount = clearColors.size();
        renderPassInfo.pClearValues    = clearColors.data();

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, blurPipe.line);

        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, blurPipe.lineLayout, 0, 1, &blurPipe.sets[currentFrame], 0, 0);

        cameraPos_OLD = cameraPos;
        cameraDir_OLD = cameraDir;
        struct rtpc {vec4 v1, v2;} pushconstant = {vec4(cameraPos,0), vec4(cameraDir,0)};
        // vkCmdPushConstants(commandBuffer, blurPipe.lineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pushconstant), &pushconstant);

        vkCmdDraw(commandBuffer, 3, 1, 0, 0);
        // vkCmdDispatch(commandBuffer, (raytraceExtent.width+7)/8, (raytraceExtent.height+7)/8, 1);

}
// void Renderer::denoise(int iterations, int denoising_radius, denoise_targets target) {
//     VkCommandBuffer &commandBuffer = computeCommandBuffers[currentFrame];
    
//     cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
//     VkImageMemoryBarrier barrier{};
//         barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
//         barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
//         barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
//         barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
//         barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
//         barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
//         barrier.subresourceRange.baseMipLevel = 0;
//         barrier.subresourceRange.levelCount = 1;
//         barrier.subresourceRange.baseArrayLayer = 0;
//         barrier.subresourceRange.layerCount = 1;
//         barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT |VK_ACCESS_MEMORY_READ_BIT;
//         barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;

//     vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, denoisePipe.line);
//         VkExtent2D dispatchExtent = {};
//         switch (target) {
//             case DENOISE_TARGET_LOWRES:
//                 barrier.image = lowresFrames[currentFrame].image;
//                 vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, denoisePipe.lineLayout, 0, 1, &denoisePipe.sets[currentFrame], 0, 0);
//                 dispatchExtent = raytraceExtent;
//                 break;
//             case DENOISE_TARGET_HIGHRES:
//                 barrier.image = scaled? highresFrames[0].image : highresFrames[currentFrame].image;
//                 vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, denoisePipe.lineLayout, 0, 1, &denoisePipe.sets[currentFrame], 0, 0);
//                 dispatchExtent = swapChainExtent;
//                 break;
//         }

//         for(int atrous_iter = 0; atrous_iter<iterations; atrous_iter++) {
//             struct denoise_pc {int iter; int radius;} dpc = {atrous_iter, denoising_radius};
//             vkCmdPushConstants(commandBuffer, denoisePipe.lineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(dpc), &dpc);
//             vkCmdDispatch(commandBuffer, (dispatchExtent.width+7)/8, (dispatchExtent.height+7)/8, 1);
//             cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, &barrier);
//         } 

//     cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
// }
// void Renderer::accumulate() {
//     VkCommandBuffer &commandBuffer = computeCommandBuffers[currentFrame];

//     cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

//     cmdTransLayoutBarrier(commandBuffer, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT, VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT,
//         &mixRatio[currentFrame]); 
    
//     VkImageMemoryBarrier barrier{};
//         barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
//         barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
//         barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
//         barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
//         barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
//         barrier.image = scaled? lowresFrames[currentFrame].image : highresFrames[currentFrame].image;
//         barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
//         barrier.subresourceRange.baseMipLevel = 0;
//         barrier.subresourceRange.levelCount = 1;
//         barrier.subresourceRange.baseArrayLayer = 0;
//         barrier.subresourceRange.layerCount = 1;
//         barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT |VK_ACCESS_MEMORY_READ_BIT;
//         barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;

//     vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, accumulatePipe.line);
//         vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, accumulatePipe.lineLayout, 0, 1, &accumulatePipe.sets[currentFrame], 0, 0);
//         vkCmdDispatch(commandBuffer, (raytraceExtent.width+7)/8, (raytraceExtent.height+7)/8, 1);
//     cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, &barrier); 

//         barrier.image = scaled? lowresFrames[currentFrame].image : highresFrames[currentFrame].image;
//     cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, &barrier); 
//         barrier.image = mixRatio[currentFrame].image;
//     cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, &barrier); 
//     cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
// }
// void Renderer::upscale() {
//     if(!scaled) crash(no reason to upscale with 1:1 ratio?);

//     VkCommandBuffer &commandBuffer = computeCommandBuffers[currentFrame];

//     cmdTransLayoutBarrier(commandBuffer, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT, VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT,
//         &lowresFrames[currentFrame]); 
//     cmdTransLayoutBarrier(commandBuffer, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT, VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT,
//         &highresFrames[0]); 
        
//     vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, upscalePipe.line);
//         vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, upscalePipe.lineLayout, 0, 1, &upscalePipe.sets[currentFrame], 0, 0);
//         vkCmdDispatch(commandBuffer, (swapChainExtent.width+7)/8, (swapChainExtent.height+7)/8, 1);

// }
void Renderer::end_compute() {
    VkCommandBuffer &commandBuffer = rayGenCommandBuffers[currentFrame];
    VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = originBlockPalette[currentFrame].image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;    
    cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, &barrier); 
    cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, &barrier); 
        barrier.image = world.image;
    cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, &barrier); 
    cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, &barrier); 
        barrier.image = originBlockPalette[previousFrame].image;
    cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, &barrier); 
    cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, &barrier); 
    
    cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
    

        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = originBlockPalette[0].image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;    
    cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, &barrier); 
    cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, &barrier); 
        barrier.image = world.image;
    cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, &barrier); 
    cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, &barrier); 
        barrier.image = originBlockPalette[1].image;
    cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, &barrier); 
    cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, &barrier); 
    
    cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
    // VK_CHECK(vkEndCommandBuffer(commandBuffer));
        // vector<VkSemaphore> computeWaitSemaphores = {rayGenFinishedSemaphores[currentFrame]};
        // VkPipelineStageFlags computeWaitStages[] = {VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT};
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
void Renderer::start_ui() {
    
    //cause we render ui directly to swapchain image
    // VkResult result = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);
    VkResult result = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        // cout << KRED"failed to present swap chain image!\n" KEND;
        // recreate_SwapchainDependent();
        resized = true;
        // return; // can be avoided, but it is just 1 frame 
    } else if ((result != VK_SUCCESS)) {
        printf(KRED "failed to acquire swap chain image!\n" KEND);
        exit(result);
    }

    // vkWaitForFences(device, 1, &graphicalInFlightFences[currentFrame], VK_TRUE, UINT32_MAX);
    // vkResetFences  (device, 1, &graphicalInFlightFences[currentFrame]);

    // vkResetCommandBuffer(overlayCommandBuffers[currentFrame], 0);
    vkResetCommandBuffer(copyOverlayCommandBuffers[currentFrame], 0);
    // vkResetCommandBuffer(renderOverlayCommandBuffers[currentFrame], 0);
    VkCommandBufferBeginInfo begin_info = {};
        begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin_info.flags = 0;
        begin_info.pInheritanceInfo = NULL;
    // VK_CHECK(vkBeginCommandBuffer(overlayCommandBuffers[currentFrame], &begin_info));
    
    VkCommandBufferInheritanceInfo 
        cmd_buf_inheritance_info = {};
        cmd_buf_inheritance_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_INHERITANCE_INFO, cmd_buf_inheritance_info.pNext = NULL;
        cmd_buf_inheritance_info.occlusionQueryEnable = VK_FALSE;
        cmd_buf_inheritance_info.queryFlags = 0;
        cmd_buf_inheritance_info.pipelineStatistics = 0;

        cmd_buf_inheritance_info.renderPass = NULL;
        cmd_buf_inheritance_info.framebuffer = NULL;
        cmd_buf_inheritance_info.subpass = 0;
    begin_info.flags = 0;
    begin_info.pInheritanceInfo = &cmd_buf_inheritance_info;
    VK_CHECK(vkBeginCommandBuffer(copyOverlayCommandBuffers[currentFrame], &begin_info));
        cmd_buf_inheritance_info.renderPass = blur2presentRpass;
        cmd_buf_inheritance_info.framebuffer = overlayFramebuffers[imageIndex];
        cmd_buf_inheritance_info.subpass = 1;
        begin_info.flags = VK_COMMAND_BUFFER_USAGE_RENDER_PASS_CONTINUE_BIT;
        begin_info.pInheritanceInfo = &cmd_buf_inheritance_info;
    // VK_CHECK(vkBeginCommandBuffer(renderOverlayCommandBuffers[currentFrame], &begin_info));


    vkCmdNextSubpass(rayGenCommandBuffers[currentFrame], VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(rayGenCommandBuffers[currentFrame], VK_PIPELINE_BIND_POINT_GRAPHICS, overlayPipe.line);
    VkViewport viewport = {};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width  = (float)(swapChainExtent.width );
        viewport.height = (float)(swapChainExtent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
    vkCmdSetViewport(rayGenCommandBuffers[currentFrame], 0, 1, &viewport);

    ui_render_interface->last_scissors.offset = {0,0};
    ui_render_interface->last_scissors.extent = swapChainExtent;
    vkCmdSetScissor(rayGenCommandBuffers[currentFrame], 0, 1, &ui_render_interface->last_scissors);
}

void Renderer::draw_ui() {
    VkCommandBuffer &commandBuffer = rayGenCommandBuffers[currentFrame];

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
    

    // vklayout
    // cmdTransLayoutBarrier(commandBuffer, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT, 
        // scaled? &highresFrames[0] : &highresFrames[currentFrame]);    


        barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.image = swapchainImages[imageIndex].image;
    // cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, &barrier);
    // cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, &barrier);
    // barrier.newLayout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    // cmdTransLayoutBarrier(commandBuffer, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT , &swapchain_images[imageIndex]);    

    //copying done
    // VK_CHECK(vkEndCommandBuffer(renderOverlayCommandBuffers[currentFrame]));

    VkClearValue clearColor = {{{0.111f, 0.444f, 0.666f, 1.0f}}};
    VkRenderPassBeginInfo renderPassInfo = {};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = blur2presentRpass;
        renderPassInfo.framebuffer = overlayFramebuffers[imageIndex];
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = swapChainExtent;
        renderPassInfo.clearValueCount = 1;
        renderPassInfo.pClearValues = &clearColor;
        // renderPassInfo.
    
    // cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);


    // vkCmdExecuteCommands(commandBuffer, 1, &renderOverlayCommandBuffers[currentFrame]);
    
    vkCmdEndRenderPass(commandBuffer); //end of blur2present rpass

    //copies. Yeah little late but who cares about single overlay frame
    // vkCmdExecuteCommands(commandBuffer, 1, &copyOverlayCommandBuffers[currentFrame]);

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
        
        barrier.image = highresFrames[currentFrame].image;
    cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, &barrier); 
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.image = swapchainImages[imageIndex].image;
    cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, &barrier); 
    vkCmdBlitImage(commandBuffer, highresFrames[currentFrame].image, VK_IMAGE_LAYOUT_GENERAL, swapchainImages[imageIndex].image, VK_IMAGE_LAYOUT_GENERAL, 1, &blit, VK_FILTER_NEAREST);
        barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, &barrier); 
    cmdTransLayoutBarrier(commandBuffer, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT, VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT, &originBlockPalette[currentFrame]);
    VK_CHECK(vkEndCommandBuffer(commandBuffer));
    cmdPipelineBarrier(copyOverlayCommandBuffers[currentFrame], VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT); 
    VK_CHECK(vkEndCommandBuffer(copyOverlayCommandBuffers[currentFrame]));

    // vkQueueSubmit
}

void Renderer::present() {
    vector<VkSemaphore> signalSemaphores = {renderFinishedSemaphores[currentFrame]};
    
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
    // vector<VkCommandBuffer> commandBuffers = {rayGenCommandBuffers[currentFrame], computeCommandBuffers[currentFrame], overlayCommandBuffers[currentFrame]};
    vector<VkCommandBuffer> commandBuffers = {copyOverlayCommandBuffers[currentFrame], rayGenCommandBuffers[currentFrame]};
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

    VK_CHECK(vkQueueSubmit(graphicsQueue, 1, &submitInfo, frameInFlightFences[currentFrame]));
    VkResult result = vkQueuePresentKHR(presentQueue, &presentInfo);


    

    
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || resized) {
        resized = false; 
        // cout << KRED"failed to present swap chain image!\n" KEND;
        recreateSwapchainDependent();
    } else if (result != VK_SUCCESS) {
        cout << KRED"failed to present swap chain image!\n" KEND;
        exit(result);
    }
}

void Renderer::end_frame() {
    previousFrame = currentFrame;
     currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    nextFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    itime++;
    process_ui_deletion_queue();
}

void Renderer::cmdPipelineBarrier(VkCommandBuffer commandBuffer, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkImageMemoryBarrier* barrier) {
    vkCmdPipelineBarrier(
        commandBuffer,
        srcStageMask, dstStageMask,
        0,
        0, NULL,
        0, NULL,
        1, barrier
    );
}
void Renderer::cmdPipelineBarrier(VkCommandBuffer commandBuffer, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkBufferMemoryBarrier* barrier) {
    vkCmdPipelineBarrier(
        commandBuffer,
        srcStageMask, dstStageMask,
        0,
        0, NULL,
        1, barrier,
        0, NULL
    );
}
void Renderer::cmdPipelineBarrier(VkCommandBuffer commandBuffer, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask) {
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

static const char* get_layout_name(VkImageLayout layout) {
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
        Image* image) {

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
    uvec3 size, int mipmaps, VkSampleCountFlagBits sample_count) {
    (*images).resize(MAX_FRAMES_IN_FLIGHT);
    

    for (i32 i=0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        (*images)[i].aspect = aspect;
        (*images)[i].layout = VK_IMAGE_LAYOUT_UNDEFINED;

        VkImageCreateInfo imageInfo = {};
            imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            imageInfo.imageType = type;
            imageInfo.format = format;
            imageInfo.mipLevels = mipmaps;
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
            if(type == VK_IMAGE_TYPE_2D) {
                viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            } else if(type == VK_IMAGE_TYPE_3D) {
                viewInfo.viewType = VK_IMAGE_VIEW_TYPE_3D;
            }
            viewInfo.format = format;
            viewInfo.subresourceRange.aspectMask = aspect;
            viewInfo.subresourceRange.baseMipLevel = 0;
            viewInfo.subresourceRange.baseArrayLayer = 0;
            viewInfo.subresourceRange.levelCount = mipmaps;
            viewInfo.subresourceRange.layerCount = 1;
        VK_CHECK(vkCreateImageView(device, &viewInfo, NULL, &(*images)[i].view));

        // transition_Image_Layout_Singletime((*images)[i].image, format, aspect, VK_IMAGE_LAYOUT_UNDEFINED, layout,
        //     VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, pipeStage,
        //     0, access);
        // if(!(vma_usage & VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT)) {
        // }
    }
}
void Renderer::create_Image_Storages(Image* image, 
    VkImageType type, VkFormat format, VkImageUsageFlags usage, VmaMemoryUsage vma_usage, VmaAllocationCreateFlags vma_flags, VkImageAspectFlags aspect, 
    uvec3 size, int mipmaps, VkSampleCountFlagBits sample_count) {

    image->aspect = aspect;
    image->layout = VK_IMAGE_LAYOUT_UNDEFINED;

    VkImageCreateInfo imageInfo = {};
        imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
        imageInfo.imageType = type;
        imageInfo.format = format;
        imageInfo.mipLevels = mipmaps;
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
        if(type == VK_IMAGE_TYPE_2D) {
            viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        } else if(type == VK_IMAGE_TYPE_3D) {
            viewInfo.viewType = VK_IMAGE_VIEW_TYPE_3D;
        }
        viewInfo.format = format;
        viewInfo.subresourceRange.aspectMask = aspect;
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.levelCount = mipmaps;
        viewInfo.subresourceRange.layerCount = 1;
    VK_CHECK(vkCreateImageView(device, &viewInfo, NULL, &image->view));

    // transition_Image_Layout_Singletime(image->image, format, aspect, VK_IMAGE_LAYOUT_UNDEFINED, layout,
    //     VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT | VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT,
    //     0, access);
}

//fill manually with cmd or copy
void Renderer::create_Buffer_Storages(vector<Buffer>* buffers, VkBufferUsageFlags usage, u32 size, bool host) {
    (*buffers).resize(MAX_FRAMES_IN_FLIGHT);
    // allocs.resize(MAX_FRAMES_IN_FLIGHT);
    
    for (i32 i=0; i < MAX_FRAMES_IN_FLIGHT; i++) {
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

vector<Image> Renderer::create_RayTrace_VoxelImages(Voxel* voxels, ivec3 size) {
    VkDeviceSize bufferSize = (sizeof(Voxel))*size.x*size.y*size.z;

    vector<Image> voxelImages = {};

    create_Image_Storages(&voxelImages,
        VK_IMAGE_TYPE_3D,
        VK_FORMAT_R8_UINT,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        0, // no VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT
        VK_IMAGE_ASPECT_COLOR_BIT,
        size);
    for (int i=0; i<MAX_FRAMES_IN_FLIGHT; i++) {
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

    for(i32 i=0; i<MAX_FRAMES_IN_FLIGHT; i++) {
        copy_Buffer(stagingBuffer, &voxelImages[i], size);
    }
    vmaDestroyBuffer(VMAllocator, stagingBuffer, stagingAllocation);
    return voxelImages;
}
void Renderer::count_Descriptor(const VkDescriptorType type) {
    if(type == VK_DESCRIPTOR_TYPE_STORAGE_BUFFER) {
        STORAGE_BUFFER_DESCRIPTOR_COUNT++;
    } else if(type == VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER) {
        COMBINED_IMAGE_SAMPLER_DESCRIPTOR_COUNT++;
    } else if(type == VK_DESCRIPTOR_TYPE_STORAGE_IMAGE) {
        STORAGE_IMAGE_DESCRIPTOR_COUNT++;
    } else if(type == VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER) {
        UNIFORM_BUFFER_DESCRIPTOR_COUNT++;    
    } else if(type == VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT) {
        INPUT_ATTACHMENT_DESCRIPTOR_COUNT++;    
    } else {
        cout << KRED "ADD DESCRIPTOR TO COUNTER\n" KEND;
        abort();
    }
}
//in order
void Renderer::create_DescriptorSetLayout(vector<VkDescriptorType> descriptorTypes, VkShaderStageFlags stageFlags, VkDescriptorSetLayout* layout, VkDescriptorSetLayoutCreateFlags flags) {
    vector<VkDescriptorSetLayoutBinding> bindings = {};

    for (i32 i=0; i<descriptorTypes.size(); i++) {
        count_Descriptor(descriptorTypes[i]);
        
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
    VK_CHECK(vkCreateDescriptorSetLayout(device, &layoutInfo, NULL, layout));
}

void Renderer::createDescriptorPool() {
    printl(STORAGE_IMAGE_DESCRIPTOR_COUNT);
    printl(COMBINED_IMAGE_SAMPLER_DESCRIPTOR_COUNT);
    printl(STORAGE_BUFFER_DESCRIPTOR_COUNT);
    printl(UNIFORM_BUFFER_DESCRIPTOR_COUNT);

    VkDescriptorPoolSize STORAGE_IMAGE_PoolSize = {};
        STORAGE_IMAGE_PoolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        STORAGE_IMAGE_PoolSize.descriptorCount = STORAGE_IMAGE_DESCRIPTOR_COUNT*8 + 1;
    VkDescriptorPoolSize COMBINED_IMAGE_SAMPLER_PoolSize = {};
        COMBINED_IMAGE_SAMPLER_PoolSize.type = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        COMBINED_IMAGE_SAMPLER_PoolSize.descriptorCount = COMBINED_IMAGE_SAMPLER_DESCRIPTOR_COUNT*8 + 1;
    VkDescriptorPoolSize STORAGE_BUFFER_PoolSize = {};
        STORAGE_BUFFER_PoolSize.type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
        STORAGE_BUFFER_PoolSize.descriptorCount = STORAGE_BUFFER_DESCRIPTOR_COUNT*8 + 1;
    VkDescriptorPoolSize UNIFORM_BUFFER_PoolSize = {};
        UNIFORM_BUFFER_PoolSize.type = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
        UNIFORM_BUFFER_PoolSize.descriptorCount = UNIFORM_BUFFER_DESCRIPTOR_COUNT*8 + 1;
    VkDescriptorPoolSize INPUT_ATTACHMENT_PoolSize = {};
        INPUT_ATTACHMENT_PoolSize.type = VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT;
        INPUT_ATTACHMENT_PoolSize.descriptorCount = INPUT_ATTACHMENT_DESCRIPTOR_COUNT*8 + 1;
    
    vector<VkDescriptorPoolSize> poolSizes = {
        STORAGE_IMAGE_PoolSize, 
        COMBINED_IMAGE_SAMPLER_PoolSize, 
        // STORAGE_BUFFER_PoolSize,
        UNIFORM_BUFFER_PoolSize,
        INPUT_ATTACHMENT_PoolSize};

    VkDescriptorPoolCreateInfo poolInfo = {};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.poolSizeCount = poolSizes.size();
        poolInfo.pPoolSizes = poolSizes.data();
        poolInfo.maxSets = MAX_FRAMES_IN_FLIGHT*11; //becuase frames_in_flight multiply of 5 differents sets, each for shader 
    VK_CHECK(vkCreateDescriptorPool(device, &poolInfo, NULL, &descriptorPool));
}

static void allocate_Descriptor(vector<VkDescriptorSet>& sets, VkDescriptorSetLayout layout, VkDescriptorPool pool, VkDevice device) {
    sets.resize(MAX_FRAMES_IN_FLIGHT);
    vector<VkDescriptorSetLayout> layouts(MAX_FRAMES_IN_FLIGHT, layout);
    VkDescriptorSetAllocateInfo allocInfo = {};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.descriptorPool = pool;
        allocInfo.descriptorSetCount = layouts.size();
        allocInfo.pSetLayouts        = layouts.data();
    VK_CHECK(vkAllocateDescriptorSets(device, &allocInfo, sets.data()));
}

void Renderer::deferDescriptorsetup(VkDescriptorSetLayout* dsetLayout, vector<VkDescriptorSet>* descriptorSets, vector<DescriptorInfo> descriptions, VkShaderStageFlags stages) {
    if(*dsetLayout == VK_NULL_HANDLE) {
        vector<VkDescriptorType> descriptorTypes(descriptions.size());
        for(int i=0; i<descriptions.size(); i++) {
            descriptorTypes[i] = descriptions[i].type;
            // if(not descriptions[i].images.empty()) {
            //     if(descriptions[i].imageSampler != 0) {
            //         descriptorTypes[i] = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            //     } else {descriptorTypes[i] = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;}
            // } else if(not descriptions[i].buffers.empty()) {
            //     descriptorTypes[i] = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            // } else {crash(what is descriptor type?);}
        }
        create_DescriptorSetLayout(descriptorTypes, stages, dsetLayout);
    }
    
    DelayedDescriptorSetup delayed_setup = {dsetLayout, descriptorSets, descriptions, stages};
    delayed_descriptor_setups.push_back(delayed_setup);
}

void Renderer::setupDescriptor(VkDescriptorSetLayout* dsetLayout, vector<VkDescriptorSet>* descriptorSets, vector<DescriptorInfo> descriptions, VkShaderStageFlags stages) {
    for (int frame_i = 0; frame_i < MAX_FRAMES_IN_FLIGHT; frame_i++) {
        int previous_frame_i = frame_i - 1;
        if (previous_frame_i < 0) previous_frame_i = MAX_FRAMES_IN_FLIGHT-1; // so frames do not intersect

        vector<VkDescriptorImageInfo >  image_infos(descriptions.size());
        vector<VkDescriptorBufferInfo> buffer_infos(descriptions.size());
        vector<VkWriteDescriptorSet> writes(descriptions.size());

        for(int i=0; i<descriptions.size(); i++) {
            writes[i] = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            writes[i].dstSet = (*descriptorSets)[frame_i];
            writes[i].dstBinding = i;
            writes[i].dstArrayElement = 0;
            writes[i].descriptorCount = 1;
            writes[i].descriptorType = descriptions[i].type;

            int descriptor_frame_id = 0;
            switch (descriptions[i].relativePos) {
                case RD_CURRENT:{
                    descriptor_frame_id = frame_i;
                    break;
                }
                case RD_PREVIOUS:{
                    descriptor_frame_id = previous_frame_i;
                    break;
                }
                case RD_FIRST:{
                    descriptor_frame_id = 0;
                    break;
                }
                case RD_NONE:{
                    writes[i].descriptorCount = 0;
                    continue;
                    break;
                }
            }
            // printl(descriptions[i].images.size());
            
            if(not descriptions[i].images.empty()) {
                // printl(i)
                // printl(descriptor_frame_id)
                // printl(descriptions[i].images[descriptor_frame_id].view)
                // printl(descriptions[i].images.size())
                // printl(writes.size())

                image_infos[i].imageView = descriptions[i].images[descriptor_frame_id].view;
                image_infos[i].imageLayout = descriptions[i].imageLayout;
                if(descriptions[i].imageSampler != 0) {
                    image_infos[i].sampler = descriptions[i].imageSampler;
                } 

                
                writes[i].pImageInfo = &image_infos[i];
                assert(descriptions[i].buffers.empty());
                
            } else if(not descriptions[i].buffers.empty()) {
                buffer_infos[i].buffer = descriptions[i].buffers[descriptor_frame_id].buffer;
                buffer_infos[i].offset = 0;
                buffer_infos[i].range = VK_WHOLE_SIZE;
                writes[i].pBufferInfo = &buffer_infos[i];
            } else crash(what is descriptor type?);
        }

        vkUpdateDescriptorSets(device, writes.size(), writes.data(), 0, NULL);
    }
}

void Renderer::flushDescriptorSetup() {
    createDescriptorPool();

    // for(int i=0; i<delayed_descriptor_setups.size(); i++) {
    for(auto setup : delayed_descriptor_setups) {
        // if((*(setup.dsetLayout)) == VK_NULL_HANDLE) {
        allocate_Descriptor(*setup.sets, *setup.setLayout, descriptorPool, device);
        // }
        setupDescriptor(setup.setLayout, setup.sets, setup.descriptions, setup.stages);
    }
}

void Renderer::createSamplers() {
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
    samplerInfo.mipmapMode = VK_SAMPLER_MIPMAP_MODE_NEAREST;
    samplerInfo.mipLodBias = 0.0f;
    samplerInfo.minLod = 0.0f;
    samplerInfo.maxLod = 10.0f;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    // VK_CHECK(vkCreateSampler(device, &samplerInfo, NULL, &frameSampler));


        samplerInfo.magFilter = VK_FILTER_NEAREST;
        samplerInfo.minFilter = VK_FILTER_NEAREST;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    VK_CHECK(vkCreateSampler(device, &samplerInfo, NULL, &overlaySampler));
}

vector<char> read_Shader(const char* filename);

u32 Renderer::find_Memory_Type(u32 typeFilter, VkMemoryPropertyFlags properties) {
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

    cmdTransLayoutBarrier(commandBuffer, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 
        VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT, image);
    VkBufferImageCopy copyRegion = {};
    // copyRegion.size = size;
        copyRegion.imageExtent.width  = size.x;
        copyRegion.imageExtent.height = size.y;
        copyRegion.imageExtent.depth  = size.z;
        copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copyRegion.imageSubresource.layerCount = 1;
    vkCmdCopyBufferToImage(commandBuffer, srcBuffer, (*image).image, VK_IMAGE_LAYOUT_GENERAL, 1, &copyRegion);
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

void Renderer::transition_Image_Layout_Singletime(Image* image, VkImageLayout newLayout, int mipmaps) {
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
    barrier.subresourceRange.levelCount = mipmaps;
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

void Renderer::generateMipmaps(VkCommandBuffer commandBuffer, VkImage image, int32_t texWidth, int32_t texHeight, uint32_t mipLevels, VkImageAspectFlags aspect) {
    // VkCommandBuffer commandBuffer = begin_Single_Time_Commands();

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.image = image;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.subresourceRange.aspectMask = aspect;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.subresourceRange.levelCount = 1;

    int32_t mipWidth = texWidth;
    int32_t mipHeight = texHeight;

    for (uint32_t i = 1; i < mipLevels; i++) {
        barrier.subresourceRange.baseMipLevel = i - 1;
        barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_READ_BIT;

        vkCmdPipelineBarrier(commandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
            0, nullptr,
            0, nullptr,
            1, &barrier);
            VkImageBlit blit{};
        blit.srcOffsets[0] = { 0, 0, 0 };
        blit.srcOffsets[1] = { mipWidth, mipHeight, 1 };
        blit.srcSubresource.aspectMask = aspect;
        blit.srcSubresource.mipLevel = i - 1;
        blit.srcSubresource.baseArrayLayer = 0;
        blit.srcSubresource.layerCount = 1;
        blit.dstOffsets[0] = { 0, 0, 0 };
        blit.dstOffsets[1] = { mipWidth > 1 ? mipWidth / 2 : 1, mipHeight > 1 ? mipHeight / 2 : 1, 1 };
        blit.dstSubresource.aspectMask = aspect;
        blit.dstSubresource.mipLevel = i;
        blit.dstSubresource.baseArrayLayer = 0;
        blit.dstSubresource.layerCount = 1;
        vkCmdBlitImage(commandBuffer,
            image, VK_IMAGE_LAYOUT_GENERAL,
            image, VK_IMAGE_LAYOUT_GENERAL,
            1, &blit,
            (aspect==VK_IMAGE_ASPECT_COLOR_BIT)? VK_FILTER_LINEAR : VK_FILTER_NEAREST);
            
        barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

        vkCmdPipelineBarrier(commandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
            0, nullptr,
            0, nullptr,
            1, &barrier);
        
        if (mipWidth  > 1) mipWidth  /= 2;
        if (mipHeight > 1) mipHeight /= 2;
    }

    barrier.subresourceRange.baseMipLevel = mipLevels - 1;
    barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

    vkCmdPipelineBarrier(commandBuffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
        0, nullptr,
        0, nullptr,
        1, &barrier);

    // end_Single_Time_Commands(commandBuffer);
}

void Renderer::copy_Whole_Image(VkExtent3D extent, VkCommandBuffer cmdbuf, VkImage src, VkImage dst) {
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
tuple<int, int> get_block_xy(int N) {
    int x = N % BLOCK_PALETTE_SIZE_X;
    int y = N / BLOCK_PALETTE_SIZE_X;
    assert(y <= BLOCK_PALETTE_SIZE_Y);
    return tuple(x,y);
}
// void Renderer::load_vertices(Mesh* mesh, Vertex* vertices) {
//     crash("not implemented yet");
// }