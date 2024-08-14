#define VMA_IMPLEMENTATION
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#include "render.hpp" 

using namespace std;
using namespace glm;

tuple<int, int> get_block_xy(int N);

vector<char> read_Shader(const char* filename);

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

    //to fix weird mesh culling problems for identity
    update_camera();

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

    DEPTH_FORMAT = findSupportedFormat({DEPTH_FORMAT_PREFERED, DEPTH_FORMAT_SPARE}, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL, 
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT);

println
    //not worth abstracting
    createRenderPass1();
println
    // createRenderPass3();
    createRenderPassAlt();
    blur2presentRpass = altRpass;
    smoke2glossyRpass = altRpass;
    // lowresDepthStencil = highresDepthStencil;
println
    // createRenderPass2();
println
    createSamplers();

    printl(swapChainImageFormat)
    
    create_Command_Pool();
    create_Command_Buffers( &computeCommandBuffers, MAX_FRAMES_IN_FLIGHT);
    create_Command_Buffers(&graphicsCommandBuffers, MAX_FRAMES_IN_FLIGHT);
    create_Command_Buffers(    &copyCommandBuffers, MAX_FRAMES_IN_FLIGHT);
println

    createSwapchainDependent();

    createImages();
        
    setupDescriptors();

    createPipilines();
    
    create_Sync_Objects();

    gen_perlin_2d();
    gen_perlin_3d();
println
    assert(timestampCount!=0);
    timestampNames.resize(timestampCount);
    timestamps.resize(timestampCount);
    ftimestamps.resize(timestampCount);
    average_ftimestamps.resize(timestampCount);

    VkQueryPoolCreateInfo query_pool_info{};
        query_pool_info.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
        query_pool_info.queryType = VK_QUERY_TYPE_TIMESTAMP;
        query_pool_info.queryCount = timestampCount;
    
    queryPoolTimestamps.resize(MAX_FRAMES_IN_FLIGHT);
    for (auto &q : queryPoolTimestamps){
        VK_CHECK(vkCreateQueryPool(device, &query_pool_info, NULL, &q));    
    }
}

void Renderer::createImages(){
    create_Buffer_Storages(&stagingWorld,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        world_size.x*world_size.y*world_size.z*sizeof(BlockID_t), true);
    create_Image_Storages(&world,
        VK_IMAGE_TYPE_3D,
        VK_FORMAT_R16_SINT,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT,
        world_size); //TODO: dynamic
    create_Image_Storages(&radianceCache,
        VK_IMAGE_TYPE_3D,
        RADIANCE_FORMAT,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT,
        {world_size.x, world_size.y, world_size.z}); //TODO: dynamic
    create_Image_Storages(&originBlockPalette,
        VK_IMAGE_TYPE_3D,
        VK_FORMAT_R8_UINT,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT,
        // {16*BLOCK_PALETTE_SIZE_X, 16*BLOCK_PALETTE_SIZE_Y, 32}); //TODO: dynamic
        {16*BLOCK_PALETTE_SIZE_X, 16*BLOCK_PALETTE_SIZE_Y, 16}); //TODO: dynamic
    create_Image_Storages(&distancePalette,
        VK_IMAGE_TYPE_3D,
        VK_FORMAT_R8_UINT,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT,
        {16*BLOCK_PALETTE_SIZE_X, 16*BLOCK_PALETTE_SIZE_Y, 16}); //TODO: dynamic
    create_Image_Storages(&bitPalette,
        VK_IMAGE_TYPE_3D,
        VK_FORMAT_R8_UINT,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT,
        {(16/8)*BLOCK_PALETTE_SIZE_X, 16*BLOCK_PALETTE_SIZE_Y, 16}); //TODO: dynamic
    create_Image_Storages(&materialPalette,
        VK_IMAGE_TYPE_2D,
        VK_FORMAT_R32_SFLOAT, //try R32G32
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT,
        {6, 256, 1}); //TODO: dynamic, text formats, pack Material into smth other than 6 floats :)
        
    create_Image_Storages(&grassState,
        VK_IMAGE_TYPE_2D,
        VK_FORMAT_R16G16_SFLOAT,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        0,
        VK_IMAGE_ASPECT_COLOR_BIT,
        {world_size.x*2, world_size.y*2, 1}); //for quality
    transition_Image_Layout_Singletime(&grassState, VK_IMAGE_LAYOUT_GENERAL);
    create_Image_Storages(&waterState,
        VK_IMAGE_TYPE_2D,
        VK_FORMAT_R16G16_SFLOAT,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        0,
        VK_IMAGE_ASPECT_COLOR_BIT,
        {world_size.x*2, world_size.y*2, 1}); //for quality
    transition_Image_Layout_Singletime(&waterState, VK_IMAGE_LAYOUT_GENERAL);

    create_Image_Storages(&perlinNoise2d,
        VK_IMAGE_TYPE_2D,
        VK_FORMAT_R16G16_SNORM,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        0,
        VK_IMAGE_ASPECT_COLOR_BIT,
        {world_size.x, world_size.y, 1}); //does not matter than much
    transition_Image_Layout_Singletime(&perlinNoise2d, VK_IMAGE_LAYOUT_GENERAL);
    create_Image_Storages(&perlinNoise3d,
        VK_IMAGE_TYPE_3D,
        VK_FORMAT_R16G16B16A16_UNORM,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        0,
        VK_IMAGE_ASPECT_COLOR_BIT,
        {32, 32, 32}); //does not matter than much
    transition_Image_Layout_Singletime(&perlinNoise3d, VK_IMAGE_LAYOUT_GENERAL);

    create_Buffer_Storages(&gpuParticles,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        maxParticleCount*sizeof(Particle), true);
    // create_Buffer_Storages(&staging_uniform,
    //     VK_BUFFER_USAGE_TRANSFER_SRC_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
    //     sizeof(mat4)*2, true);
    create_Buffer_Storages(&uniform,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        sizeof(mat4), false);

    create_Buffer_Storages(&gpuRadianceUpdates,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        sizeof(ivec4)*world_size.x*world_size.y*world_size.z, false); //TODO test extra mem
    create_Buffer_Storages(&stagingRadianceUpdates,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        sizeof(ivec4)*world_size.x*world_size.y*world_size.z, true); //TODO test extra mem
    // staging_uniform_mapped.resize(MAX_FRAMES_IN_FLIGHT);
    stagingWorldMapped.resize(MAX_FRAMES_IN_FLIGHT);
    gpuParticlesMapped.resize(MAX_FRAMES_IN_FLIGHT);
    stagingRadianceUpdatesMapped.resize(MAX_FRAMES_IN_FLIGHT);
    for (int i=0; i<MAX_FRAMES_IN_FLIGHT; i++) {
        vmaMapMemory(VMAllocator, gpuParticles[i].alloc, &gpuParticlesMapped[i]);
        vmaMapMemory(VMAllocator, stagingWorld[i].alloc, &stagingWorldMapped[i]);
        vmaMapMemory(VMAllocator, stagingRadianceUpdates[i].alloc, &stagingRadianceUpdatesMapped[i]);
    }
    transition_Image_Layout_Singletime(&world, VK_IMAGE_LAYOUT_GENERAL);
    transition_Image_Layout_Singletime(&radianceCache, VK_IMAGE_LAYOUT_GENERAL);

    transition_Image_Layout_Singletime(&maskFrame, VK_IMAGE_LAYOUT_GENERAL);
    transition_Image_Layout_Singletime(&farDepth, VK_IMAGE_LAYOUT_GENERAL);
    transition_Image_Layout_Singletime(&nearDepth, VK_IMAGE_LAYOUT_GENERAL);
    if(scaled){
        transition_Image_Layout_Singletime(&lowresDepthStencil, VK_IMAGE_LAYOUT_GENERAL);
        transition_Image_Layout_Singletime(&lowresMatNorm, VK_IMAGE_LAYOUT_GENERAL);
    }
    // transition_Image_Layout_Singletime(&stencil, VK_IMAGE_LAYOUT_GENERAL);
    
    for (int i=0; i<MAX_FRAMES_IN_FLIGHT; i++) {
        transition_Image_Layout_Singletime(&originBlockPalette[i], VK_IMAGE_LAYOUT_GENERAL);
        transition_Image_Layout_Singletime(&bitPalette, VK_IMAGE_LAYOUT_GENERAL);
        transition_Image_Layout_Singletime(&distancePalette, VK_IMAGE_LAYOUT_GENERAL);
        transition_Image_Layout_Singletime(&materialPalette[i], VK_IMAGE_LAYOUT_GENERAL);
    }
}

void Renderer::setupDescriptors(){
    create_DescriptorSetLayout({
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, //in texture (ui)
        }, 
        VK_SHADER_STAGE_FRAGMENT_BIT, &overlayPipe.setLayout,
        VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR);

println
    deferDescriptorsetup(&radiancePipe.setLayout, &radiancePipe.sets, {
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, RD_FIRST  , {/*empty*/}, {world},              unnormNearest, VK_IMAGE_LAYOUT_GENERAL},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, RD_CURRENT, {/*empty*/}, (originBlockPalette), unnormNearest, VK_IMAGE_LAYOUT_GENERAL},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, RD_CURRENT, {/*empty*/}, (materialPalette),    NO_SAMPLER, VK_IMAGE_LAYOUT_GENERAL},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, RD_FIRST  , {/*empty*/}, {radianceCache},      unnormLinear, VK_IMAGE_LAYOUT_GENERAL},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, RD_FIRST  , {/*empty*/}, {radianceCache},      NO_SAMPLER, VK_IMAGE_LAYOUT_GENERAL},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER,RD_FIRST  , {gpuRadianceUpdates}, {},          NO_SAMPLER, NO_LAYOUT},
    }, VK_SHADER_STAGE_COMPUTE_BIT);
println
    deferDescriptorsetup(&diffusePipe.setLayout, &diffusePipe.sets, {
        {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, RD_FIRST, {/*empty*/}, {highresMatNorms}, NO_SAMPLER,     VK_IMAGE_LAYOUT_GENERAL},
        {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, RD_FIRST, {/*empty*/}, {highresDepthStencil},   NO_SAMPLER, VK_IMAGE_LAYOUT_GENERAL},
        // {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE         , RD_FIRST  , {/*empty*/}, {world},              NO_SAMPLER,     VK_IMAGE_LAYOUT_GENERAL},
        // {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE         , RD_FIRST, {/*empty*/}, {originBlockPalette}, NO_SAMPLER,     VK_IMAGE_LAYOUT_GENERAL},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE         , RD_FIRST, {/*empty*/}, {materialPalette},    NO_SAMPLER,     VK_IMAGE_LAYOUT_GENERAL},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER         , RD_FIRST  , {/*empty*/}, {radianceCache},      unnormLinear,     VK_IMAGE_LAYOUT_GENERAL},
    }, VK_SHADER_STAGE_FRAGMENT_BIT);
println

    deferDescriptorsetup(&fillStencilGlossyPipe.setLayout, &fillStencilGlossyPipe.sets, {
        {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, RD_FIRST, {/*empty*/}, {lowresMatNorm}, NO_SAMPLER,     VK_IMAGE_LAYOUT_GENERAL},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, RD_CURRENT, {/*empty*/}, (materialPalette),    NO_SAMPLER,     VK_IMAGE_LAYOUT_GENERAL},
    }, VK_SHADER_STAGE_FRAGMENT_BIT);
println
    deferDescriptorsetup(&fillStencilSmokePipe.setLayout, &fillStencilSmokePipe.sets, {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, RD_CURRENT, (uniform), {/*empty*/}, NO_SAMPLER, NO_LAYOUT},
    }, VK_SHADER_STAGE_VERTEX_BIT);
println
    deferDescriptorsetup(&glossyPipe.setLayout, &glossyPipe.sets, { 
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, RD_FIRST, {/*empty*/}, {lowresMatNorm}, NO_SAMPLER,     VK_IMAGE_LAYOUT_GENERAL},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, RD_FIRST, {/*empty*/}, {lowresDepthStencil},   linearSampler, VK_IMAGE_LAYOUT_GENERAL},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, RD_FIRST  , {/*empty*/}, {world},              unnormNearest,     VK_IMAGE_LAYOUT_GENERAL},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, RD_CURRENT, {/*empty*/}, (originBlockPalette), unnormNearest,     VK_IMAGE_LAYOUT_GENERAL},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, RD_CURRENT, {/*empty*/}, (materialPalette),    NO_SAMPLER,     VK_IMAGE_LAYOUT_GENERAL},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, RD_FIRST  , {/*empty*/}, {radianceCache},      unnormLinear,     VK_IMAGE_LAYOUT_GENERAL},
        // {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, RD_FIRST, {/*empty*/}, {distancePalette}, NO_SAMPLER, VK_IMAGE_LAYOUT_GENERAL},
        // {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, RD_FIRST, {/*empty*/}, {bitPalette}, NO_SAMPLER, VK_IMAGE_LAYOUT_GENERAL},
    }, VK_SHADER_STAGE_FRAGMENT_BIT);
println
    deferDescriptorsetup(&smokePipe.setLayout, &smokePipe.sets, {
        {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, RD_FIRST, {/*empty*/}, {farDepth}, NO_SAMPLER,     VK_IMAGE_LAYOUT_GENERAL},
        {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, RD_FIRST, {/*empty*/}, {nearDepth}, NO_SAMPLER,     VK_IMAGE_LAYOUT_GENERAL},
        // {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, RD_FIRST  , {/*empty*/}, {world},              NO_SAMPLER,     VK_IMAGE_LAYOUT_GENERAL},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, RD_FIRST  , {/*empty*/}, {radianceCache},      NO_SAMPLER,     VK_IMAGE_LAYOUT_GENERAL},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, RD_FIRST, {/*empty*/}, {perlinNoise3d}, linearSampler_tiled, VK_IMAGE_LAYOUT_GENERAL},
    }, VK_SHADER_STAGE_FRAGMENT_BIT);
println
    deferDescriptorsetup(&blurPipe.setLayout, &blurPipe.sets, { 
        {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, RD_FIRST, {/*empty*/}, {highresMatNorms}, NO_SAMPLER,     VK_IMAGE_LAYOUT_GENERAL},
        {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, RD_FIRST, {/*empty*/}, {highresFrames},  NO_SAMPLER,     VK_IMAGE_LAYOUT_GENERAL},
        // if(true)
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, RD_FIRST, {/*empty*/}, {maskFrame}, scaled? linearSampler : nearestSampler, VK_IMAGE_LAYOUT_GENERAL},
    }, VK_SHADER_STAGE_FRAGMENT_BIT);
println
        // create_DescriptorSetLayout({
        //     VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, // per-quad attributes
        //     }, 
        //     VK_SHADER_STAGE_VERTEX_BIT, &blocksPushLayout, 
        //     VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR);
    deferDescriptorsetup(&raygenBlocksPipe.setLayout, &raygenBlocksPipe.sets, {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, RD_CURRENT, (uniform), {/*empty*/}, NO_SAMPLER, NO_LAYOUT},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, RD_CURRENT, {}, {originBlockPalette}, unnormNearest, VK_IMAGE_LAYOUT_GENERAL},
    }, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    // setup_RayGen_Particles_Descriptors();
println
    deferDescriptorsetup(&raygenParticlesPipe.setLayout, &raygenParticlesPipe.sets, {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, RD_CURRENT , (uniform),   {/*empty*/},            NO_SAMPLER, NO_LAYOUT},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, RD_FIRST   , {/*empty*/}, {world},                NO_SAMPLER, VK_IMAGE_LAYOUT_GENERAL},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, RD_CURRENT , {/*empty*/}, (originBlockPalette), NO_SAMPLER, VK_IMAGE_LAYOUT_GENERAL},
    }, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT);


    deferDescriptorsetup(&updateGrassPipe.setLayout, &updateGrassPipe.sets, {
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, RD_FIRST, {}, {grassState}, NO_SAMPLER, VK_IMAGE_LAYOUT_GENERAL},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, RD_FIRST, {}, {perlinNoise2d}, linearSampler_tiled, VK_IMAGE_LAYOUT_GENERAL},
    }, VK_SHADER_STAGE_COMPUTE_BIT);

    deferDescriptorsetup(&updateWaterPipe.setLayout, &updateWaterPipe.sets, {
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, RD_FIRST, {}, {waterState}, NO_SAMPLER, VK_IMAGE_LAYOUT_GENERAL},
    }, VK_SHADER_STAGE_COMPUTE_BIT);

    deferDescriptorsetup(&raygenGrassPipe.setLayout, &raygenGrassPipe.sets, {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, RD_CURRENT , (uniform),   {/*empty*/},            NO_SAMPLER, NO_LAYOUT},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, RD_FIRST, {}, {grassState}, linearSampler, VK_IMAGE_LAYOUT_GENERAL},
    }, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT);

    deferDescriptorsetup(&raygenWaterPipe.setLayout, &raygenWaterPipe.sets, {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, RD_CURRENT , (uniform),   {/*empty*/},            NO_SAMPLER, NO_LAYOUT},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, RD_FIRST, {}, {waterState}, NO_SAMPLER, VK_IMAGE_LAYOUT_GENERAL},
    }, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT);

    deferDescriptorsetup(&genPerlin2dPipe.setLayout, &genPerlin2dPipe.sets, {
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, RD_FIRST, {}, {perlinNoise2d}, NO_SAMPLER, VK_IMAGE_LAYOUT_GENERAL},
    }, VK_SHADER_STAGE_COMPUTE_BIT);
    deferDescriptorsetup(&genPerlin3dPipe.setLayout, &genPerlin3dPipe.sets, {
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, RD_FIRST, {}, {perlinNoise3d}, NO_SAMPLER, VK_IMAGE_LAYOUT_GENERAL},
    }, VK_SHADER_STAGE_COMPUTE_BIT);

    create_DescriptorSetLayout({
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, // model voxels
        }, 
        VK_SHADER_STAGE_COMPUTE_BIT, &mapPushLayout, 
        VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR);
        // 0);
    deferDescriptorsetup(&mapPipe.setLayout, &mapPipe.sets, {
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, RD_FIRST, {}, {world}, NO_SAMPLER, VK_IMAGE_LAYOUT_GENERAL},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, RD_CURRENT, {}, originBlockPalette, NO_SAMPLER, VK_IMAGE_LAYOUT_GENERAL},
    }, VK_SHADER_STAGE_COMPUTE_BIT);


    deferDescriptorsetup(&dfxPipe.setLayout, &dfxPipe.sets, {
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, RD_FIRST, {}, {originBlockPalette}, NO_SAMPLER, VK_IMAGE_LAYOUT_GENERAL},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, RD_FIRST, {}, {distancePalette   }, NO_SAMPLER, VK_IMAGE_LAYOUT_GENERAL},
    }, VK_SHADER_STAGE_COMPUTE_BIT);
    deferDescriptorsetup(&dfyPipe.setLayout, &dfyPipe.sets, {
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, RD_FIRST, {}, {originBlockPalette}, NO_SAMPLER, VK_IMAGE_LAYOUT_GENERAL},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, RD_FIRST, {}, {distancePalette   }, NO_SAMPLER, VK_IMAGE_LAYOUT_GENERAL},
    }, VK_SHADER_STAGE_COMPUTE_BIT);
    deferDescriptorsetup(&dfzPipe.setLayout, &dfzPipe.sets, {
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, RD_FIRST, {}, {originBlockPalette}, NO_SAMPLER, VK_IMAGE_LAYOUT_GENERAL},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, RD_FIRST, {}, {distancePalette   }, NO_SAMPLER, VK_IMAGE_LAYOUT_GENERAL},
    }, VK_SHADER_STAGE_COMPUTE_BIT);
    deferDescriptorsetup(&bitmaskPipe.setLayout, &bitmaskPipe.sets, {
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, RD_FIRST, {}, {originBlockPalette}, NO_SAMPLER, VK_IMAGE_LAYOUT_GENERAL},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, RD_FIRST, {}, {bitPalette}, NO_SAMPLER, VK_IMAGE_LAYOUT_GENERAL},
    }, VK_SHADER_STAGE_COMPUTE_BIT);
println
    flushDescriptorSetup();
println
}

void Renderer::createPipilines(){
    raygenBlocksPipe.renderPass = raygen2diffuseRpass;
    raygenParticlesPipe.renderPass = raygen2diffuseRpass;
    raygenGrassPipe.renderPass = raygen2diffuseRpass;
    raygenWaterPipe.renderPass = raygen2diffuseRpass;
    diffusePipe.renderPass = raygen2diffuseRpass;
    
    fillStencilGlossyPipe.renderPass = smoke2glossyRpass;
    fillStencilSmokePipe.renderPass = smoke2glossyRpass;
    glossyPipe.renderPass = smoke2glossyRpass;
    smokePipe.renderPass = smoke2glossyRpass;

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
            {VK_FORMAT_R8G8B8_UINT, offsetof(PackedVoxelCircuit, pos)},
            // {VK_FORMAT_R8G8B8_SINT, offsetof(VoxelVertex, norm)},
            // {VK_FORMAT_R8_UINT, offsetof(PackedVoxelVertex, matID)},
        }, 
        sizeof(PackedVoxelCircuit), VK_VERTEX_INPUT_RATE_VERTEX, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        swapChainExtent, {NO_BLEND}, (sizeof(quat) + sizeof(vec4)*2), FULL_DEPTH_TEST, VK_CULL_MODE_NONE, NO_DISCARD, NO_STENCIL);

println
    raygenParticlesPipe.subpassId = 1;
    create_Raster_Pipeline(&raygenParticlesPipe, {
            {"shaders/compiled/rayGenParticlesVert.spv", VK_SHADER_STAGE_VERTEX_BIT}, 
            {"shaders/compiled/rayGenParticlesGeom.spv", VK_SHADER_STAGE_GEOMETRY_BIT},
            {"shaders/compiled/rayGenParticlesFrag.spv", VK_SHADER_STAGE_FRAGMENT_BIT},
        },{
            {VK_FORMAT_R32G32B32_SFLOAT, offsetof(Particle, pos)},
            {VK_FORMAT_R32G32B32_SFLOAT, offsetof(Particle, vel)},
            {VK_FORMAT_R32_SFLOAT, offsetof(Particle, lifeTime)},
            {VK_FORMAT_R8_UINT, offsetof(Particle, matID)},
        }, 
        sizeof(Particle), VK_VERTEX_INPUT_RATE_VERTEX, VK_PRIMITIVE_TOPOLOGY_POINT_LIST,
        swapChainExtent, {NO_BLEND}, 0, FULL_DEPTH_TEST, VK_CULL_MODE_NONE, NO_DISCARD, NO_STENCIL);
println
    raygenGrassPipe.subpassId = 2;
    create_Raster_Pipeline(&raygenGrassPipe, {
            {"shaders/compiled/grassVert.spv", VK_SHADER_STAGE_VERTEX_BIT}, 
            {"shaders/compiled/grassFrag.spv", VK_SHADER_STAGE_FRAGMENT_BIT},
        },{/*empty*/}, 
        0, VK_VERTEX_INPUT_RATE_VERTEX, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
        swapChainExtent, {NO_BLEND}, sizeof(vec4) + sizeof(int)*2 + sizeof(int)*2, FULL_DEPTH_TEST, VK_CULL_MODE_NONE, NO_DISCARD, NO_STENCIL);
    raygenWaterPipe.subpassId = 3;
println
    create_Raster_Pipeline(&raygenWaterPipe, {
            {"shaders/compiled/waterVert.spv", VK_SHADER_STAGE_VERTEX_BIT}, 
            {"shaders/compiled/grassFrag.spv", VK_SHADER_STAGE_FRAGMENT_BIT},
        },{/*empty*/}, 
        0, VK_VERTEX_INPUT_RATE_VERTEX, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
        swapChainExtent, {NO_BLEND}, sizeof(vec4) + sizeof(int)*2, FULL_DEPTH_TEST, VK_CULL_MODE_NONE, NO_DISCARD, NO_STENCIL);
println
    diffusePipe.subpassId = 4;
    create_Raster_Pipeline(&diffusePipe, {
            {"shaders/compiled/diffuseVert.spv", VK_SHADER_STAGE_VERTEX_BIT}, 
            {"shaders/compiled/diffuseFrag.spv", VK_SHADER_STAGE_FRAGMENT_BIT},
        },{/*fullscreen pass*/}, 
        0, VK_VERTEX_INPUT_RATE_VERTEX, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        swapChainExtent, {NO_BLEND}, sizeof(ivec4) + sizeof(vec4)*4, NO_DEPTH_TEST, VK_CULL_MODE_NONE, NO_DISCARD, NO_STENCIL);

println
    fillStencilGlossyPipe.subpassId = 0;
    create_Raster_Pipeline(&fillStencilGlossyPipe, {
            {"shaders/compiled/fillStencilGlossyVert.spv", VK_SHADER_STAGE_VERTEX_BIT}, 
            {"shaders/compiled/fillStencilGlossyFrag.spv", VK_SHADER_STAGE_FRAGMENT_BIT}, 
        },{/*fullscreen pass*/}, 
        0, VK_VERTEX_INPUT_RATE_VERTEX, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        swapChainExtent, {NO_BLEND}, 0, NO_DEPTH_TEST, VK_CULL_MODE_NONE, NO_DISCARD, {
            //sets 01 bit on mat.rough < 0.5 or smth similar 
            .failOp = VK_STENCIL_OP_REPLACE,
            .passOp = VK_STENCIL_OP_REPLACE,
            .depthFailOp = VK_STENCIL_OP_REPLACE,
            .compareOp = VK_COMPARE_OP_ALWAYS,
            .compareMask = 0b00,
            .writeMask = 0b01, //01 for reflection
            .reference = 0b01,
        });
println
    fillStencilSmokePipe.subpassId = 1;
    create_Raster_Pipeline(&fillStencilSmokePipe, {
            {"shaders/compiled/fillStencilSmokeVert.spv", VK_SHADER_STAGE_VERTEX_BIT}, 
            {"shaders/compiled/fillStencilSmokeFrag.spv", VK_SHADER_STAGE_FRAGMENT_BIT},
        },{/*passed as push constants lol*/}, 
        0, VK_VERTEX_INPUT_RATE_VERTEX, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        swapChainExtent, {BLEND_REPLACE_IF_GREATER, BLEND_REPLACE_IF_LESS}, sizeof(vec3)+sizeof(int)+sizeof(vec4), READ_DEPTH_TEST, VK_CULL_MODE_NONE, NO_DISCARD, {
            //sets 10 bit on rasterization 
            .failOp = VK_STENCIL_OP_KEEP,
            .passOp = VK_STENCIL_OP_REPLACE,
            .depthFailOp = VK_STENCIL_OP_KEEP,
            .compareOp = VK_COMPARE_OP_ALWAYS,
            .compareMask = 0b00,
            .writeMask = 0b10, //10 for smoke
            .reference = 0b10,
        });
println
    glossyPipe.subpassId = 2;
    create_Raster_Pipeline(&glossyPipe, {
            {"shaders/compiled/glossyVert.spv", VK_SHADER_STAGE_VERTEX_BIT}, 
            {"shaders/compiled/glossyFrag.spv", VK_SHADER_STAGE_FRAGMENT_BIT},
        },{/*fullscreen pass*/}, 
        0, VK_VERTEX_INPUT_RATE_VERTEX, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        swapChainExtent, {NO_BLEND}, sizeof(vec4) + sizeof(vec4), NO_DEPTH_TEST, VK_CULL_MODE_NONE, NO_DISCARD, {
            .failOp = VK_STENCIL_OP_KEEP,
            .passOp = VK_STENCIL_OP_KEEP,
            .depthFailOp = VK_STENCIL_OP_KEEP,
            .compareOp = VK_COMPARE_OP_EQUAL,
            // .compareOp = VK_COMPARE_OP_ALWAYS,
            .compareMask = 0b01,
            .writeMask = 0b00, //01 for glossy
            .reference = 0b01,
        });
println
    smokePipe.subpassId = 3;
    create_Raster_Pipeline(&smokePipe, {
            {"shaders/compiled/smokeVert.spv", VK_SHADER_STAGE_VERTEX_BIT}, 
            {"shaders/compiled/smokeFrag.spv", VK_SHADER_STAGE_FRAGMENT_BIT},
        },{/*fullscreen pass*/}, 
        0, VK_VERTEX_INPUT_RATE_VERTEX, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        swapChainExtent, {DO_BLEND}, sizeof(vec4) + sizeof(vec4), NO_DEPTH_TEST, VK_CULL_MODE_NONE, NO_DISCARD, {
            //sets 10 bit on rasterization 
            .failOp = VK_STENCIL_OP_KEEP,
            .passOp = VK_STENCIL_OP_KEEP,
            .depthFailOp = VK_STENCIL_OP_KEEP,
            .compareOp = VK_COMPARE_OP_EQUAL,
            .compareMask = 0b10,
            .writeMask = 0b00, //10 for smoke
            .reference = 0b10,
        });

println
    blurPipe.subpassId = 0+4;
    create_Raster_Pipeline(&blurPipe, {
            {"shaders/compiled/blurVert.spv", VK_SHADER_STAGE_VERTEX_BIT}, 
            {"shaders/compiled/blurFrag.spv", VK_SHADER_STAGE_FRAGMENT_BIT},
        },{/*fullscreen pass*/}, 
        0, VK_VERTEX_INPUT_RATE_VERTEX, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        swapChainExtent, {NO_BLEND}, 0, NO_DEPTH_TEST, VK_CULL_MODE_NONE, NO_DISCARD, NO_STENCIL);

println
    overlayPipe.subpassId = 1+4;
    create_Raster_Pipeline(&overlayPipe, {
            {"shaders/compiled/overlayVert.spv", VK_SHADER_STAGE_VERTEX_BIT}, 
            {"shaders/compiled/overlayFrag.spv", VK_SHADER_STAGE_FRAGMENT_BIT}, 
        },{
            {VK_FORMAT_R32G32_SFLOAT, offsetof(Rml::Vertex, position)},
            {VK_FORMAT_R8G8B8A8_UNORM, offsetof(Rml::Vertex, colour)},
            {VK_FORMAT_R32G32_SFLOAT, offsetof(Rml::Vertex, tex_coord)},
        }, 
        sizeof(Rml::Vertex), VK_VERTEX_INPUT_RATE_VERTEX, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
        swapChainExtent, {DO_BLEND}, sizeof(vec4)+sizeof(mat4), NO_DEPTH_TEST, VK_CULL_MODE_NONE, NO_DISCARD, NO_STENCIL);

println
    create_Compute_Pipeline(&radiancePipe,0, "shaders/compiled/radiance.spv", sizeof(int)*4,                  VK_PIPELINE_CREATE_DISPATCH_BASE_BIT);
println
    create_Compute_Pipeline(&updateGrassPipe,0, "shaders/compiled/updateGrass.spv", sizeof(vec2)*2 + sizeof(float), 0);
println
    // create_Compute_Pipeline(&updateWaterPipe,0, "shaders/compiled/updateWater.spv", sizeof(float) + sizeof(vec2)*2, 0);
println
    create_Compute_Pipeline(&genPerlin2dPipe,0, "shaders/compiled/perlin2.spv", 0, 0);
println
    create_Compute_Pipeline(&genPerlin3dPipe,0, "shaders/compiled/perlin3.spv", 0, 0);
println
    // create_Compute_Pipeline(&dfxPipe,0, "shaders/compiled/dfx.spv", 0, 0);
    // create_Compute_Pipeline(&dfyPipe,0, "shaders/compiled/dfy.spv", 0, 0);
    // create_Compute_Pipeline(&dfzPipe,0, "shaders/compiled/dfz.spv", 0, 0);
    // create_Compute_Pipeline(&bitmaskPipe,0, "shaders/compiled/bitmask.spv", 0, 0);
    create_Compute_Pipeline(&mapPipe, mapPushLayout, "shaders/compiled/map.spv",      sizeof(mat4) + sizeof(ivec4),   0);
println

}

void Renderer::createSwapchainDependent() {
    // int mipmaps;
    create_Image_Storages(&highresMatNorms,
        VK_IMAGE_TYPE_2D,
        MATNORM_FORMAT,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT,
        {swapChainExtent.width, swapChainExtent.height, 1});
    create_Image_Storages(&highresDepthStencil,
        VK_IMAGE_TYPE_2D,
        DEPTH_FORMAT,
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        VK_IMAGE_ASPECT_DEPTH_BIT|VK_IMAGE_ASPECT_STENCIL_BIT,
        {swapChainExtent.width, swapChainExtent.height, 1});
    create_Image_Storages(&highresFrames,
        VK_IMAGE_TYPE_2D,
        FRAME_FORMAT,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT|VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT,
        {swapChainExtent.width, swapChainExtent.height, 1});

    if(scaled){
        create_Image_Storages(&lowresMatNorm,
            VK_IMAGE_TYPE_2D,
            MATNORM_FORMAT,
            VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
            VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
            VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
            VK_IMAGE_ASPECT_COLOR_BIT,
            {raytraceExtent.width, raytraceExtent.height, 1});
        create_Image_Storages(&lowresDepthStencil,
            VK_IMAGE_TYPE_2D,
            DEPTH_FORMAT,
            VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
            VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
            VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
            VK_IMAGE_ASPECT_DEPTH_BIT|VK_IMAGE_ASPECT_STENCIL_BIT,
            {raytraceExtent.width, raytraceExtent.height, 1});
    } else {
        //set them to the same ptrs
        lowresMatNorm = highresMatNorms;
        lowresDepthStencil = highresDepthStencil;
    }

        VkImageViewCreateInfo viewInfo = {};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = lowresDepthStencil.image;
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = DEPTH_FORMAT;
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT; //create stencil view yourself
        viewInfo.subresourceRange.baseMipLevel = 0;
        viewInfo.subresourceRange.baseArrayLayer = 0;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.layerCount = 1;
        VK_CHECK(vkCreateImageView(device, &viewInfo, NULL, &stencilViewForDS));

    //required anyways
    create_Image_Storages(&maskFrame, //stencil for glossy&smoke
        VK_IMAGE_TYPE_2D,
        FRAME_FORMAT,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT,
        {raytraceExtent.width, raytraceExtent.height, 1});
    create_Image_Storages(&farDepth, //smoke depth
        VK_IMAGE_TYPE_2D,
        SECONDARY_DEPTH_FORMAT,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT,
        {raytraceExtent.width, raytraceExtent.height, 1});
    create_Image_Storages(&nearDepth, //smoke depth
        VK_IMAGE_TYPE_2D,
        SECONDARY_DEPTH_FORMAT,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT,
        {raytraceExtent.width, raytraceExtent.height, 1});

    vector<vector<VkImageView>> rayGenVeiws(3);
    for(int i=0; i<MAX_FRAMES_IN_FLIGHT; i++) {
        rayGenVeiws[0].push_back(highresMatNorms.view);
        rayGenVeiws[1].push_back(highresFrames.view);
        rayGenVeiws[2].push_back(highresDepthStencil.view);
    }

    vector<vector<VkImageView>> interVeiws(5);
    for(int i=0; i<MAX_FRAMES_IN_FLIGHT; i++) { //single framebuffer
        interVeiws[0].push_back(lowresMatNorm.view);
        interVeiws[1].push_back(maskFrame.view);
        interVeiws[2].push_back(stencilViewForDS);
        interVeiws[3].push_back(farDepth.view);
        interVeiws[4].push_back(nearDepth.view);
    }

    vector<vector<VkImageView>> blurVeiws(2);
    for(int i=0; i<MAX_FRAMES_IN_FLIGHT; i++) {
        blurVeiws[0].push_back(highresMatNorms.view);
        blurVeiws[1].push_back(highresFrames.view);
    }
    
    vector<vector<VkImageView>> altVeiws(5);
    for(int i=0; i<MAX_FRAMES_IN_FLIGHT; i++) {
        altVeiws[0].push_back(highresMatNorms.view);
        altVeiws[1].push_back(highresFrames.view);
        altVeiws[2].push_back(stencilViewForDS);
        altVeiws[3].push_back(farDepth.view);
        altVeiws[4].push_back(nearDepth.view);
        // blurVeiws[5].push_back(highresFrames.view);
    }
    
println
    create_N_Framebuffers(&rayGenFramebuffers, &rayGenVeiws, raygen2diffuseRpass, MAX_FRAMES_IN_FLIGHT, swapChainExtent.width, swapChainExtent.height);
println
//     create_N_Framebuffers(&glossyFramebuffers, &interVeiws, smoke2glossyRpass, MAX_FRAMES_IN_FLIGHT, raytraceExtent.width, raytraceExtent.height);
// println
//     create_N_Framebuffers(&overlayFramebuffers, &blurVeiws, blur2presentRpass, MAX_FRAMES_IN_FLIGHT, swapChainExtent.width, swapChainExtent.height);
println
    create_N_Framebuffers(&altFramebuffers, &altVeiws, altRpass, MAX_FRAMES_IN_FLIGHT, swapChainExtent.width, swapChainExtent.height);
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
    // create_Command_Buffers(&overlayCommandBuffers, MAX_FRAMES_IN_FLIGHT);
    create_Command_Buffers(&   graphicsCommandBuffers, MAX_FRAMES_IN_FLIGHT);
    create_Command_Buffers(&  computeCommandBuffers, MAX_FRAMES_IN_FLIGHT);

    VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = MAX_FRAMES_IN_FLIGHT;
    copyCommandBuffers.resize(MAX_FRAMES_IN_FLIGHT);
    VK_CHECK(vkAllocateCommandBuffers(device, &allocInfo, copyCommandBuffers.data())); 
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
	
    deleteImages(&radianceCache);
    deleteImages(&world);
    deleteImages(&originBlockPalette);
    deleteImages(&distancePalette);
    deleteImages(&bitPalette);
    deleteImages(&materialPalette);
    deleteImages(&perlinNoise2d);
    deleteImages(&grassState);
    deleteImages(&waterState);

    vkDestroySampler(device, nearestSampler, NULL);
    vkDestroySampler(device,  linearSampler, NULL);
    vkDestroySampler(device, overlaySampler, NULL);
    vkDestroySampler(device, linearSampler_tiled, NULL);

    for (int i=0; i<MAX_FRAMES_IN_FLIGHT; i++) {
            vmaUnmapMemory(VMAllocator,                           stagingWorld[i].alloc);
            vmaUnmapMemory(VMAllocator,                           gpuParticles[i].alloc);
            vmaUnmapMemory(VMAllocator,                           stagingRadianceUpdates[i].alloc);
        vmaDestroyBuffer(VMAllocator,  stagingWorld[i].buffer, stagingWorld[i].alloc);
        vmaDestroyBuffer(VMAllocator,  gpuParticles[i].buffer, gpuParticles[i].alloc);
        vmaDestroyBuffer(VMAllocator,  uniform[i].buffer, uniform[i].alloc);
        vmaDestroyBuffer(VMAllocator,  stagingRadianceUpdates[i].buffer, stagingRadianceUpdates[i].alloc);
    }
    vmaDestroyBuffer(VMAllocator,  gpuRadianceUpdates.buffer, gpuRadianceUpdates.alloc);

    vkDestroyDescriptorPool(device, descriptorPool, NULL);

    for (int i=0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroySemaphore(device,  imageAvailableSemaphores[i], NULL);
        vkDestroySemaphore(device,  renderFinishedSemaphores[i], NULL);
        vkDestroyFence(device,    frameInFlightFences[i], NULL);
        
        vkDestroyQueryPool(device, queryPoolTimestamps[i], NULL);
    }
    vkDestroyCommandPool(device, commandPool, NULL);
println
    vkDestroyRenderPass(device, raygen2diffuseRpass, NULL);
    vkDestroyRenderPass(device, blur2presentRpass, NULL);
println
    destroy_Compute_Pipeline(       &mapPipe);
    vkDestroyDescriptorSetLayout(device, mapPushLayout, NULL);
    destroy_Compute_Pipeline(  &raytracePipe);
    destroy_Compute_Pipeline(  &radiancePipe);
    destroy_Compute_Pipeline(   &updateGrassPipe);
    destroy_Compute_Pipeline(   &updateWaterPipe);
    destroy_Compute_Pipeline(   &genPerlin2dPipe);
    destroy_Compute_Pipeline(   &genPerlin3dPipe);
    destroy_Compute_Pipeline(   &dfxPipe);
    destroy_Compute_Pipeline(   &dfyPipe);
    destroy_Compute_Pipeline(   &dfzPipe);
    destroy_Compute_Pipeline(   &bitmaskPipe);

    destroy_Raster_Pipeline(&raygenBlocksPipe);
    destroy_Raster_Pipeline(&raygenParticlesPipe);
    destroy_Raster_Pipeline(&raygenGrassPipe);
    destroy_Raster_Pipeline(&raygenWaterPipe);
    destroy_Raster_Pipeline(&diffusePipe);
    destroy_Raster_Pipeline(&glossyPipe);
    destroy_Raster_Pipeline(&blurPipe);
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
    deleteImages(&highresDepthStencil);
    deleteImages(&highresFrames);    
    deleteImages(&highresMatNorms);

    
    for (auto framebuffer : rayGenFramebuffers) {
        vkDestroyFramebuffer(device, framebuffer, NULL);
    }
    // for (auto framebuffer : glossyFramebuffers) {
    //     vkDestroyFramebuffer(device, framebuffer, NULL);
    // }
    // for (auto framebuffer : overlayFramebuffers) {
    //     vkDestroyFramebuffer(device, framebuffer, NULL);
    // }
    for (auto framebuffer : altFramebuffers) {
        vkDestroyFramebuffer(device, framebuffer, NULL);
    }
    for (auto img : swapchainImages) {
        vkDestroyImageView(device, img.view, NULL);
    }
    vkDestroySwapchainKHR(device, swapchain, NULL);
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

void Renderer::process_ui_deletion_queue() {
    int write_index = 0;
    write_index = 0;
    for (int i=0; i<imageDeletionQueue.size(); i++) {
        bool should_keep = imageDeletionQueue[i].life_counter > 0;
        if(should_keep) {
            imageDeletionQueue[write_index] = imageDeletionQueue[i];
            imageDeletionQueue[write_index].life_counter -= 1;
            write_index++;
        }
        else {
            //free image
            vmaDestroyImage(VMAllocator, imageDeletionQueue[i].image.image, imageDeletionQueue[i].image.alloc);
            vkDestroyImageView(device, imageDeletionQueue[i].image.view, NULL);
        }
    }
    imageDeletionQueue.resize(write_index);

    write_index = 0;
    for (int i=0; i<bufferDeletionQueue.size(); i++) {
        bool should_keep = bufferDeletionQueue[i].life_counter > 0;
        if(should_keep) {
            bufferDeletionQueue[write_index] = bufferDeletionQueue[i];
            bufferDeletionQueue[write_index].life_counter -= 1;
            write_index++;
        }
        else {
            //free image
            vmaDestroyBuffer(VMAllocator, bufferDeletionQueue[i].buffer.buffer, bufferDeletionQueue[i].buffer.alloc);
        }
    }
    bufferDeletionQueue.resize(write_index);
}

void Renderer::gen_perlin_2d(){
    VkCommandBuffer commandBuffer = begin_Single_Time_Commands();

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = perlinNoise2d.image;
    barrier.subresourceRange.aspectMask = perlinNoise2d.aspect;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT;

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, genPerlin2dPipe.line);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, genPerlin2dPipe.lineLayout, 0, 1, &genPerlin2dPipe.sets[currentFrame], 0, 0);

    vkCmdDispatch(commandBuffer, world_size.x/8, world_size.y/8, 1);
    vkCmdPipelineBarrier(
        commandBuffer,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        0,
        0, NULL,
        0, NULL,
        1, &barrier
    );
    end_Single_Time_Commands(commandBuffer);
}

void Renderer::gen_perlin_3d(){
    VkCommandBuffer commandBuffer = begin_Single_Time_Commands();

    VkImageMemoryBarrier barrier{};
    barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
    barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    barrier.image = perlinNoise3d.image;
    barrier.subresourceRange.aspectMask = perlinNoise3d.aspect;
    barrier.subresourceRange.baseMipLevel = 0;
    barrier.subresourceRange.levelCount = 1;
    barrier.subresourceRange.baseArrayLayer = 0;
    barrier.subresourceRange.layerCount = 1;
    barrier.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT;
    barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT;

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, genPerlin3dPipe.line);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, genPerlin3dPipe.lineLayout, 0, 1, &genPerlin3dPipe.sets[currentFrame], 0, 0);

    vkCmdDispatch(commandBuffer, 64/4, 64/4, 64/4);
    vkCmdPipelineBarrier(
        commandBuffer,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        0,
        0, NULL,
        0, NULL,
        1, &barrier
    );
    end_Single_Time_Commands(commandBuffer);
}

void Renderer::update_camera(){
    dvec3 up = glm::dvec3(0.0f, 0.0f, 1.0f); // Up vector

    dmat4 view = glm::lookAt(cameraPos, cameraPos+cameraDir, up);
    const double voxel_in_pixels = 5.0; //we want voxel to be around 10 pixels in width / height
    const double  view_width_in_voxels = 1920.0 / voxel_in_pixels; //todo make dynamic and use spec constnats
    const double view_height_in_voxels = 1080.0 / voxel_in_pixels;
    dmat4 projection = glm::ortho(-view_width_in_voxels/2.0, view_width_in_voxels/2.0, view_height_in_voxels/2.0, -view_height_in_voxels/2.0, -0.0, +2000.0); // => *100.0 decoding
    dmat4     worldToScreen = projection * view;

    cameraTransform_OLD = cameraTransform;
    cameraTransform = worldToScreen;
}

// #include <glm/gtx/string_cast.hpp>
void Renderer::start_frame() {
    update_camera();
    
    vkWaitForFences(device, 1, &frameInFlightFences[currentFrame], VK_TRUE, UINT32_MAX);
    vkResetFences  (device, 1, &frameInFlightFences[currentFrame]);

    vkResetCommandBuffer(computeCommandBuffers[currentFrame], /*VkCommandBufferResetFlagBits*/ 0);
    vkResetCommandBuffer(graphicsCommandBuffers[currentFrame], /*VkCommandBufferResetFlagBits*/ 0);
    vkResetCommandBuffer(copyCommandBuffers[currentFrame], /*VkCommandBufferResetFlagBits*/ 0);

    VkCommandBufferBeginInfo beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = 0;
        beginInfo.pInheritanceInfo = NULL;
    VK_CHECK(vkBeginCommandBuffer(computeCommandBuffers[currentFrame], &beginInfo));
    VK_CHECK(vkBeginCommandBuffer(graphicsCommandBuffers[currentFrame], &beginInfo));
    VK_CHECK(vkBeginCommandBuffer(copyCommandBuffers[currentFrame], &beginInfo));
}

void Renderer::start_compute() {
    VkCommandBuffer &commandBuffer = computeCommandBuffers[currentFrame];
  
    // cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
    // #ifdef MEASURE_PERFOMANCE
    vkCmdResetQueryPool(commandBuffer, queryPoolTimestamps[currentFrame], 0, timestampCount);
    // #endif
    currentTimestamp = 0;
}

void Renderer::start_blockify() {
    VkCommandBuffer &commandBuffer = computeCommandBuffers[currentFrame];

    blockCopyQueue.clear();

    paletteCounter = static_block_palette_size;
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
        //zero clear TODO
        int current_block = current_world(xx,yy,zz); 
        // if (current_block == 0) { //if not allocated
        //     current_world(xx,yy,zz) = palette_counter;
        //     palette_counter++;
        // } else {
            if(current_block < static_block_palette_size) { // static
                //add to copy queue
                ivec2 src_block = {}; 
                tie(src_block.x, src_block.y) = get_block_xy(current_block);
                ivec2 dst_block = {}; 
                tie(dst_block.x, dst_block.y) = get_block_xy(paletteCounter);
                    // static_block_copy.
                    
                    // if(current_block == 0){
                        // VkImageSubresourceRange
                        //     static_block_clear = {};
                        //     static_block_clear.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                        //     static_block_clear.srcSubresource.baseArrayLayer = 0;
                        //     static_block_clear.srcSubresource.layerCount = 1;
                        //     static_block_clear.srcSubresource.mipLevel = 0;
                        //     static_block_clear.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                        //     static_block_clear.dstSubresource.baseArrayLayer = 0;
                        //     static_block_clear.dstSubresource.layerCount = 1;
                        //     static_block_clear.dstSubresource.mipLevel = 0;
                        //     static_block_clear.extent = {16,16,16};
                        //     static_block_clear.srcOffset = {src_block.x*16, src_block.y*16, 0};
                        //     static_block_clear.dstOffset = {dst_block.x*16, dst_block.y*16, 0};
                        // blockCopyQueue.push_back(static_block_copy);
                    // } else {
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
                        blockCopyQueue.push_back(static_block_copy);
                    // }



                    // printf("pc %d ", palette_counter);
                    current_world(xx,yy,zz) = paletteCounter;
                    paletteCounter++; // yeah i dont trust myself in this  
            } else {
                //already new block, just leave it
            }
        // } //TODO copy empty instead of whole image
    }}}
}
void Renderer::end_blockify() {
    // vkDeviceWaitIdle(device);
    size_t size_to_copy = world_size.x * world_size.y * world_size.z * sizeof(BlockID_t);
    assert(size_to_copy != 0);
    memcpy(stagingWorldMapped[currentFrame], current_world.data(), size_to_copy);
    vmaFlushAllocation(VMAllocator, stagingWorld[currentFrame].alloc, 0, size_to_copy);
    // printf("static_block_palette_size %d\n", static_block_palette_size);
    // for(int zz = 0; zz < world_size.z; zz++) {
    // for(int yy = 0; yy < world_size.y; yy++) {
    // for(int xx = 0; xx < world_size.x; xx++) {
    //     int block_id = current_world(xx,yy,zz);

    //     assert(block_id <= paletteCounter);
    //     assert(block_id >= 0);
    //     // printf("%d ", block_id);
    // }
    // }
    // }
}
void Renderer::blockify_custom(void* ptr) {
    size_t size_to_copy = world_size.x * world_size.y * world_size.z * sizeof(BlockID_t);
    memcpy(stagingWorldMapped[currentFrame], ptr, size_to_copy);
    vmaFlushAllocation(VMAllocator, stagingWorld[currentFrame].alloc, 0, size_to_copy);
}

void Renderer::update_radiance() {
    VkCommandBuffer &commandBuffer = computeCommandBuffers[currentFrame];
    
    // cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

    radianceUpdates.clear();

    for(int xx = 0; xx < world_size.x; xx++) {
    for(int yy = 0; yy < world_size.y; yy++) {
    for(int zz = 0; zz < world_size.z; zz++) {
        // int block_id = current_world(xx,yy,zz);
        //yeah kinda slow... but who cares on less then a million blocks?
        int sum_of_neighbours = 0;
        for(int dx=-1; dx<=+1; dx++) {
        for(int dy=-1; dy<=+1; dy++) {
        for(int dz=-1; dz<=+1; dz++) {
            sum_of_neighbours += current_world(xx+dx,yy+dy,zz+dz); 
        }}}
        // TODO: finish dynamic update system, integrate with RaVE
        if(sum_of_neighbours != 0) {
            radianceUpdates.push_back(ivec4(xx,yy,zz,0));
        }
    }}}
    // printl(radianceUpdates.size())

    VkDeviceSize bufferSize = sizeof(ivec4)*radianceUpdates.size();
    memcpy(stagingRadianceUpdatesMapped[currentFrame], radianceUpdates.data(), bufferSize);
    
    // cmdPipelineBarrier(commandBuffer, 
    //     VK_PIPELINE_STAGE_HOST_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
    //     VK_ACCESS_HOST_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT,
    //     stagingRadianceUpdates[currentFrame]);
    // cmdPipelineBarrier(commandBuffer, 
    //     VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
    //     VK_ACCESS_MEMORY_WRITE_BIT|VK_ACCESS_MEMORY_READ_BIT, VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT,
    //     gpuRadianceUpdates);

    VkBufferCopy 
        copyRegion = {};
        copyRegion.size = bufferSize;
    vkCmdCopyBuffer(commandBuffer, stagingRadianceUpdates[currentFrame].buffer, gpuRadianceUpdates.buffer, 1, &copyRegion);

    cmdPipelineBarrier(commandBuffer, 
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
        gpuRadianceUpdates);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, radiancePipe.line);

        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, radiancePipe.lineLayout, 0, 1, &radiancePipe.sets[currentFrame], 0, 0);

        int magic_number = iFrame % 2;
        //if fast than increase work
        if(timeTakenByRadiance < 1.8){
            magicSize --;
        //if slow than decrease work
        } else if(timeTakenByRadiance > 2.2) {
            magicSize ++;
        }
        magicSize = glm::max(magicSize,1); //never remove
        magicSize = glm::min(magicSize,10);

        cameraPos_OLD = cameraPos;
        cameraDir_OLD = cameraDir;
        struct rtpc {int time, iters, size, shift;} pushconstant = {iFrame, 0, magicSize, iFrame % magicSize};
        vkCmdPushConstants(commandBuffer, radiancePipe.lineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pushconstant), &pushconstant);

        int wg_count = radianceUpdates.size() / magicSize;
        
        PLACE_TIMESTAMP_ALWAYS();
    vkCmdDispatch(commandBuffer, wg_count, 1, 1);
        PLACE_TIMESTAMP_ALWAYS();

    cmdPipelineBarrier(commandBuffer, 
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_WRITE_BIT,
        radianceCache);
    cmdPipelineBarrier(commandBuffer, 
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
        radianceCache);
}

void Renderer::recalculate_df(){
    VkCommandBuffer &commandBuffer = computeCommandBuffers[currentFrame];

    VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = distancePalette.image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;   
    vkCmdPipelineBarrier(
        commandBuffer,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0,
        0, NULL,
        0, NULL,
        1, &barrier
    );

    // vkCmdPipelineBarrier(
    //     commandBuffer,
    //     VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
    //     0,
    //     0, NULL,
    //     0, NULL,
    //     1, &barrier
    // );
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, dfxPipe.line);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, dfxPipe.lineLayout, 0, 1, &dfxPipe.sets[currentFrame], 0, 0);
        vkCmdDispatch(commandBuffer, 1, 1, 16*paletteCounter);
        vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            0,
            0, NULL,
            0, NULL,
            1, &barrier
        );
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, dfyPipe.line);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, dfyPipe.lineLayout, 0, 1, &dfyPipe.sets[currentFrame], 0, 0);
        vkCmdDispatch(commandBuffer, 1, 1, 16*paletteCounter);
        vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            0,
            0, NULL,
            0, NULL,
            1, &barrier
        ); 
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, dfzPipe.line);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, dfzPipe.lineLayout, 0, 1, &dfzPipe.sets[currentFrame], 0, 0);
        vkCmdDispatch(commandBuffer, 1, 1, 16*paletteCounter);
        vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            0,
            0, NULL,
            0, NULL,
            1, &barrier
        );
}
void Renderer::recalculate_bit(){
    VkCommandBuffer &commandBuffer = computeCommandBuffers[currentFrame];

    VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = bitPalette.image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.baseMipLevel = 0;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.baseArrayLayer = 0;
        barrier.subresourceRange.layerCount = 1;
        barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT;
        barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;   
    vkCmdPipelineBarrier(
        commandBuffer,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0,
        0, NULL,
        0, NULL,
        1, &barrier
    );

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, bitmaskPipe.line);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, bitmaskPipe.lineLayout, 0, 1, &bitmaskPipe.sets[currentFrame], 0, 0);
        vkCmdDispatch(commandBuffer, 1, 1, 8*paletteCounter);
        vkCmdPipelineBarrier(
            commandBuffer,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            0,
            0, NULL,
            0, NULL,
            1, &barrier
        );
}

void Renderer::exec_copies() {
    VkCommandBuffer &commandBuffer = computeCommandBuffers[currentFrame];

    // cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);

    if(blockCopyQueue.size() != 0) {
        //we can copy from previous image, cause static blocks are same in both palettes. Additionaly, it gives src and dst optimal layouts
        cmdTransLayoutBarrier(commandBuffer, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, 
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 
            VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT , VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT, 
            &originBlockPalette[previousFrame]);
        cmdTransLayoutBarrier(commandBuffer, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, 
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 
            VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT , VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT, 
            &originBlockPalette[ currentFrame]);

        PLACE_TIMESTAMP();
        vkCmdCopyImage(commandBuffer, originBlockPalette[previousFrame].image, VK_IMAGE_LAYOUT_GENERAL, originBlockPalette[currentFrame].image, VK_IMAGE_LAYOUT_GENERAL, blockCopyQueue.size(), blockCopyQueue.data());
        PLACE_TIMESTAMP();

        cmdTransLayoutBarrier(commandBuffer, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, 
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 
            VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT , VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT, 
            &originBlockPalette[previousFrame]);
        cmdTransLayoutBarrier(commandBuffer, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL, 
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 
            VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT , VK_ACCESS_MEMORY_READ_BIT|VK_ACCESS_MEMORY_WRITE_BIT, 
            &originBlockPalette[ currentFrame]);    
    }
    VkBufferImageCopy copyRegion = {};
    // copyRegion.size = size;
        copyRegion.imageExtent.width  = world_size.x;
        copyRegion.imageExtent.height = world_size.y;
        copyRegion.imageExtent.depth  = world_size.z;
        copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copyRegion.imageSubresource.layerCount = 1;
        
    cmdTransLayoutBarrier(commandBuffer, VK_IMAGE_LAYOUT_GENERAL, 
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 
        VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_TRANSFER_WRITE_BIT, 
        &world);
    
    vkCmdCopyBufferToImage(commandBuffer, stagingWorld[currentFrame].buffer, world.image, VK_IMAGE_LAYOUT_GENERAL, 1, &copyRegion);

    cmdTransLayoutBarrier(commandBuffer, VK_IMAGE_LAYOUT_GENERAL,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 
        VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, 
        &world);
}

void Renderer::start_map() {
    VkCommandBuffer &commandBuffer = computeCommandBuffers[currentFrame];
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, mapPipe.line);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, mapPipe.lineLayout, 0, 1, &mapPipe.sets[currentFrame], 0, 0);
        PLACE_TIMESTAMP();
}

void Renderer::map_mesh(Mesh* mesh) {
    VkCommandBuffer &commandBuffer = computeCommandBuffers[currentFrame];    
        VkDescriptorImageInfo
            modelVoxelsInfo = {};
            modelVoxelsInfo.imageView = (*mesh).voxels[currentFrame].view; //CHANGE ME
            modelVoxelsInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;

        VkWriteDescriptorSet 
            modelVoxelsWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
            modelVoxelsWrite.dstSet = NULL;
            modelVoxelsWrite.dstBinding = 0;
            modelVoxelsWrite.dstArrayElement = 0;
            modelVoxelsWrite.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
            modelVoxelsWrite.descriptorCount = 1;
            modelVoxelsWrite.pImageInfo = &modelVoxelsInfo;
        vector<VkWriteDescriptorSet> descriptorWrites = {modelVoxelsWrite};

    vkCmdPushDescriptorSetKHR(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, mapPipe.lineLayout, 1, descriptorWrites.size(), descriptorWrites.data());

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
    VkCommandBuffer &commandBuffer = computeCommandBuffers[currentFrame];

    // cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
    cmdTransLayoutBarrier(commandBuffer, VK_IMAGE_LAYOUT_GENERAL, 
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 
        VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT, 
        &originBlockPalette[currentFrame]);
        PLACE_TIMESTAMP();
}

void Renderer::end_compute() {
    VkCommandBuffer &commandBuffer = computeCommandBuffers[currentFrame];
}

void Renderer::start_raygen() {
    VkCommandBuffer &commandBuffer = graphicsCommandBuffers[currentFrame];
        PLACE_TIMESTAMP();
    
    struct unicopy {mat4 trans;} unicopy = {cameraTransform};
    vkCmdUpdateBuffer(commandBuffer, uniform[currentFrame].buffer, 0, sizeof(unicopy), &unicopy);
    cmdPipelineBarrier(commandBuffer, 
                VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
                VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT,
                uniform[currentFrame]);

    VkClearValue clear_depth = {};
    clear_depth.depthStencil.depth = 1;
    vector<VkClearValue> clearColors = {
        {}, 
        {}, 
        clear_depth
    };
    VkRenderPassBeginInfo renderPassInfo = {};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = raygen2diffuseRpass;
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

#define CHECK_N_DRAW(__norm, __dir) \
if(is_face_visible(mesh->rot*__norm, cameraDir)) {\
    draw_face_helper(__norm, (*mesh).triangles.__dir, block_id);\
}

void Renderer::draw_face_helper(vec3 normal, IndexedVertices& buff, int block_id){
    VkCommandBuffer &commandBuffer = graphicsCommandBuffers[currentFrame];

    vkCmdBindIndexBuffer(commandBuffer, buff.indexes[currentFrame].buffer, 0, VK_INDEX_TYPE_UINT16);
        VkDeviceSize offsets[] = {0};
    struct {vec3 n; float id;} nid = {normal, float(block_id)};
    vkCmdPushConstants(commandBuffer, raygenBlocksPipe.lineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 32, sizeof(nid), &nid);
    vkCmdDrawIndexed(commandBuffer, buff.icount, 1, 0, 0, 0);
}

void Renderer::raygen_mesh(Mesh *mesh, int block_id) {
    VkCommandBuffer &commandBuffer = graphicsCommandBuffers[currentFrame];

        VkBuffer vertexBuffers[] = {(*mesh).triangles.vertexes[currentFrame].buffer};
        VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
    
    //TODO:
    struct {quat r1; vec4 s1;} raygen_pushconstant = {(*mesh).rot, vec4((*mesh).shift,0)};
    vkCmdPushConstants(commandBuffer, raygenBlocksPipe.lineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(raygen_pushconstant), &raygen_pushconstant);

    (*mesh).old_rot   = (*mesh).rot;
    (*mesh).old_shift = (*mesh).shift;


    // glm::mult
    // if(old_buff != (*mesh).indexes[currentFrame].buffer) {
    // vkCmdBindVertexBuffers(commandBuffer, 0, 1, vertexBuffers, offsets);
    // printl((*mesh).triangles.Pzz.icount);
    // if(is_face_visible(mesh->rot*vec3(+1,0,0), cameraDir)) {
    //     draw_face_helper(vec3(+1,0,0), (*mesh).triangles.Pzz);
    // }
    CHECK_N_DRAW(vec3(+1,0,0), Pzz);
    CHECK_N_DRAW(vec3(-1,0,0), Nzz);
    CHECK_N_DRAW(vec3(0,+1,0), zPz);
    CHECK_N_DRAW(vec3(0,-1,0), zNz);
    CHECK_N_DRAW(vec3(0,0,+1), zzP);
    CHECK_N_DRAW(vec3(0,0,-1), zzN);
        // old_buff = (*mesh).indexes[currentFrame].buffer;
    // }
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

void Renderer::raygen_map_particles() {
    VkCommandBuffer &commandBuffer = graphicsCommandBuffers[currentFrame];

    //go to next no matter what
    vkCmdNextSubpass(commandBuffer, VK_SUBPASS_CONTENTS_INLINE);
    
        PLACE_TIMESTAMP();
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
        PLACE_TIMESTAMP();
}
void Renderer::raygen_start_grass(){
    VkCommandBuffer &commandBuffer = graphicsCommandBuffers[currentFrame];

        PLACE_TIMESTAMP();
    vkCmdNextSubpass(commandBuffer, VK_SUBPASS_CONTENTS_INLINE);
    
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
//DOES NOT run in renderpass. Placed here cause spatially makes sense
void Renderer::updade_grass(vec2 windDirection){
    VkCommandBuffer &commandBuffer = computeCommandBuffers[currentFrame];

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, updateGrassPipe.line);
    struct {vec2 wd, cp; float dt;} pushconstant = {windDirection, {}, float(iFrame)};
    vkCmdPushConstants(commandBuffer, updateGrassPipe.lineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(pushconstant), &pushconstant);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, updateGrassPipe.lineLayout, 0, 1, &updateGrassPipe.sets[0], 0, 0);

    cmdPipelineBarrier(commandBuffer, 
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_WRITE_BIT,
        grassState);
    cmdPipelineBarrier(commandBuffer, 
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
        grassState);

        PLACE_TIMESTAMP();
    vkCmdDispatch(commandBuffer, (world_size.x*2 +7)/8, (world_size.y*2 +7)/8, 1); //2x8 2x8 1x1
        PLACE_TIMESTAMP();
}
//blade is hardcoded but it does not really increase perfomance
//done this way for simplicity, can easilly be replaced
void Renderer::raygen_map_grass(vec4 shift, int size){
    VkCommandBuffer &commandBuffer = graphicsCommandBuffers[currentFrame];

    bool x_flip = cameraDir.x < 0;
    bool y_flip = cameraDir.y < 0;
    struct {vec4 _shift; int _size, _time; int xf, yf;} raygen_pushconstant = {shift, size, iFrame, x_flip, y_flip};
    vkCmdPushConstants(commandBuffer, raygenGrassPipe.lineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(raygen_pushconstant), &raygen_pushconstant);

    // const int verts_per_blade = 4*6 + 3; //for triangle list
    // const int verts_per_blade = 11+3; //for triangle strip
    // const int blade_per_instance = 2; //for triangle strip
    const int verts_per_blade = 11; //for triangle strip
    const int blade_per_instance = 1; //for triangle strip
    vkCmdDraw(commandBuffer, 
        verts_per_blade*blade_per_instance, 
        (size*size + (blade_per_instance-1))/blade_per_instance, 
        0, 0);
}

// void Renderer::updade_water(vec2 windDirection, vec2 collsionPoint){

// }

void Renderer::raygen_start_water(){
    VkCommandBuffer &commandBuffer = graphicsCommandBuffers[currentFrame];

        PLACE_TIMESTAMP();
        PLACE_TIMESTAMP();
    vkCmdNextSubpass(commandBuffer, VK_SUBPASS_CONTENTS_INLINE);
    
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, raygenWaterPipe.line);
    vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, raygenWaterPipe.lineLayout, 0, 1, &raygenWaterPipe.sets[currentFrame], 0, 0);

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

void Renderer::raygen_map_water(vec4 shift, int size){
    VkCommandBuffer &commandBuffer = graphicsCommandBuffers[currentFrame];

    struct {vec4 _shift; int _size, _time;} raygen_pushconstant = {shift, size, iFrame};
    vkCmdPushConstants(commandBuffer, raygenWaterPipe.lineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(raygen_pushconstant), &raygen_pushconstant);

    const int verts_per_water_tape = 32;
    const int tapes_per_block = 16;
    vkCmdDraw(commandBuffer, 
        verts_per_water_tape, 
        tapes_per_block, 
        0, 0);
}

void Renderer::end_raygen() {
    VkCommandBuffer &commandBuffer = graphicsCommandBuffers[currentFrame];
}

void Renderer::diffuse() {
    VkCommandBuffer &commandBuffer = graphicsCommandBuffers[currentFrame];

        PLACE_TIMESTAMP();
    vkCmdNextSubpass(commandBuffer, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, diffusePipe.line);
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, diffusePipe.lineLayout, 0, 1, &diffusePipe.sets[currentFrame], 0, 0);

        cameraPos_OLD = cameraPos;
        cameraDir_OLD = cameraDir;
        struct rtpc {vec4 v1, v2;} pushconstant = {vec4(cameraPos,intBitsToFloat(iFrame)), vec4(cameraDir,0)};
        vkCmdPushConstants(commandBuffer, diffusePipe.lineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pushconstant), &pushconstant);
        
        PLACE_TIMESTAMP();
        vkCmdDraw(commandBuffer, 3, 1, 0, 0);
        PLACE_TIMESTAMP();

    vkCmdEndRenderPass(commandBuffer);
}

void Renderer::start_2nd_spass(){
    VkCommandBuffer &commandBuffer = graphicsCommandBuffers[currentFrame];

    // if(scaled){
        cmdTransLayoutBarrier(commandBuffer, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_MEMORY_WRITE_BIT|VK_ACCESS_MEMORY_READ_BIT, VK_ACCESS_MEMORY_WRITE_BIT|VK_ACCESS_MEMORY_READ_BIT,
            &highresFrames);
        cmdTransLayoutBarrier(commandBuffer, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_MEMORY_WRITE_BIT|VK_ACCESS_MEMORY_READ_BIT, VK_ACCESS_MEMORY_WRITE_BIT|VK_ACCESS_MEMORY_READ_BIT,
            &highresMatNorms);
        cmdTransLayoutBarrier(commandBuffer, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_MEMORY_WRITE_BIT|VK_ACCESS_MEMORY_READ_BIT, VK_ACCESS_MEMORY_WRITE_BIT|VK_ACCESS_MEMORY_READ_BIT,
            &highresDepthStencil);
        cmdTransLayoutBarrier(commandBuffer, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_MEMORY_WRITE_BIT|VK_ACCESS_MEMORY_READ_BIT, VK_ACCESS_MEMORY_WRITE_BIT|VK_ACCESS_MEMORY_READ_BIT,
            &maskFrame);
        cmdTransLayoutBarrier(commandBuffer, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_MEMORY_WRITE_BIT|VK_ACCESS_MEMORY_READ_BIT, VK_ACCESS_MEMORY_WRITE_BIT|VK_ACCESS_MEMORY_READ_BIT,
            &lowresMatNorm);
        cmdTransLayoutBarrier(commandBuffer, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_MEMORY_WRITE_BIT|VK_ACCESS_MEMORY_READ_BIT, VK_ACCESS_MEMORY_WRITE_BIT|VK_ACCESS_MEMORY_READ_BIT,
            &lowresDepthStencil);
        // cmdTransLayoutBarrier(commandBuffer, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_MEMORY_WRITE_BIT|VK_ACCESS_MEMORY_READ_BIT, VK_ACCESS_MEMORY_WRITE_BIT|VK_ACCESS_MEMORY_READ_BIT,
        //     &stencil);
        VkImageBlit 
            blit = {};
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
        // vkCmdBlitImage(commandBuffer, highresFrames.image, VK_IMAGE_LAYOUT_GENERAL, lowresFrame.image, VK_IMAGE_LAYOUT_GENERAL, 1, &blit, VK_FILTER_NEAREST);
        if(scaled) {
            vkCmdBlitImage(commandBuffer, highresMatNorms.image, VK_IMAGE_LAYOUT_GENERAL, lowresMatNorm.image, VK_IMAGE_LAYOUT_GENERAL, 1, &blit, VK_FILTER_NEAREST);
                blit.srcSubresource.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
                blit.dstSubresource.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
            vkCmdBlitImage(commandBuffer, highresDepthStencil.image, VK_IMAGE_LAYOUT_GENERAL, lowresDepthStencil.image, VK_IMAGE_LAYOUT_GENERAL, 1, &blit, VK_FILTER_NEAREST);
        }

        cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_MEMORY_WRITE_BIT|VK_ACCESS_MEMORY_READ_BIT, VK_ACCESS_MEMORY_WRITE_BIT|VK_ACCESS_MEMORY_READ_BIT,
            maskFrame);
        cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_MEMORY_WRITE_BIT|VK_ACCESS_MEMORY_READ_BIT, VK_ACCESS_MEMORY_WRITE_BIT|VK_ACCESS_MEMORY_READ_BIT,
            lowresMatNorm);
        cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_MEMORY_WRITE_BIT|VK_ACCESS_MEMORY_READ_BIT, VK_ACCESS_MEMORY_WRITE_BIT|VK_ACCESS_MEMORY_READ_BIT,
            lowresDepthStencil);
        // cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_MEMORY_WRITE_BIT|VK_ACCESS_MEMORY_READ_BIT, VK_ACCESS_MEMORY_WRITE_BIT|VK_ACCESS_MEMORY_READ_BIT,
        //     stencil);
    // }
    VkClearValue 
        far = {};
        far.color.float32[0] = -10000.f;
        far.color.float32[1] = -10000.f;
        far.color.float32[2] = -10000.f;
        far.color.float32[3] = -10000.f;
    VkClearValue 
        near = {};
        near.color.float32[0] = +10000.f;
        near.color.float32[1] = +10000.f;
        near.color.float32[2] = +10000.f;
        near.color.float32[3] = +10000.f;
    vector<VkClearValue> clearColors = {
        {}, 
        {}, 
        {}, 
        far,
        near, 
    };
    VkRenderPassBeginInfo renderPassInfo = {};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = smoke2glossyRpass;
        renderPassInfo.framebuffer = altFramebuffers[currentFrame];
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = {raytraceExtent.width, raytraceExtent.height};
        renderPassInfo.clearValueCount = clearColors.size();
        renderPassInfo.pClearValues    = clearColors.data();

    // cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_GRAPHICS_BIT, VK_ACCESS_MEMORY_WRITE_BIT|VK_ACCESS_MEMORY_READ_BIT, VK_ACCESS_MEMORY_WRITE_BIT|VK_ACCESS_MEMORY_READ_BIT,
    //     lowresMatNorm);

    vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, fillStencilGlossyPipe.line);

    VkViewport viewport = {};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width  = (float)(raytraceExtent.width );
        viewport.height = (float)(raytraceExtent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor = {};
        scissor.offset = {0, 0};
        scissor.extent = raytraceExtent;
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
    
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, fillStencilGlossyPipe.lineLayout, 0, 1, &fillStencilGlossyPipe.sets[currentFrame], 0, 0);

        PLACE_TIMESTAMP();
        vkCmdDraw(commandBuffer, 3, 1, 0, 0);
        PLACE_TIMESTAMP();
    
    vkCmdNextSubpass(commandBuffer, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, fillStencilSmokePipe.line);

    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
    
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, fillStencilSmokePipe.lineLayout, 0, 1, &fillStencilSmokePipe.sets[currentFrame], 0, 0);

        struct rtpc {vec4 centerSize;} pushconstant = {vec4(vec3(11,11,1.5)*16.f, 32)};
        vkCmdPushConstants(commandBuffer, fillStencilSmokePipe.lineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pushconstant), &pushconstant);

        PLACE_TIMESTAMP();
        vkCmdDraw(commandBuffer, 36, 1, 0, 0);
        PLACE_TIMESTAMP();
}

void Renderer::smoke() {
    VkCommandBuffer &commandBuffer = graphicsCommandBuffers[currentFrame];

    vkCmdNextSubpass(commandBuffer, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, smokePipe.line);

    VkViewport viewport = {};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width  = (float)(raytraceExtent.width );
        viewport.height = (float)(raytraceExtent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor = {};
        scissor.offset = {0, 0};
        scissor.extent = raytraceExtent;
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);

        struct rtpc {vec3 pos; int t; vec4 dir;} pushconstant = {vec3(cameraPos), iFrame, vec4(cameraDir,0)};
        vkCmdPushConstants(commandBuffer, smokePipe.lineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pushconstant), &pushconstant);

        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, smokePipe.lineLayout, 0, 1, &smokePipe.sets[currentFrame], 0, 0);

        PLACE_TIMESTAMP();
        vkCmdDraw(commandBuffer, 3, 1, 0, 0);
        PLACE_TIMESTAMP();
}

void Renderer::glossy() {
    VkCommandBuffer &commandBuffer = graphicsCommandBuffers[currentFrame];

    vkCmdNextSubpass(commandBuffer, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, glossyPipe.line);

    VkViewport viewport = {};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width  = (float)(raytraceExtent.width );
        viewport.height = (float)(raytraceExtent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    VkRect2D scissor = {};
        scissor.offset = {0, 0};
        scissor.extent = raytraceExtent;
    vkCmdSetScissor(commandBuffer, 0, 1, &scissor);
    
        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, glossyPipe.lineLayout, 0, 1, &glossyPipe.sets[currentFrame], 0, 0);

        struct rtpc {vec4 v1, v2;} pushconstant = {vec4(cameraPos,0), vec4(cameraDir,0)};
        vkCmdPushConstants(commandBuffer, glossyPipe.lineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pushconstant), &pushconstant);

        PLACE_TIMESTAMP();
        vkCmdDraw(commandBuffer, 3, 1, 0, 0);
        PLACE_TIMESTAMP();
}

void Renderer::end_2nd_spass(){
    VkCommandBuffer &commandBuffer = graphicsCommandBuffers[currentFrame];

    // vkCmdEndRenderPass(commandBuffer);
}

//basically samples results from previous smoke & glossy rpass
void Renderer::collect_glossy() {
    VkCommandBuffer &commandBuffer = graphicsCommandBuffers[currentFrame];

    // vector<VkClearValue> clearColors = {
    //     {}, 
    //     {}, 
    //     {}
    // };
    // VkRenderPassBeginInfo renderPassInfo = {};
    //     renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    //     renderPassInfo.renderPass = blur2presentRpass;
    //     renderPassInfo.framebuffer = overlayFramebuffers[currentFrame];
    //     renderPassInfo.renderArea.offset = {0, 0};
    //     renderPassInfo.renderArea.extent = swapChainExtent;
    //     renderPassInfo.clearValueCount = clearColors.size();
    //     renderPassInfo.pClearValues    = clearColors.data();

    // vkCmdBeginRenderPass(commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdNextSubpass(commandBuffer, VK_SUBPASS_CONTENTS_INLINE);

    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, blurPipe.line);

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

        vkCmdBindDescriptorSets(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, blurPipe.lineLayout, 0, 1, &blurPipe.sets[currentFrame], 0, 0);

        cameraPos_OLD = cameraPos;
        cameraDir_OLD = cameraDir;
        struct rtpc {vec4 v1, v2;} pushconstant = {vec4(cameraPos,0), vec4(cameraDir,0)};
        // vkCmdPushConstants(commandBuffer, blurPipe.lineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pushconstant), &pushconstant);

        // PLACE_TIMESTAMP();
        // vkCmdDraw(commandBuffer, 3, 1, 0, 0);
        // PLACE_TIMESTAMP();
        // vkCmdDispatch(commandBuffer, (raytraceExtent.width+7)/8, (raytraceExtent.height+7)/8, 1);

}

void Renderer::start_ui() {
    VkCommandBuffer& commandBuffer = graphicsCommandBuffers[currentFrame];
        PLACE_TIMESTAMP();
    
        PLACE_TIMESTAMP();
    VkResult result = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        // recreate_SwapchainDependent();
        resized = true;
        // return; // can be avoided, but it is just 1 frame 
    } else if ((result != VK_SUCCESS)) {
        printf(KRED "failed to acquire swap chain image!\n" KEND);
        exit(result);
    }

    vkCmdNextSubpass(commandBuffer, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline(commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, overlayPipe.line);
    VkViewport viewport = {};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width  = (float)(swapChainExtent.width );
        viewport.height = (float)(swapChainExtent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
    vkCmdSetViewport(commandBuffer, 0, 1, &viewport);

    ui_render_interface->last_scissors.offset = {0,0};
    ui_render_interface->last_scissors.extent = swapChainExtent;
    vkCmdSetScissor(commandBuffer, 0, 1, &ui_render_interface->last_scissors);
}

void Renderer::end_ui() {
    VkCommandBuffer &commandBuffer = graphicsCommandBuffers[currentFrame];

    vkCmdEndRenderPass(commandBuffer); //end of blur2present rpass

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

    cmdTransLayoutBarrier(commandBuffer, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL,
                VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT, VK_ACCESS_TRANSFER_READ_BIT,
                &highresFrames);

    cmdTransLayoutBarrier(commandBuffer, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL,
                VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
                VK_ACCESS_NONE, VK_ACCESS_MEMORY_WRITE_BIT,
                &swapchainImages[imageIndex]);

    vkCmdBlitImage(commandBuffer, highresFrames.image, VK_IMAGE_LAYOUT_GENERAL, swapchainImages[imageIndex].image, VK_IMAGE_LAYOUT_GENERAL, 1, &blit, VK_FILTER_NEAREST);

    cmdTransLayoutBarrier(commandBuffer, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR,
                VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT,
                VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_NONE,
                &swapchainImages[imageIndex]); 
    // cmdTransLayoutBarrier(commandBuffer, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT, VK_ACCESS_MEMORY_WRITE_BIT | VK_ACCESS_MEMORY_READ_BIT, &originBlockPalette[currentFrame]);
    // cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT); 
    // cmdPipelineBarrier(copyCommandBuffers[currentFrame], VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT); 
        PLACE_TIMESTAMP();
}

void Renderer::present() {
    VK_CHECK(vkEndCommandBuffer(computeCommandBuffers[currentFrame]));
    VK_CHECK(vkEndCommandBuffer(graphicsCommandBuffers[currentFrame]));
    VK_CHECK(vkEndCommandBuffer(copyCommandBuffers[currentFrame]));
    
    vector<VkSemaphore> signalSemaphores = {renderFinishedSemaphores};
    
    vector<VkSemaphore> waitSemaphores = {imageAvailableSemaphores[currentFrame]};
    VkPipelineStageFlags waitStages[] = {
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT,
    };

    vector<VkCommandBuffer> commandBuffers = {copyCommandBuffers[currentFrame], computeCommandBuffers[currentFrame], graphicsCommandBuffers[currentFrame]};
    // vector<VkCommandBuffer> commandBuffers = {computeCommandBuffers[currentFrame], graphicsCommandBuffers[currentFrame]};
    VkSubmitInfo submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.waitSemaphoreCount = waitSemaphores.size();
        submitInfo.pWaitSemaphores    = waitSemaphores.data();
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
    // printl(currentTimestamp)
    if(iFrame != 0){
        //this was causing basically device wait idle
        vkGetQueryPoolResults(
            device,
            queryPoolTimestamps[previousFrame],
            0,
            currentTimestamp,
            currentTimestamp * sizeof(uint64_t),
            timestamps.data(),
            sizeof(uint64_t),
            VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
    }

    double timestampPeriod = physicalDeviceProperties.limits.timestampPeriod;
    timeTakenByRadiance = float(timestamps[1] - timestamps[0]) * timestampPeriod / 1000000.0f;
    
    for(int i=0; i<timestampCount; i++){
        ftimestamps[i] = double(timestamps[i]) * timestampPeriod / 1000000.0;
    }
    if(iFrame > 5){
        for(int i=0; i<timestampCount; i++){
            average_ftimestamps[i] = mix(average_ftimestamps[i], ftimestamps[i], 0.1);
        }
    }
    
    previousFrame = currentFrame;
     currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    nextFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    iFrame++;
    process_ui_deletion_queue();

    // #ifdef MEASURE_PERFOMANCE
    // #endif
    // printl(timeTakenByRadiance)
}

void Renderer::cmdPipelineBarrier(VkCommandBuffer commandBuffer, 
    VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, 
    VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask,
    Buffer buffer) {
    VkBufferMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.srcAccessMask = srcAccessMask;
        barrier.dstAccessMask = dstAccessMask;
        barrier.size = VK_WHOLE_SIZE;
        barrier.buffer = buffer.buffer; // buffer buffer buffer

    vkCmdPipelineBarrier(
        commandBuffer,
        srcStageMask, dstStageMask,
        0,
        0, NULL,
        1, &barrier,
        0, NULL
    );
}
void Renderer::cmdPipelineBarrier(VkCommandBuffer commandBuffer, 
    VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, 
    VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask,
    Image image) {

    VkImageMemoryBarrier barrier = {};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = (image).layout;
        barrier.newLayout = (image).layout;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = (image).image;
        barrier.subresourceRange.aspectMask = (image).aspect;
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
void Renderer::cmdTransLayoutBarrier(VkCommandBuffer commandBuffer, VkImageLayout srcLayout, VkImageLayout targetLayout,
        VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask, 
        Image* image) {

    VkImageMemoryBarrier barrier = {};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = srcLayout;
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
// void Renderer::create_grass_state(GrassState* state){
//     create_Image_Storages(&state->marks,
//         VK_IMAGE_TYPE_2D,
//         VK_FORMAT_R16G16_SFLOAT,
//         VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
//         VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
//         0,
//         VK_IMAGE_ASPECT_COLOR_BIT,
//         {16, 16, 1});
//     transition_Image_Layout_Singletime(&state->marks, VK_IMAGE_LAYOUT_GENERAL);
// }
// void Renderer::create_water_state(WaterState* state){
//         create_Image_Storages(&state->waves,
//         VK_IMAGE_TYPE_2D,
//         VK_FORMAT_R16G16_SFLOAT,
//         VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
//         VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
//         0,
//         VK_IMAGE_ASPECT_COLOR_BIT,
//         {16, 16, 1});
//     transition_Image_Layout_Singletime(&state->waves, VK_IMAGE_LAYOUT_GENERAL);
// }

void Renderer::create_Image_Storages(vector<Image>* images, 
    VkImageType type, VkFormat format, VkImageUsageFlags usage, VmaMemoryUsage vma_usage, VmaAllocationCreateFlags vma_flags, VkImageAspectFlags aspect, 
    uvec3 size, int mipmaps, VkSampleCountFlagBits sample_count) {
    (*images).resize(MAX_FRAMES_IN_FLIGHT);
    
    // VkFormat chosen_format = findSupportedFormat(format, type, VK_IMAGE_TILING_OPTIMAL, usage);

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

    // VkFormat chosen_format = findSupportedFormat(format, type, VK_IMAGE_TILING_OPTIMAL, usage);

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
        if((aspect&VK_IMAGE_ASPECT_DEPTH_BIT) and (aspect&VK_IMAGE_ASPECT_STENCIL_BIT)) {
            viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT; //create stencil view yourself
        } else {
            viewInfo.subresourceRange.aspectMask = aspect;
        }
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
void Renderer::create_Buffer_Storages(Buffer* buffer, VkBufferUsageFlags usage, u32 size, bool host) {
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
    VK_CHECK(vmaCreateBuffer(VMAllocator, &bufferInfo, &allocInfo, &(*buffer).buffer, &(*buffer).alloc, NULL));
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
        poolInfo.maxSets = descriptor_sets_count; //becuase frames_in_flight multiply of 5 differents sets, each for shader 
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

void Renderer::deferDescriptorsetup(VkDescriptorSetLayout* dsetLayout, vector<VkDescriptorSet>* descriptorSets, vector<DescriptorInfo> descriptions, VkShaderStageFlags stages, VkDescriptorSetLayoutCreateFlags createFlags) {
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
        create_DescriptorSetLayout(descriptorTypes, stages, dsetLayout, createFlags);
    }
    
    descriptor_sets_count += MAX_FRAMES_IN_FLIGHT;
    DelayedDescriptorSetup delayed_setup = {dsetLayout, descriptorSets, descriptions, stages, createFlags};
    delayed_descriptor_setups.push_back(delayed_setup);
}

void Renderer::setupDescriptor(VkDescriptorSetLayout* dsetLayout, vector<VkDescriptorSet>* descriptorSets, vector<DescriptorInfo> descriptions, VkShaderStageFlags stages) {
    for (int frame_i = 0; frame_i < (*descriptorSets).size(); frame_i++) {
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
                    if(descriptions[i].type != VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER){
                        crash(descriptor has sampler but type is not for sampler);
                    }
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
        if((setup.sets->empty())) {
            allocate_Descriptor(*setup.sets, *setup.setLayout, descriptorPool, device);
        }
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
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
    // samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    // samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    // samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    VK_CHECK(vkCreateSampler(device, &samplerInfo, NULL, &linearSampler_tiled));


        samplerInfo.magFilter = VK_FILTER_NEAREST;
        samplerInfo.minFilter = VK_FILTER_NEAREST;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    VK_CHECK(vkCreateSampler(device, &samplerInfo, NULL, &overlaySampler));
    
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.magFilter = VK_FILTER_LINEAR;
    samplerInfo.minFilter = VK_FILTER_LINEAR;
    VK_CHECK(vkCreateSampler(device, &samplerInfo, NULL, &unnormLinear));
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
    samplerInfo.unnormalizedCoordinates = VK_FALSE;
    samplerInfo.magFilter = VK_FILTER_NEAREST;
    samplerInfo.minFilter = VK_FILTER_NEAREST;
    VK_CHECK(vkCreateSampler(device, &samplerInfo, NULL, &unnormNearest));
}

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

VkFormat Renderer::findSupportedFormat(vector<VkFormat> candidates, 
                             VkImageType type,
                             VkImageTiling tiling, 
                             VkImageUsageFlags usage) 
{
    for (VkFormat format : candidates) {
        // cout << string_VkFormat(format) << "\n";
        VkImageFormatProperties imageFormatProperties;
        VkResult result = vkGetPhysicalDeviceImageFormatProperties(
            physicalDevice, 
            format, 
            type,
            tiling, 
            usage, 
            // 0, 
            0, // No flags
            &imageFormatProperties);

        // cout << string_VkResult(result) << "\n";
        if (result == VK_SUCCESS) {
            return format;
        }
    }
    crash(Failed to find supported format!);
}