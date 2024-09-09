#define VMA_IMPLEMENTATION
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#include "render.hpp"
// #include <stdfloat>
// #include <BS_thread_pool.hpp> //TODO: howto depend
#include "ao_lut.hpp"

using std::array;
using std::vector;

using glm::u8, glm::u16, glm::u16, glm::u32;
using glm::i8, glm::i16, glm::i32;
using glm::f32, glm::f64;
using glm::defaultp;
using glm::quat;
using glm::ivec2,glm::ivec3,glm::ivec4;
using glm::i8vec2,glm::i8vec3,glm::i8vec4;
using glm::i16vec2,glm::i16vec3,glm::i16vec4;
using glm::uvec2,glm::uvec3,glm::uvec4;
using glm::u8vec2,glm::u8vec3,glm::u8vec4;
using glm::u16vec2,glm::u16vec3,glm::u16vec4;
using glm::vec,glm::vec2,glm::vec3,glm::vec4;
using glm::dvec2,glm::dvec3,glm::dvec4;
using glm::mat, glm::mat2, glm::mat3, glm::mat4;
using glm::dmat2, glm::dmat3, glm::dmat4;
using glm::quat_identity;
using glm::identity;
using glm::intBitsToFloat, glm::floatBitsToInt, glm::floatBitsToUint;

tuple<int, int> get_block_xy (int N);

vector<char> readFile (const char* filename);

void Renderer::init (Settings settings) {

    this->settings = settings;
    origin_world.allocate (settings.world_size);
    current_world.allocate (settings.world_size);
    lightmapExtent = {1024, 1024};
    // lightmapExtent = {768,768};
    //to fix weird mesh culling problems for identity
    // camera.updateCamera();

    ui_render_interface = new(MyRenderInterface);
    ui_render_interface->render = this;
    createWindow();
    VK_CHECK (volkInitialize());
    getInstanceExtensions();
    createInstance();
    volkLoadInstance (instance);
#ifndef VKNDEBUG
    setupDebugMessenger();
#endif
    createSurface();
    pickPhysicalDevice();
    createLogicalDevice();
    createAllocator();
    createSwapchain(); //should be before anything related to its size and format
    createSwapchainImageViews();
    DEPTH_FORMAT = findSupportedFormat ({DEPTH_FORMAT_PREFERED, DEPTH_FORMAT_SPARE}, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL,
                                        VK_IMAGE_USAGE_TRANSFER_SRC_BIT | 
                                        VK_IMAGE_USAGE_TRANSFER_DST_BIT | 
                                        VK_IMAGE_USAGE_SAMPLED_BIT | 
                                        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | 
                                        VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT);
    createCommandPool();
    createImages();
println
    createSwapchainDependentImages();
println
    //ok i did it
    createRenderPass({
            {&lightmap, Clear, Store, DontCare, DontCare}
        }, {
            {{&lightmapBlocksPipe, &lightmapModelsPipe}, {}, {}, &lightmap}
        }, &lightmapRpass);
println
    createRenderPass({
            {&highresMatNorm, DontCare, Store, DontCare, DontCare},
            {&highresDepthStencil, Clear, Store, Clear, Store},
        }, {
            {{&raygenBlocksPipe}, {}, {&highresMatNorm}, &highresDepthStencil},
            {{&raygenModelsPipe}, {}, {&highresMatNorm}, &highresDepthStencil},
            {{&raygenParticlesPipe}, {}, {&highresMatNorm}, &highresDepthStencil},
            {{&raygenGrassPipe}, {}, {&highresMatNorm}, &highresDepthStencil},
            {{&raygenWaterPipe}, {}, {&highresMatNorm}, &highresDepthStencil},
        }, &gbufferRpass);
println
    createRenderPass({
            {&highresMatNorm,      Load    , DontCare, DontCare, DontCare},
            {&highresFrame,        DontCare, DontCare, DontCare, DontCare},
            {&swapchainImages[0],  DontCare, Store   , DontCare, DontCare, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR},
            {&highresDepthStencil, Load    , DontCare, Load    , DontCare},
            {&farDepth,            Clear   , DontCare, DontCare, DontCare},
            {&nearDepth,           Clear   , DontCare, DontCare, DontCare},
        }, {
            {{&diffusePipe},           {&highresMatNorm, &highresDepthStencil}, {&highresFrame},         NULL},
            {{&aoPipe},                {&highresMatNorm, &highresDepthStencil}, {&highresFrame},         NULL},
            {{&fillStencilGlossyPipe}, {&highresMatNorm},                       {/*empty*/},             &highresDepthStencil},
            {{&fillStencilSmokePipe }, {/*empty*/},                             {&farDepth, &nearDepth}, &highresDepthStencil},
            {{&glossyPipe},            {/*empty*/},                             {&highresFrame},         &highresDepthStencil},
            {{&smokePipe},             {&nearDepth, &farDepth},                 {&highresFrame},         &highresDepthStencil},
            {{&tonemapPipe},           {&highresFrame},                         {&swapchainImages[0]},   NULL},
            {{&overlayPipe},           {/*empty*/},                             {&swapchainImages[0]},   NULL},
        }, &shadeRpass);
    createFramebuffers();
println
    createSamplers();
    printl (swapChainImageFormat)
    createCommandBuffers ( &computeCommandBuffers, MAX_FRAMES_IN_FLIGHT);
    createCommandBuffers (&lightmapCommandBuffers, MAX_FRAMES_IN_FLIGHT);
    createCommandBuffers (&graphicsCommandBuffers, MAX_FRAMES_IN_FLIGHT);
    createCommandBuffers ( &copyCommandBuffers, MAX_FRAMES_IN_FLIGHT);
println
    setupDescriptors();
println
    createPipilines();
println
    createSyncObjects();
    gen_perlin_2d();
    gen_perlin_3d();
println
    assert (timestampCount != 0);
    timestampNames.resize (timestampCount);
    timestamps.resize (timestampCount);
    ftimestamps.resize (timestampCount);
    average_ftimestamps.resize (timestampCount);
    VkQueryPoolCreateInfo query_pool_info{};
    query_pool_info.sType = VK_STRUCTURE_TYPE_QUERY_POOL_CREATE_INFO;
    query_pool_info.queryType = VK_QUERY_TYPE_TIMESTAMP;
    query_pool_info.queryCount = timestampCount;
    queryPoolTimestamps.resize (MAX_FRAMES_IN_FLIGHT);
    for (auto &q : queryPoolTimestamps) {
        VK_CHECK (vkCreateQueryPool (device, &query_pool_info, NULL, &q));
    }
println
}

void Renderer::createImages() {
    createBufferStorages (&stagingWorld,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        settings.world_size.x* settings.world_size.y* settings.world_size.z* sizeof (BlockID_t), true);
    createImageStorages (&world,
        VK_IMAGE_TYPE_3D,
        VK_FORMAT_R16_SINT,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT,
        settings.world_size); //TODO: dynamic
    createImageStorages (&lightmap,
        VK_IMAGE_TYPE_2D,
        LIGHTMAPS_FORMAT,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        VK_IMAGE_ASPECT_DEPTH_BIT,
    {lightmapExtent.width, lightmapExtent.height, 1});
println
    createImageStorages (&radianceCache,
        VK_IMAGE_TYPE_3D,
        RADIANCE_FORMAT,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT,
    {settings.world_size.x, settings.world_size.y, settings.world_size.z}); //TODO: dynamic
    createImageStorages (&originBlockPalette,
        VK_IMAGE_TYPE_3D,
        VK_FORMAT_R8_UINT,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT,
        // {16*BLOCK_PALETTE_SIZE_X, 16*BLOCK_PALETTE_SIZE_Y, 32}); //TODO: dynamic
    {16 * BLOCK_PALETTE_SIZE_X, 16 * BLOCK_PALETTE_SIZE_Y, 16}); //TODO: dynamic
    createImageStorages (&materialPalette,
        VK_IMAGE_TYPE_2D,
        VK_FORMAT_R32_SFLOAT, //try R32G32
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT,
    {6, 256, 1}); //TODO: dynamic, text formats, pack Material into smth other than 6 floats :)
    createImageStorages (&grassState,
        VK_IMAGE_TYPE_2D,
        VK_FORMAT_R16G16_SFLOAT,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        0,
        VK_IMAGE_ASPECT_COLOR_BIT,
    {settings.world_size.x * 2, settings.world_size.y * 2, 1}); //for quality
    createImageStorages (&waterState,
        VK_IMAGE_TYPE_2D,
        VK_FORMAT_R16G16B16A16_SFLOAT,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        0,
        VK_IMAGE_ASPECT_COLOR_BIT,
    {settings.world_size.x * 2, settings.world_size.y * 2, 1}); //for quality
    createImageStorages (&perlinNoise2d,
        VK_IMAGE_TYPE_2D,
        VK_FORMAT_R16G16_SNORM,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        0,
        VK_IMAGE_ASPECT_COLOR_BIT,
    {settings.world_size.x, settings.world_size.y, 1}); //does not matter than much
    createImageStorages (&perlinNoise3d,
        VK_IMAGE_TYPE_3D,
        VK_FORMAT_R16G16B16A16_UNORM,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        0,
        VK_IMAGE_ASPECT_COLOR_BIT,
    {32, 32, 32}); //does not matter than much
    createBufferStorages (&gpuParticles,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        settings.maxParticleCount* sizeof (Particle), true);
    createBufferStorages (&uniform,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        220, false); //no way i write it with sizeof's
    createBufferStorages (&lightUniform,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        sizeof (mat4), false);
    createBufferStorages (&aoLutUniform,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        sizeof (AoLut) * 8, false); //TODO DYNAMIC AO SAMPLE COUNT
    createBufferStorages (&gpuRadianceUpdates,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        sizeof (i8vec4)*settings.world_size.x* settings.world_size.y* settings.world_size.z, false); //TODO test extra mem
    createBufferStorages (&stagingRadianceUpdates,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        sizeof (ivec4)*settings.world_size.x* settings.world_size.y* settings.world_size.z, true); //TODO test extra mem
    stagingWorldMapped.resize (MAX_FRAMES_IN_FLIGHT);
    gpuParticlesMapped.resize (MAX_FRAMES_IN_FLIGHT);
    stagingRadianceUpdatesMapped.resize (MAX_FRAMES_IN_FLIGHT);
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vmaMapMemory (VMAllocator, gpuParticles[i].alloc, &gpuParticlesMapped[i]);
            gpuParticles[i].is_mapped = true;
        vmaMapMemory (VMAllocator, stagingWorld[i].alloc, &stagingWorldMapped[i]);
            stagingWorld[i].is_mapped = true;
        vmaMapMemory (VMAllocator, stagingRadianceUpdates[i].alloc, &stagingRadianceUpdatesMapped[i]);
            stagingRadianceUpdates[i].is_mapped = true;
    }
}

void Renderer::setupDescriptors() {
    createDescriptorSetLayout ({
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, //in texture (ui)
    },
    VK_SHADER_STAGE_FRAGMENT_BIT, &overlayPipe.setLayout,
    VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR);
    deferDescriptorSetup (&lightmapBlocksPipe.setLayout, &lightmapBlocksPipe.sets, {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, RD_CURRENT, (lightUniform), {/*empty*/}, NO_SAMPLER, NO_LAYOUT},
    }, VK_SHADER_STAGE_VERTEX_BIT);
    deferDescriptorSetup (&lightmapModelsPipe.setLayout, &lightmapModelsPipe.sets, {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, RD_CURRENT, (lightUniform), {/*empty*/}, NO_SAMPLER, NO_LAYOUT},
    }, VK_SHADER_STAGE_VERTEX_BIT);
println
    deferDescriptorSetup (&radiancePipe.setLayout, &radiancePipe.sets, {
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, RD_FIRST, {/*empty*/}, {world}, unnormNearest, VK_IMAGE_LAYOUT_GENERAL},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, RD_CURRENT, {/*empty*/}, (originBlockPalette), unnormNearest, VK_IMAGE_LAYOUT_GENERAL},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, RD_CURRENT, {/*empty*/}, (materialPalette), nearestSampler, VK_IMAGE_LAYOUT_GENERAL},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, RD_FIRST, {/*empty*/}, {radianceCache}, unnormLinear, VK_IMAGE_LAYOUT_GENERAL},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, RD_FIRST, {/*empty*/}, {radianceCache}, NO_SAMPLER, VK_IMAGE_LAYOUT_GENERAL},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, RD_FIRST, {gpuRadianceUpdates}, {}, NO_SAMPLER, NO_LAYOUT},
    }, VK_SHADER_STAGE_COMPUTE_BIT);
println
    deferDescriptorSetup (&diffusePipe.setLayout, &diffusePipe.sets, {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, RD_CURRENT, (uniform), {/*empty*/}, NO_SAMPLER, NO_LAYOUT, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT},
        {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, RD_FIRST, {/*empty*/}, {highresMatNorm}, NO_SAMPLER, VK_IMAGE_LAYOUT_GENERAL},
        {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, RD_FIRST, {/*empty*/}, {highresDepthStencil}, NO_SAMPLER, VK_IMAGE_LAYOUT_GENERAL},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, RD_FIRST, {/*empty*/}, {materialPalette}, nearestSampler, VK_IMAGE_LAYOUT_GENERAL},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, RD_FIRST, {/*empty*/}, {radianceCache}, unnormLinear, VK_IMAGE_LAYOUT_GENERAL},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, RD_FIRST, {/*empty*/}, {lightmap}, shadowSampler, VK_IMAGE_LAYOUT_GENERAL},
        // {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, RD_FIRST, {/*empty*/}, {lightmap}, linearSampler, VK_IMAGE_LAYOUT_GENERAL},
    }, VK_SHADER_STAGE_FRAGMENT_BIT);
println
    deferDescriptorSetup (&aoPipe.setLayout, &aoPipe.sets, {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, RD_CURRENT, (uniform), {/*empty*/}, NO_SAMPLER, NO_LAYOUT},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, RD_CURRENT, (aoLutUniform), {/*empty*/}, NO_SAMPLER, NO_LAYOUT},
        {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, RD_FIRST, {/*empty*/}, {highresMatNorm}, NO_SAMPLER, VK_IMAGE_LAYOUT_GENERAL},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, RD_FIRST, {/*empty*/}, {highresDepthStencil}, nearestSampler, VK_IMAGE_LAYOUT_GENERAL},
    }, VK_SHADER_STAGE_FRAGMENT_BIT);
println
    deferDescriptorSetup (&tonemapPipe.setLayout, &tonemapPipe.sets, {
        {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, RD_FIRST, {/*empty*/}, {highresFrame}, NO_SAMPLER, VK_IMAGE_LAYOUT_GENERAL},
    }, VK_SHADER_STAGE_FRAGMENT_BIT);
println
    deferDescriptorSetup (&fillStencilGlossyPipe.setLayout, &fillStencilGlossyPipe.sets, {
        {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, RD_FIRST, {/*empty*/}, {highresMatNorm}, NO_SAMPLER, VK_IMAGE_LAYOUT_GENERAL},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, RD_CURRENT, {/*empty*/}, (materialPalette), nearestSampler, VK_IMAGE_LAYOUT_GENERAL},
    }, VK_SHADER_STAGE_FRAGMENT_BIT);
println
    deferDescriptorSetup (&fillStencilSmokePipe.setLayout, &fillStencilSmokePipe.sets, {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, RD_CURRENT, (uniform), {/*empty*/}, NO_SAMPLER, NO_LAYOUT},
    }, VK_SHADER_STAGE_VERTEX_BIT);
println
    deferDescriptorSetup (&glossyPipe.setLayout, &glossyPipe.sets, {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, RD_CURRENT, (uniform), {/*empty*/}, NO_SAMPLER, NO_LAYOUT},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, RD_FIRST, {/*empty*/}, {highresMatNorm}, nearestSampler, VK_IMAGE_LAYOUT_GENERAL},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, RD_FIRST, {/*empty*/}, {lowresDepthStencil}, nearestSampler, VK_IMAGE_LAYOUT_GENERAL},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, RD_FIRST, {/*empty*/}, {world}, unnormNearest, VK_IMAGE_LAYOUT_GENERAL},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, RD_CURRENT, {/*empty*/}, (originBlockPalette), unnormNearest, VK_IMAGE_LAYOUT_GENERAL},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, RD_CURRENT, {/*empty*/}, (materialPalette), nearestSampler, VK_IMAGE_LAYOUT_GENERAL},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, RD_FIRST, {/*empty*/}, {radianceCache}, unnormLinear, VK_IMAGE_LAYOUT_GENERAL},
    }, VK_SHADER_STAGE_FRAGMENT_BIT);
println
    deferDescriptorSetup (&smokePipe.setLayout, &smokePipe.sets, {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, RD_CURRENT, (uniform), {/*empty*/}, NO_SAMPLER, NO_LAYOUT},
        {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, RD_FIRST, {/*empty*/}, {farDepth}, NO_SAMPLER, VK_IMAGE_LAYOUT_GENERAL},
        {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, RD_FIRST, {/*empty*/}, {nearDepth}, NO_SAMPLER, VK_IMAGE_LAYOUT_GENERAL},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, RD_FIRST, {/*empty*/}, {radianceCache}, NO_SAMPLER, VK_IMAGE_LAYOUT_GENERAL},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, RD_FIRST, {/*empty*/}, {perlinNoise3d}, linearSampler_tiled, VK_IMAGE_LAYOUT_GENERAL},
    }, VK_SHADER_STAGE_FRAGMENT_BIT);
println
    // create_DescriptorSetLayout({
    // VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, // per-quad attributes
    // },
    // VK_SHADER_STAGE_VERTEX_BIT, &blocksPushLayout,
    // VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR);
    deferDescriptorSetup (&raygenBlocksPipe.setLayout, &raygenBlocksPipe.sets, {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, RD_CURRENT, (uniform), {/*empty*/}, NO_SAMPLER, NO_LAYOUT},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, RD_CURRENT, {}, {originBlockPalette}, unnormNearest, VK_IMAGE_LAYOUT_GENERAL},
    }, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    // setup_RayGen_Particles_Descriptors();
    createDescriptorSetLayout ({
        VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, // model voxels
    },
    VK_SHADER_STAGE_FRAGMENT_BIT, &raygenModelsPushLayout,
    VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR);
    // 0);
    deferDescriptorSetup (&raygenModelsPipe.setLayout, &raygenModelsPipe.sets, {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, RD_CURRENT, (uniform), {/*empty*/}, NO_SAMPLER, NO_LAYOUT},
    }, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
println
    deferDescriptorSetup (&raygenParticlesPipe.setLayout, &raygenParticlesPipe.sets, {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, RD_CURRENT, (uniform), {/*empty*/}, NO_SAMPLER, NO_LAYOUT},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, RD_FIRST, {/*empty*/}, {world}, NO_SAMPLER, VK_IMAGE_LAYOUT_GENERAL},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, RD_CURRENT, {/*empty*/}, (originBlockPalette), NO_SAMPLER, VK_IMAGE_LAYOUT_GENERAL},
    }, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT);
    deferDescriptorSetup (&updateGrassPipe.setLayout, &updateGrassPipe.sets, {
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, RD_FIRST, {}, {grassState}, NO_SAMPLER, VK_IMAGE_LAYOUT_GENERAL},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, RD_FIRST, {}, {perlinNoise2d}, linearSampler_tiled, VK_IMAGE_LAYOUT_GENERAL},
    }, VK_SHADER_STAGE_COMPUTE_BIT);
    deferDescriptorSetup (&updateWaterPipe.setLayout, &updateWaterPipe.sets, {
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, RD_FIRST, {}, {waterState}, NO_SAMPLER, VK_IMAGE_LAYOUT_GENERAL},
    }, VK_SHADER_STAGE_COMPUTE_BIT);
    deferDescriptorSetup (&raygenGrassPipe.setLayout, &raygenGrassPipe.sets, {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, RD_CURRENT, (uniform), {/*empty*/}, NO_SAMPLER, NO_LAYOUT},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, RD_FIRST, {}, {grassState}, linearSampler, VK_IMAGE_LAYOUT_GENERAL},
    }, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT);
    deferDescriptorSetup (&raygenWaterPipe.setLayout, &raygenWaterPipe.sets, {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, RD_CURRENT, (uniform), {/*empty*/}, NO_SAMPLER, NO_LAYOUT},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, RD_FIRST, {}, {waterState}, linearSampler_tiled, VK_IMAGE_LAYOUT_GENERAL},
    }, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT);
    deferDescriptorSetup (&genPerlin2dPipe.setLayout, &genPerlin2dPipe.sets, {
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, RD_FIRST, {}, {perlinNoise2d}, NO_SAMPLER, VK_IMAGE_LAYOUT_GENERAL},
    }, VK_SHADER_STAGE_COMPUTE_BIT);
    deferDescriptorSetup (&genPerlin3dPipe.setLayout, &genPerlin3dPipe.sets, {
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, RD_FIRST, {}, {perlinNoise3d}, NO_SAMPLER, VK_IMAGE_LAYOUT_GENERAL},
    }, VK_SHADER_STAGE_COMPUTE_BIT);
    createDescriptorSetLayout ({
        VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, // model voxels
    },
    VK_SHADER_STAGE_COMPUTE_BIT, &mapPushLayout,
    VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR);
    // 0);
    deferDescriptorSetup (&mapPipe.setLayout, &mapPipe.sets, {
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, RD_FIRST, {}, {world}, NO_SAMPLER, VK_IMAGE_LAYOUT_GENERAL},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, RD_CURRENT, {}, originBlockPalette, NO_SAMPLER, VK_IMAGE_LAYOUT_GENERAL},
    }, VK_SHADER_STAGE_COMPUTE_BIT);
    // deferDescriptorsetup(&dfxPipe.setLayout, &dfxPipe.sets, {
    // {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, RD_FIRST, {}, {originBlockPalette}, NO_SAMPLER, VK_IMAGE_LAYOUT_GENERAL},
    // {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, RD_FIRST, {}, {distancePalette   }, NO_SAMPLER, VK_IMAGE_LAYOUT_GENERAL},
    // }, VK_SHADER_STAGE_COMPUTE_BIT);
    // deferDescriptorsetup(&dfyPipe.setLayout, &dfyPipe.sets, {
    // {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, RD_FIRST, {}, {originBlockPalette}, NO_SAMPLER, VK_IMAGE_LAYOUT_GENERAL},
    // {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, RD_FIRST, {}, {distancePalette   }, NO_SAMPLER, VK_IMAGE_LAYOUT_GENERAL},
    // }, VK_SHADER_STAGE_COMPUTE_BIT);
    // deferDescriptorsetup(&dfzPipe.setLayout, &dfzPipe.sets, {
    // {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, RD_FIRST, {}, {originBlockPalette}, NO_SAMPLER, VK_IMAGE_LAYOUT_GENERAL},
    // {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, RD_FIRST, {}, {distancePalette   }, NO_SAMPLER, VK_IMAGE_LAYOUT_GENERAL},
    // }, VK_SHADER_STAGE_COMPUTE_BIT);
    // deferDescriptorsetup(&bitmaskPipe.setLayout, &bitmaskPipe.sets, {
    // {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, RD_FIRST, {}, {originBlockPalette}, NO_SAMPLER, VK_IMAGE_LAYOUT_GENERAL},
    // {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, RD_FIRST, {}, {bitPalette}, NO_SAMPLER, VK_IMAGE_LAYOUT_GENERAL},
    // }, VK_SHADER_STAGE_COMPUTE_BIT);
println
    flushDescriptorSetup();
println
}

void Renderer::createPipilines() {
    //that is why NOT abstracting vulkan is also an option
    //if you cannot guess what things mean by just looking at them maybe read old (0.0.3) release src
println
    lightmapBlocksPipe.subpassId = 0;
    createRasterPipeline (&lightmapBlocksPipe, 0, {
        {"shaders/compiled/lightmapBlocksVert.spv", VK_SHADER_STAGE_VERTEX_BIT},
        //doesnt need fragment
    }, {
        {VK_FORMAT_R8G8B8_UINT, offsetof (PackedVoxelCircuit, pos)},
    },
    sizeof (PackedVoxelCircuit), VK_VERTEX_INPUT_RATE_VERTEX, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    lightmapExtent, {NO_BLEND}, (sizeof (i16vec4)), FULL_DEPTH_TEST, VK_CULL_MODE_NONE, NO_DISCARD, NO_STENCIL);
    lightmapModelsPipe.subpassId = 0;
    createRasterPipeline (&lightmapModelsPipe, 0, {
        {"shaders/compiled/lightmapModelsVert.spv", VK_SHADER_STAGE_VERTEX_BIT},
        //doesnt need fragment
    }, {
        {VK_FORMAT_R8G8B8_UINT, offsetof (PackedVoxelCircuit, pos)},
    },
    sizeof (PackedVoxelCircuit), VK_VERTEX_INPUT_RATE_VERTEX, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    lightmapExtent, {NO_BLEND}, (sizeof (quat) + sizeof (vec4)), FULL_DEPTH_TEST, VK_CULL_MODE_NONE, NO_DISCARD, NO_STENCIL);
    raygenBlocksPipe.subpassId = 0;
    createRasterPipeline (&raygenBlocksPipe, 0, {
        {"shaders/compiled/rayGenBlocksVert.spv", VK_SHADER_STAGE_VERTEX_BIT},
        {"shaders/compiled/rayGenBlocksFrag.spv", VK_SHADER_STAGE_FRAGMENT_BIT},
    }, {
        {VK_FORMAT_R8G8B8_UINT, offsetof (PackedVoxelCircuit, pos)},
        // {VK_FORMAT_R8G8B8_SINT, offsetof(VoxelVertex, norm)},
        // {VK_FORMAT_R8_UINT, offsetof(PackedVoxelVertex, matID)},
    },
    sizeof (PackedVoxelCircuit), VK_VERTEX_INPUT_RATE_VERTEX, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    swapChainExtent, {NO_BLEND}, (12), FULL_DEPTH_TEST, VK_CULL_MODE_NONE, NO_DISCARD, NO_STENCIL);
println
    raygenModelsPipe.subpassId = 1;
    createRasterPipeline (&raygenModelsPipe, raygenModelsPushLayout, {
        {"shaders/compiled/rayGenModelsVert.spv", VK_SHADER_STAGE_VERTEX_BIT},
        {"shaders/compiled/rayGenModelsFrag.spv", VK_SHADER_STAGE_FRAGMENT_BIT},
    }, {
        {VK_FORMAT_R8G8B8_UINT, offsetof (PackedVoxelCircuit, pos)},
    },
    sizeof (PackedVoxelCircuit), VK_VERTEX_INPUT_RATE_VERTEX, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    swapChainExtent, {NO_BLEND}, (sizeof (vec4) * 3), FULL_DEPTH_TEST, VK_CULL_MODE_NONE, NO_DISCARD, NO_STENCIL);
println
    raygenParticlesPipe.subpassId = 2;
    createRasterPipeline (&raygenParticlesPipe, 0, {
        {"shaders/compiled/rayGenParticlesVert.spv", VK_SHADER_STAGE_VERTEX_BIT},
        {"shaders/compiled/rayGenParticlesGeom.spv", VK_SHADER_STAGE_GEOMETRY_BIT},
        {"shaders/compiled/rayGenParticlesFrag.spv", VK_SHADER_STAGE_FRAGMENT_BIT},
    }, {
        {VK_FORMAT_R32G32B32_SFLOAT, offsetof (Particle, pos)},
        {VK_FORMAT_R32G32B32_SFLOAT, offsetof (Particle, vel)},
        {VK_FORMAT_R32_SFLOAT, offsetof (Particle, lifeTime)},
        {VK_FORMAT_R8_UINT, offsetof (Particle, matID)},
    },
    sizeof (Particle), VK_VERTEX_INPUT_RATE_VERTEX, VK_PRIMITIVE_TOPOLOGY_POINT_LIST,
    swapChainExtent, {NO_BLEND}, 0, FULL_DEPTH_TEST, VK_CULL_MODE_NONE, NO_DISCARD, NO_STENCIL);
println
    raygenGrassPipe.subpassId = 3;
    createRasterPipeline (&raygenGrassPipe, 0, {
        {"shaders/compiled/grassVert.spv", VK_SHADER_STAGE_VERTEX_BIT},
        {"shaders/compiled/grassFrag.spv", VK_SHADER_STAGE_FRAGMENT_BIT},
    }, {/*empty*/},
    0, VK_VERTEX_INPUT_RATE_VERTEX, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
    swapChainExtent, {NO_BLEND}, sizeof (vec4) + sizeof (int) * 2 + sizeof (int) * 2, FULL_DEPTH_TEST, VK_CULL_MODE_NONE, NO_DISCARD, NO_STENCIL);
println
    raygenWaterPipe.subpassId = 4;
    createRasterPipeline (&raygenWaterPipe, 0, {
        {"shaders/compiled/waterVert.spv", VK_SHADER_STAGE_VERTEX_BIT},
        {"shaders/compiled/waterFrag.spv", VK_SHADER_STAGE_FRAGMENT_BIT},
    }, {/*empty*/},
    0, VK_VERTEX_INPUT_RATE_VERTEX, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
    swapChainExtent, {NO_BLEND}, sizeof (vec4) + sizeof (int) * 2, FULL_DEPTH_TEST, VK_CULL_MODE_NONE, NO_DISCARD, NO_STENCIL);
println
    diffusePipe.subpassId = 0;
    createRasterPipeline (&diffusePipe, 0, {
        {"shaders/compiled/fullscreenTriagVert.spv", VK_SHADER_STAGE_VERTEX_BIT},
        {"shaders/compiled/diffuseFrag.spv", VK_SHADER_STAGE_FRAGMENT_BIT},
    }, {/*fullscreen pass*/},
    0, VK_VERTEX_INPUT_RATE_VERTEX, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    swapChainExtent, {NO_BLEND}, sizeof (ivec4) + sizeof (vec4) * 4 + sizeof (mat4), NO_DEPTH_TEST, VK_CULL_MODE_NONE, NO_DISCARD, NO_STENCIL);
    aoPipe.subpassId = 1;
    createRasterPipeline (&aoPipe, 0, {
        {"shaders/compiled/fullscreenTriagVert.spv", VK_SHADER_STAGE_VERTEX_BIT},
        {"shaders/compiled/hbaoFrag.spv", VK_SHADER_STAGE_FRAGMENT_BIT},
    }, {/*fullscreen pass*/},
    0, VK_VERTEX_INPUT_RATE_VERTEX, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    swapChainExtent, {BLEND_MIX}, 0, NO_DEPTH_TEST, VK_CULL_MODE_NONE, NO_DISCARD, NO_STENCIL);
println
    fillStencilGlossyPipe.subpassId = 2;
    createRasterPipeline (&fillStencilGlossyPipe, 0, {
        {"shaders/compiled/fullscreenTriagVert.spv", VK_SHADER_STAGE_VERTEX_BIT},
        {"shaders/compiled/fillStencilGlossyFrag.spv", VK_SHADER_STAGE_FRAGMENT_BIT},
    }, {/*fullscreen pass*/},
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
    fillStencilSmokePipe.subpassId = 3;
    createRasterPipeline (&fillStencilSmokePipe, 0, {
        {"shaders/compiled/fillStencilSmokeVert.spv", VK_SHADER_STAGE_VERTEX_BIT},
        {"shaders/compiled/fillStencilSmokeFrag.spv", VK_SHADER_STAGE_FRAGMENT_BIT},
    }, {/*passed as push constants lol*/},
    0, VK_VERTEX_INPUT_RATE_VERTEX, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    swapChainExtent, {BLEND_REPLACE_IF_GREATER, BLEND_REPLACE_IF_LESS}, sizeof (vec3) + sizeof (int) + sizeof (vec4), READ_DEPTH_TEST, VK_CULL_MODE_NONE, NO_DISCARD, {
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
    glossyPipe.subpassId = 4;
    createRasterPipeline (&glossyPipe, 0, {
        {"shaders/compiled/fullscreenTriagVert.spv", VK_SHADER_STAGE_VERTEX_BIT},
        {"shaders/compiled/glossyFrag.spv", VK_SHADER_STAGE_FRAGMENT_BIT},
    }, {/*fullscreen pass*/},
    0, VK_VERTEX_INPUT_RATE_VERTEX, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    swapChainExtent, {BLEND_MIX}, sizeof (vec4) + sizeof (vec4), NO_DEPTH_TEST, VK_CULL_MODE_NONE, NO_DISCARD, {
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
    smokePipe.subpassId = 5;
    createRasterPipeline (&smokePipe, 0, {
        {"shaders/compiled/fullscreenTriagVert.spv", VK_SHADER_STAGE_VERTEX_BIT},
        {"shaders/compiled/smokeFrag.spv", VK_SHADER_STAGE_FRAGMENT_BIT},
    }, {/*fullscreen pass*/},
    0, VK_VERTEX_INPUT_RATE_VERTEX, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    swapChainExtent, {BLEND_MIX}, 0, NO_DEPTH_TEST, VK_CULL_MODE_NONE, NO_DISCARD, {
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
    tonemapPipe.subpassId = 6;
    createRasterPipeline (&tonemapPipe, 0, {
        {"shaders/compiled/fullscreenTriagVert.spv", VK_SHADER_STAGE_VERTEX_BIT},
        {"shaders/compiled/tonemapFrag.spv", VK_SHADER_STAGE_FRAGMENT_BIT},
    }, {/*fullscreen pass*/},
    0, VK_VERTEX_INPUT_RATE_VERTEX, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    swapChainExtent, {NO_BLEND}, 0, NO_DEPTH_TEST, VK_CULL_MODE_NONE, NO_DISCARD, NO_STENCIL);
println
    overlayPipe.subpassId = 7;
    createRasterPipeline (&overlayPipe, 0, {
        {"shaders/compiled/overlayVert.spv", VK_SHADER_STAGE_VERTEX_BIT},
        {"shaders/compiled/overlayFrag.spv", VK_SHADER_STAGE_FRAGMENT_BIT},
    }, {
        {VK_FORMAT_R32G32_SFLOAT, offsetof (Rml::Vertex, position)},
        {VK_FORMAT_R8G8B8A8_UNORM, offsetof (Rml::Vertex, colour)},
        {VK_FORMAT_R32G32_SFLOAT, offsetof (Rml::Vertex, tex_coord)},
    },
    sizeof (Rml::Vertex), VK_VERTEX_INPUT_RATE_VERTEX, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    swapChainExtent, {BLEND_MIX}, sizeof (vec4) + sizeof (mat4), NO_DEPTH_TEST, VK_CULL_MODE_NONE, NO_DISCARD, NO_STENCIL);
println
    createComputePipeline (&radiancePipe, 0, "shaders/compiled/radiance.spv", sizeof (int) * 4, VK_PIPELINE_CREATE_DISPATCH_BASE_BIT);
println
    createComputePipeline (&updateGrassPipe, 0, "shaders/compiled/updateGrass.spv", sizeof (vec2) * 2 + sizeof (float), 0);
println
    createComputePipeline (&updateWaterPipe, 0, "shaders/compiled/updateWater.spv", sizeof (float) + sizeof (vec2) * 2, 0);
println
    createComputePipeline (&genPerlin2dPipe, 0, "shaders/compiled/perlin2.spv", 0, 0);
println
    createComputePipeline (&genPerlin3dPipe, 0, "shaders/compiled/perlin3.spv", 0, 0);
println
    // createComputePipeline(&dfxPipe,0, "shaders/compiled/dfx.spv", 0, 0);
    // createComputePipeline(&dfyPipe,0, "shaders/compiled/dfy.spv", 0, 0);
    // createComputePipeline(&dfzPipe,0, "shaders/compiled/dfz.spv", 0, 0);
    // createComputePipeline(&bitmaskPipe,0, "shaders/compiled/bitmask.spv", 0, 0);
    createComputePipeline (&mapPipe, mapPushLayout, "shaders/compiled/map.spv", sizeof (mat4) + sizeof (ivec4), 0);
println
}

void Renderer::createSwapchainDependentImages() {
    // int mipmaps;
    createImageStorages (&highresMatNorm,
        VK_IMAGE_TYPE_2D,
        MATNORM_FORMAT,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT,
    {swapChainExtent.width, swapChainExtent.height, 1});
    createImageStorages (&highresDepthStencil,
        VK_IMAGE_TYPE_2D,
        DEPTH_FORMAT,
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT,
    {swapChainExtent.width, swapChainExtent.height, 1});
    createImageStorages (&highresFrame,
        VK_IMAGE_TYPE_2D,
        FRAME_FORMAT,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT,
    {swapChainExtent.width, swapChainExtent.height, 1});
    if (settings.scaled) {
        createImageStorages (&lowresMatNorm,
            VK_IMAGE_TYPE_2D,
            MATNORM_FORMAT,
            VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
            VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
            VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
            VK_IMAGE_ASPECT_COLOR_BIT,
        {raytraceExtent.width, raytraceExtent.height, 1});
        createImageStorages (&lowresDepthStencil,
            VK_IMAGE_TYPE_2D,
            DEPTH_FORMAT,
            VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
            VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
            VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
            VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT,
        {raytraceExtent.width, raytraceExtent.height, 1});
    } else {
        //set them to the same ptrs
        lowresMatNorm = highresMatNorm;
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
    VK_CHECK (vkCreateImageView (device, &viewInfo, NULL, &stencilViewForDS));
    //required anyways
    createImageStorages (&farDepth, //smoke depth
        VK_IMAGE_TYPE_2D,
        SECONDARY_DEPTH_FORMAT,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT,
    {raytraceExtent.width, raytraceExtent.height, 1});
    createImageStorages (&nearDepth, //smoke depth
        VK_IMAGE_TYPE_2D,
        SECONDARY_DEPTH_FORMAT,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT,
    {raytraceExtent.width, raytraceExtent.height, 1});
}
void Renderer::createFramebuffers() {
    vector<vector<VkImageView>> rayGenVeiws (2);
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        rayGenVeiws[0].push_back (highresMatNorm.view);
        rayGenVeiws[1].push_back (highresDepthStencil.view);
    }
    vector<vector<VkImageView>> altVeiws (6);
    for (int i = 0; i < swapchainImages.size(); i++) {
        altVeiws[0].push_back (highresMatNorm.view);
        altVeiws[1].push_back (highresFrame.view);
        altVeiws[2].push_back (swapchainImages[i].view);
        altVeiws[3].push_back (stencilViewForDS);
        altVeiws[4].push_back (farDepth.view);
        altVeiws[5].push_back (nearDepth.view);
    }
    vector<vector<VkImageView>> lightmapVeiws (1);
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        lightmapVeiws[0].push_back (lightmap.view);
    }
println
    createNFramebuffers (&lightmapFramebuffers, &lightmapVeiws, lightmapRpass, MAX_FRAMES_IN_FLIGHT, lightmapExtent.width, lightmapExtent.height);
println
    createNFramebuffers (&rayGenFramebuffers, &rayGenVeiws, gbufferRpass, MAX_FRAMES_IN_FLIGHT, swapChainExtent.width, swapChainExtent.height);
println
    createNFramebuffers (&altFramebuffers, &altVeiws, shadeRpass, swapchainImages.size(), raytraceExtent.width, raytraceExtent.height);
println
}
void Renderer::recreateSwapchainDependent() {
    int width = 0, height = 0;
    glfwGetFramebufferSize (window.pointer, &width, &height);
    while (width == 0 || height == 0) {
        glfwGetFramebufferSize (window.pointer, &width, &height);
        glfwWaitEvents();
    }
    vkDeviceWaitIdle (device);
    cleanupSwapchainDependent();
    vkDeviceWaitIdle (device);
    createSwapchain();
    createSwapchainImageViews();
    vkDestroyCommandPool (device, commandPool, NULL);
    createCommandPool();
    createCommandBuffers (& graphicsCommandBuffers, MAX_FRAMES_IN_FLIGHT);
    createCommandBuffers (& computeCommandBuffers, MAX_FRAMES_IN_FLIGHT);
    VkCommandBufferAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.commandPool = commandPool;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandBufferCount = MAX_FRAMES_IN_FLIGHT;
    copyCommandBuffers.resize (MAX_FRAMES_IN_FLIGHT);
    VK_CHECK (vkAllocateCommandBuffers (device, &allocInfo, copyCommandBuffers.data()));
    createSwapchainDependentImages();
    vkDeviceWaitIdle (device);
}

void Renderer::deleteImages (vector<Image>* images) {
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroyImageView (device, (*images)[i].view, NULL);
        vmaDestroyImage (VMAllocator, (*images)[i].image, (*images)[i].alloc);
    }
}
void Renderer::deleteImages (Image* image) {
    vkDestroyImageView (device, (*image).view, NULL);
    vmaDestroyImage (VMAllocator, (*image).image, (*image).alloc);
}

void Renderer::deleteBuffers (vector<Buffer>* buffers) {
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        if((*buffers)[i].is_mapped){
            vmaUnmapMemory (VMAllocator, (*buffers)[i].alloc);
        }
        vmaDestroyBuffer (VMAllocator, (*buffers)[i].buffer, (*buffers)[i].alloc);
    }
}
void Renderer::deleteBuffers (Buffer* buffer) {
    if((*buffer).is_mapped){
        vmaUnmapMemory (VMAllocator, (*buffer).alloc);
    }
    vmaDestroyBuffer (VMAllocator, (*buffer).buffer, (*buffer).alloc);
}

void Renderer::cleanup() {
    delete ui_render_interface;
    deleteImages (&radianceCache);
    deleteImages (&world);
    deleteImages (&originBlockPalette);
    // deleteImages(&distancePalette);
    // deleteImages(&bitPalette);
    deleteImages (&materialPalette);
    deleteImages (&perlinNoise2d);
    deleteImages (&perlinNoise3d);
    deleteImages (&grassState);
    deleteImages (&waterState);
    deleteImages (&lightmap);
    vkDestroySampler (device, nearestSampler, NULL);
    vkDestroySampler (device, linearSampler, NULL);
    vkDestroySampler (device, linearSampler_tiled, NULL);
    vkDestroySampler (device, linearSampler_tiled_mirrored, NULL);
    vkDestroySampler (device, overlaySampler, NULL);
    vkDestroySampler (device, shadowSampler, NULL);
    vkDestroySampler (device, unnormLinear, NULL);
    vkDestroySampler (device, unnormNearest, NULL);
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vmaUnmapMemory (VMAllocator, stagingWorld[i].alloc);
        vmaUnmapMemory (VMAllocator, gpuParticles[i].alloc);
        vmaUnmapMemory (VMAllocator, stagingRadianceUpdates[i].alloc);
        vmaDestroyBuffer (VMAllocator, stagingWorld[i].buffer, stagingWorld[i].alloc);
        vmaDestroyBuffer (VMAllocator, gpuParticles[i].buffer, gpuParticles[i].alloc);
        vmaDestroyBuffer (VMAllocator, uniform[i].buffer, uniform[i].alloc);
        vmaDestroyBuffer (VMAllocator, lightUniform[i].buffer, lightUniform[i].alloc);
        vmaDestroyBuffer (VMAllocator, aoLutUniform[i].buffer, aoLutUniform[i].alloc);
        vmaDestroyBuffer (VMAllocator, stagingRadianceUpdates[i].buffer, stagingRadianceUpdates[i].alloc);
    }
    vmaDestroyBuffer (VMAllocator, gpuRadianceUpdates.buffer, gpuRadianceUpdates.alloc);
    vkDestroyDescriptorPool (device, descriptorPool, NULL);
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vkDestroySemaphore (device, imageAvailableSemaphores[i], NULL);
        vkDestroySemaphore (device, renderFinishedSemaphores[i], NULL);
        vkDestroyFence (device, frameInFlightFences[i], NULL);
        vkDestroyQueryPool (device, queryPoolTimestamps[i], NULL);
    }
    vkDestroyCommandPool (device, commandPool, NULL);
println
    vkDestroyRenderPass (device, lightmapRpass, NULL);
    vkDestroyRenderPass (device, gbufferRpass , NULL);
    vkDestroyRenderPass (device, shadeRpass   , NULL);
println
    destroyComputePipeline ( &mapPipe);
    vkDestroyDescriptorSetLayout (device, mapPushLayout, NULL);
    destroyComputePipeline ( &raytracePipe);
    destroyComputePipeline ( &radiancePipe);
    destroyComputePipeline ( &updateGrassPipe);
    destroyComputePipeline ( &updateWaterPipe);
    destroyComputePipeline ( &genPerlin2dPipe);
    destroyComputePipeline ( &genPerlin3dPipe);
    // destroyComputePipeline(   &dfxPipe);
    // destroyComputePipeline(   &dfyPipe);
    // destroyComputePipeline(   &dfzPipe);
    // destroyComputePipeline(   &bitmaskPipe);
    destroyRasterPipeline (&lightmapBlocksPipe);
    destroyRasterPipeline (&lightmapModelsPipe);
    vkDestroyDescriptorSetLayout (device, raygenModelsPushLayout, NULL);
    destroyRasterPipeline (&raygenBlocksPipe);
    destroyRasterPipeline (&raygenModelsPipe);
    destroyRasterPipeline (&raygenParticlesPipe);
    destroyRasterPipeline (&raygenGrassPipe);
    destroyRasterPipeline (&raygenWaterPipe);
    destroyRasterPipeline (&diffusePipe);
    destroyRasterPipeline (&aoPipe);
    destroyRasterPipeline (&fillStencilSmokePipe);
    destroyRasterPipeline (&fillStencilGlossyPipe);
    destroyRasterPipeline (&glossyPipe);
    destroyRasterPipeline (&smokePipe);
    destroyRasterPipeline (&tonemapPipe);
    destroyRasterPipeline (&overlayPipe);
    cleanupSwapchainDependent();
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT + 1; i++) {
        processDeletionQueue();
        // char* dump;
        // vmaBuildStatsString(VMAllocator, &dump, 1);
        // cout << dump << std::fflush(stdout);
        // fflush(stdout);
        // vmaFreeStatsString(VMAllocator, dump);
    }
    //do before destroyDevice
    vmaDestroyAllocator (VMAllocator);
    vkDestroyDevice (device, NULL);
    //"unpicking" physical device is unnessesary :)
    vkDestroySurfaceKHR (instance, surface, NULL);
#ifndef VKNDEBUG
    vkDestroyDebugUtilsMessengerEXT (instance, debugMessenger, NULL);
#endif
    vkDestroyInstance (instance, NULL);
    glfwDestroyWindow (window.pointer);
}

void Renderer::createSwapchain() {
    SwapChainSupportDetails swapChainSupport = querySwapchainSupport (physicalDevice);
    VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat (swapChainSupport.formats);
    VkPresentModeKHR presentMode = chooseSwapPresentMode (swapChainSupport.presentModes);
    VkExtent2D extent = chooseSwapExtent (swapChainSupport.capabilities);
    u32 imageCount = swapChainSupport.capabilities.minImageCount + 1;
    if (swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
        imageCount = swapChainSupport.capabilities.maxImageCount;
    }
    VkSwapchainCreateInfoKHR createInfo = {VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR};
        createInfo.surface = surface;
        createInfo.minImageCount = imageCount;
        createInfo.imageFormat = surfaceFormat.format;
        createInfo.imageColorSpace = surfaceFormat.colorSpace;
        createInfo.imageExtent = extent;
        createInfo.imageArrayLayers = 1;
        createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    QueueFamilyIndices indices = findQueueFamilies (physicalDevice);
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
    VK_CHECK (vkCreateSwapchainKHR (device, &createInfo, NULL, &swapchain));
    vkGetSwapchainImagesKHR (device, swapchain, &imageCount, NULL);
    swapchainImages.resize (imageCount);
    vector<VkImage> imgs (imageCount);
    vkGetSwapchainImagesKHR (device, swapchain, &imageCount, imgs.data());
    for (int i = 0; i < imageCount; i++) {
        swapchainImages[i].image = imgs[i];
    }
    swapChainImageFormat = surfaceFormat.format;
    swapChainExtent = extent;
    raytraceExtent.width = swapChainExtent.width / settings.ratio.x;
    raytraceExtent.height = swapChainExtent.height / settings.ratio.y;
}

void Renderer::cleanupSwapchainDependent() {
    deleteImages (&highresFrame);
    deleteImages (&highresDepthStencil);
    vkDestroyImageView (device, stencilViewForDS, NULL);
    deleteImages (&highresMatNorm);
    deleteImages (&farDepth);
    deleteImages (&nearDepth);
    for (auto framebuffer : rayGenFramebuffers) {
        vkDestroyFramebuffer (device, framebuffer, NULL);
    }
    for (auto framebuffer : lightmapFramebuffers) {
        vkDestroyFramebuffer (device, framebuffer, NULL);
    }
    for (auto framebuffer : altFramebuffers) {
        vkDestroyFramebuffer (device, framebuffer, NULL);
    }
    for (auto img : swapchainImages) {
        vkDestroyImageView (device, img.view, NULL);
    }
    vkDestroySwapchainKHR (device, swapchain, NULL);
}

void Renderer::createSwapchainImageViews() {
    for (i32 i = 0; i < swapchainImages.size(); i++) {
        VkImageViewCreateInfo createInfo = {VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO};
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
        swapchainImages[i].format = swapChainImageFormat;
        VK_CHECK (vkCreateImageView (device, &createInfo, NULL, &swapchainImages[i].view));
    }
}

void Renderer::createNFramebuffers (vector<VkFramebuffer>* framebuffers, vector<vector<VkImageView>>* views, VkRenderPass renderPass, u32 N, u32 Width, u32 Height) {
    (*framebuffers).resize (N);
    for (u32 i = 0; i < N; i++) {
        vector<VkImageView> attachments = {};
        for (auto viewVec : *views) {
            attachments.push_back (viewVec[i]);
        }
        VkFramebufferCreateInfo framebufferInfo = {VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO};
            framebufferInfo.renderPass = renderPass;
            framebufferInfo.attachmentCount = attachments.size();
            framebufferInfo.pAttachments = attachments.data();
            framebufferInfo.width = Width;
            framebufferInfo.height = Height;
            framebufferInfo.layers = 1;
        VK_CHECK (vkCreateFramebuffer (device, &framebufferInfo, NULL, & (*framebuffers)[i]));
    }
}

void Renderer::processDeletionQueue() {
    int write_index = 0;
    write_index = 0;
    for (int i = 0; i < imageDeletionQueue.size(); i++) {
        bool should_keep = imageDeletionQueue[i].life_counter > 0;
        if (should_keep) {
            imageDeletionQueue[write_index] = imageDeletionQueue[i];
            imageDeletionQueue[write_index].life_counter -= 1;
            write_index++;
        } else {
            vmaDestroyImage (VMAllocator, imageDeletionQueue[i].image.image, imageDeletionQueue[i].image.alloc);
            vkDestroyImageView (device, imageDeletionQueue[i].image.view, NULL);
        }
    }
    imageDeletionQueue.resize (write_index);
    write_index = 0;
    for (int i = 0; i < bufferDeletionQueue.size(); i++) {
        bool should_keep = bufferDeletionQueue[i].life_counter > 0;
        if (should_keep) {
            bufferDeletionQueue[write_index] = bufferDeletionQueue[i];
            bufferDeletionQueue[write_index].life_counter -= 1;
            write_index++;
        } else {
            vmaDestroyBuffer (VMAllocator, bufferDeletionQueue[i].buffer.buffer, bufferDeletionQueue[i].buffer.alloc);
        }
    }
    bufferDeletionQueue.resize (write_index);
}

void Renderer::gen_perlin_2d() {
    VkCommandBuffer commandBuffer = beginSingleTimeCommands();
    vkCmdBindPipeline (commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, genPerlin2dPipe.line);
    vkCmdBindDescriptorSets (commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, genPerlin2dPipe.lineLayout, 0, 1, &genPerlin2dPipe.sets[currentFrame], 0, 0);
    vkCmdDispatch (commandBuffer, settings.world_size.x / 8, settings.world_size.y / 8, 1);
    cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
        perlinNoise2d);
    endSingleTimeCommands (commandBuffer);
}

void Renderer::gen_perlin_3d() {
    VkCommandBuffer commandBuffer = beginSingleTimeCommands();
    vkCmdBindPipeline (commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, genPerlin3dPipe.line);
    vkCmdBindDescriptorSets (commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, genPerlin3dPipe.lineLayout, 0, 1, &genPerlin3dPipe.sets[currentFrame], 0, 0);
    vkCmdDispatch (commandBuffer, 64 / 4, 64 / 4, 64 / 4);
    cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
        perlinNoise3d);
    endSingleTimeCommands (commandBuffer);
}

void Camera::updateCamera() {
    dvec3 up = glm::dvec3 (0.0f, 0.0f, 1.0f); // Up vector
    viewSize = originViewSize / pixelsInVoxel;
    dmat4 view = glm::lookAt (cameraPos, cameraPos + cameraDir, up);
    dmat4 projection = glm::ortho (-viewSize.x / 2.0, viewSize.x / 2.0, viewSize.y / 2.0, -viewSize.y / 2.0, -0.0, +2000.0); // => *(2000.0/2) for decoding
    dmat4 worldToScreen = projection * view;
    cameraTransform = worldToScreen;

    cameraRayDirPlane = normalize (dvec3 (cameraDir.x, cameraDir.y, 0));
    horizline = normalize (cross (cameraRayDirPlane, dvec3 (0, 0, 1)));
    vertiline = normalize (cross (cameraDir, horizline));
}

void Renderer::update_light_transform() {
    dvec3 horizon = normalize (cross (dvec3 (1, 0, 0), lightDir));
    dvec3 up = normalize (cross (horizon, lightDir)); // Up vector
    // dvec3 light_pos = dvec3(dvec2(settings.world_size*16),0)/2.0 - 5*16.0*lightDir;
    dvec3 light_pos = dvec3 (dvec2 (settings.world_size * 16), 0) / 2.0 - 1 * 16.0 * lightDir;
    dmat4 view = glm::lookAt (light_pos, light_pos + lightDir, up);
    const double voxel_in_pixels = 5.0; //we want voxel to be around 10 pixels in width / height
    const double view_width_in_voxels = 3000.0 / voxel_in_pixels; //todo make dynamic and use spec constnats
    const double view_height_in_voxels = 3000.0 / voxel_in_pixels;
    dmat4 projection = glm::ortho (-view_width_in_voxels / 2.0, view_width_in_voxels / 2.0, view_height_in_voxels / 2.0, -view_height_in_voxels / 2.0, -350.0, +350.0); // => *(2000.0/2) for decoding
    dmat4 worldToScreen = projection * view;
    lightTransform = worldToScreen;
}

// #include <glm/gtx/string_cast.hpp>
void Renderer::start_frame() {
    camera.updateCamera();
    update_light_transform();
    VK_CHECK (vkWaitForFences (device, 1, &frameInFlightFences[currentFrame], VK_TRUE, UINT32_MAX));
    VK_CHECK (vkResetFences (device, 1, &frameInFlightFences[currentFrame]));
    assert(computeCommandBuffers[currentFrame]);
    assert(graphicsCommandBuffers[currentFrame]);
    assert(copyCommandBuffers[currentFrame]);
    assert(lightmapCommandBuffers[currentFrame]);
    VK_CHECK (vkResetCommandBuffer (computeCommandBuffers[currentFrame], /*VkCommandBufferResetFlagBits*/ 0));
    VK_CHECK (vkResetCommandBuffer (graphicsCommandBuffers[currentFrame], /*VkCommandBufferResetFlagBits*/ 0));
    VK_CHECK (vkResetCommandBuffer (copyCommandBuffers[currentFrame], /*VkCommandBufferResetFlagBits*/ 0));
    VK_CHECK (vkResetCommandBuffer (lightmapCommandBuffers[currentFrame], /*VkCommandBufferResetFlagBits*/ 0));
    VkCommandBufferBeginInfo 
        beginInfo = {};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = 0;
        beginInfo.pInheritanceInfo = NULL;
    VK_CHECK (vkBeginCommandBuffer (computeCommandBuffers[currentFrame], &beginInfo));
    VK_CHECK (vkBeginCommandBuffer (lightmapCommandBuffers[currentFrame], &beginInfo));
    VK_CHECK (vkBeginCommandBuffer (copyCommandBuffers[currentFrame], &beginInfo));
    VK_CHECK (vkBeginCommandBuffer (graphicsCommandBuffers[currentFrame], &beginInfo));
}

void Renderer::start_compute() {
    VkCommandBuffer& commandBuffer = computeCommandBuffers[currentFrame];
    vkCmdResetQueryPool (commandBuffer, queryPoolTimestamps[currentFrame], 0, timestampCount);
    currentTimestamp = 0;
}

void Renderer::start_blockify() {
    VkCommandBuffer& commandBuffer = computeCommandBuffers[currentFrame];
    blockCopyQueue.clear();
    paletteCounter = settings.static_block_palette_size;
    size_t size_to_copy = settings.world_size.x * settings.world_size.y * settings.world_size.z * sizeof (BlockID_t);
    memcpy (current_world.data(), origin_world.data(), size_to_copy);
}

struct AABB {
    vec3 min;
    vec3 max;
};

static AABB get_shift (dmat4 trans, ivec3 size) {
    vec3 box = vec3 (size - 1);
    vec3 zero = vec3 (0);
    vec3 corners[8];
    corners[0] = zero;
    corners[1] = vec3 (zero.x, box.y, zero.z);
    corners[2] = vec3 (zero.x, box.y, box.z);
    corners[3] = vec3 (zero.x, zero.y, box.z);
    corners[4] = vec3 (box.x, zero.y, zero.z);
    corners[5] = vec3 (box.x, box.y, zero.z);
    corners[6] = box;
    corners[7] = vec3 (box.x, zero.y, box.z);
    // transform the first corner
    vec3 tmin = vec3 (trans* vec4 (corners[0], 1.0));
    vec3 tmax = tmin;
    // transform the other 7 corners and compute the result AABB
    for (int i = 1; i < 8; i++) {
        vec3 point = vec3 (trans* vec4 (corners[i], 1.0));
        tmin = min (tmin, point);
        tmax = max (tmax, point);
    }
    AABB rbox;
    rbox.min = tmin;
    rbox.max = tmax;
    return rbox;
}

// allocates temp block in palette for every block that intersects with every mesh blockified
void Renderer::blockify_mesh (Mesh* mesh) {
    mat4 rotate = toMat4 ((*mesh).rot);
    mat4 shift = translate (identity<mat4>(), (*mesh).shift);
    struct AABB border_in_voxel = get_shift (shift* rotate, (*mesh).size);
    struct {ivec3 min; ivec3 max;} border;
    border.min = (ivec3 (border_in_voxel.min) - ivec3 (1)) / ivec3 (16);
    border.max = (ivec3 (border_in_voxel.max) + ivec3 (1)) / ivec3 (16);
    border.min = glm::clamp (border.min, ivec3 (0), ivec3 (settings.world_size - 1));
    border.max = glm::clamp (border.max, ivec3 (0), ivec3 (settings.world_size - 1));
    for (int xx = border.min.x; xx <= border.max.x; xx++) {
        for (int yy = border.min.y; yy <= border.max.y; yy++) {
            for (int zz = border.min.z; zz <= border.max.z; zz++) {
                //if inside model TODO
                //zero clear TODO
                int current_block = current_world (xx, yy, zz);
                if (current_block < settings.static_block_palette_size) { // static
                    //add to copy queue
                    ivec2 src_block = {};
                    tie (src_block.x, src_block.y) = get_block_xy (current_block);
                    ivec2 dst_block = {};
                    tie (dst_block.x, dst_block.y) = get_block_xy (paletteCounter);
                    VkImageCopy static_block_copy = {};
                        static_block_copy.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                        static_block_copy.srcSubresource.baseArrayLayer = 0;
                        static_block_copy.srcSubresource.layerCount = 1;
                        static_block_copy.srcSubresource.mipLevel = 0;
                        static_block_copy.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
                        static_block_copy.dstSubresource.baseArrayLayer = 0;
                        static_block_copy.dstSubresource.layerCount = 1;
                        static_block_copy.dstSubresource.mipLevel = 0;
                        static_block_copy.extent = {16, 16, 16};
                        static_block_copy.srcOffset = {src_block.x * 16, src_block.y * 16, 0};
                        static_block_copy.dstOffset = {dst_block.x * 16, dst_block.y * 16, 0};
                    blockCopyQueue.push_back (static_block_copy);
                    current_world (xx, yy, zz) = paletteCounter;
                    paletteCounter++; // yeah i dont trust myself in this
                } else {
                    //already new block, just leave it
                }
            }
        }
    }
}

void Renderer::end_blockify() {
    size_t size_to_copy = settings.world_size.x * settings.world_size.y * settings.world_size.z * sizeof (BlockID_t);
    assert (size_to_copy != 0);
    memcpy (stagingWorldMapped[currentFrame], current_world.data(), size_to_copy);
    vmaFlushAllocation (VMAllocator, stagingWorld[currentFrame].alloc, 0, size_to_copy);
}

void Renderer::blockify_custom (void* ptr) {
    size_t size_to_copy = settings.world_size.x * settings.world_size.y * settings.world_size.z * sizeof (BlockID_t);
    memcpy (stagingWorldMapped[currentFrame], ptr, size_to_copy);
    vmaFlushAllocation (VMAllocator, stagingWorld[currentFrame].alloc, 0, size_to_copy);
}

#include <glm/gtx/hash.hpp>
void Renderer::update_radiance() {
    VkCommandBuffer& commandBuffer = computeCommandBuffers[currentFrame];
    robin_hood::unordered_set<i8vec3> set = {};
    radianceUpdates.clear();
    for (int xx = 0; xx < settings.world_size.x; xx++) {
        for (int yy = 0; yy < settings.world_size.y; yy++) {
            for (int zz = 0; zz < settings.world_size.z; zz++) {
                // int block_id = current_world(xx,yy,zz);
                //yeah kinda slow... but who cares on less then a million blocks?
                int sum_of_neighbours = 0;
                for (int dx = -1; (dx <= +1); dx++) {
                    for (int dy = -1; (dy <= +1); dy++) {
                        for (int dz = -1; (dz <= +1); dz++) {
                            ivec3 test_block = ivec3 (xx + dx, yy + dy, zz + dz);
                            // test_block = clamp(test_block, ivec3(0), settings.world_size-1);
                            sum_of_neighbours += current_world (test_block);
                        }
                    }
                }
                // TODO: finish dynamic update system, integrate with RaVE
                if (sum_of_neighbours != 0) {
                    radianceUpdates.push_back (i8vec4 (xx, yy, zz, 0));
                    set.insert (i8vec3 (xx, yy, zz));
                }
            }
        }
    }
    for (auto u : specialRadianceUpdates) {
        if (!set.contains (u)) {
            radianceUpdates.push_back (u);
        }
    } specialRadianceUpdates.clear();
    VkDeviceSize bufferSize = sizeof (radianceUpdates[0]) * radianceUpdates.size();
    memcpy (stagingRadianceUpdatesMapped[currentFrame], radianceUpdates.data(), bufferSize);

    VkBufferCopy
        copyRegion = {};
        copyRegion.size = bufferSize;
    vkCmdCopyBuffer (commandBuffer, stagingRadianceUpdates[currentFrame].buffer, gpuRadianceUpdates.buffer, 1, &copyRegion);
    cmdPipelineBarrier (commandBuffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
        gpuRadianceUpdates);
    vkCmdBindPipeline (commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, radiancePipe.line);
    vkCmdBindDescriptorSets (commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, radiancePipe.lineLayout, 0, 1, &radiancePipe.sets[currentFrame], 0, 0);
    int magic_number = iFrame % 2;
    //if fast than increase work
    if (timeTakenByRadiance < 1.8) {
        magicSize --;
        //if slow than decrease work
    } else if (timeTakenByRadiance > 2.2) {
        magicSize ++;
    }
    magicSize = glm::max (magicSize, 1); //never remove
    magicSize = glm::min (magicSize, 10);
    struct rtpc {int time, iters, size, shift;} pushconstant = {iFrame, 0, magicSize, iFrame % magicSize};
    vkCmdPushConstants (commandBuffer, radiancePipe.lineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof (pushconstant), &pushconstant);
    int wg_count = radianceUpdates.size() / magicSize;
    PLACE_TIMESTAMP_ALWAYS();
    vkCmdDispatch (commandBuffer, wg_count, 1, 1);
    PLACE_TIMESTAMP_ALWAYS();
    cmdPipelineBarrier (commandBuffer,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_WRITE_BIT,
        radianceCache);
    cmdPipelineBarrier (commandBuffer,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
        radianceCache);
}

void Renderer::recalculate_df() {
    VkCommandBuffer& commandBuffer = computeCommandBuffers[currentFrame];
    cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
        distancePalette);
    vkCmdBindPipeline (commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, dfxPipe.line);
    vkCmdBindDescriptorSets (commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, dfxPipe.lineLayout, 0, 1, &dfxPipe.sets[currentFrame], 0, 0);
    vkCmdDispatch (commandBuffer, 1, 1, 16 * paletteCounter);
    cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
        distancePalette);
    vkCmdBindPipeline (commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, dfyPipe.line);
    vkCmdBindDescriptorSets (commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, dfyPipe.lineLayout, 0, 1, &dfyPipe.sets[currentFrame], 0, 0);
    vkCmdDispatch (commandBuffer, 1, 1, 16 * paletteCounter);
    cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
        distancePalette);
    vkCmdBindPipeline (commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, dfzPipe.line);
    vkCmdBindDescriptorSets (commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, dfzPipe.lineLayout, 0, 1, &dfzPipe.sets[currentFrame], 0, 0);
    vkCmdDispatch (commandBuffer, 1, 1, 16 * paletteCounter);
    cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
        distancePalette);
}

void Renderer::recalculate_bit() {
    VkCommandBuffer& commandBuffer = computeCommandBuffers[currentFrame];
    cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
        bitPalette);
    vkCmdBindPipeline (commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, bitmaskPipe.line);
    vkCmdBindDescriptorSets (commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, bitmaskPipe.lineLayout, 0, 1, &bitmaskPipe.sets[currentFrame], 0, 0);
    vkCmdDispatch (commandBuffer, 1, 1, 8 * paletteCounter);
    cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
        bitPalette);
}

void Renderer::exec_copies() {
    VkCommandBuffer& commandBuffer = computeCommandBuffers[currentFrame];
    if (blockCopyQueue.size() != 0) {
        //we can copy from previous image, cause static blocks are same in both palettes. Additionaly, it gives src and dst optimal layouts
        cmdTransLayoutBarrier (commandBuffer, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
            &originBlockPalette[previousFrame]);
        cmdTransLayoutBarrier (commandBuffer, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
            &originBlockPalette[ currentFrame]);
        PLACE_TIMESTAMP();
        vkCmdCopyImage (commandBuffer, originBlockPalette[previousFrame].image, VK_IMAGE_LAYOUT_GENERAL, originBlockPalette[currentFrame].image, VK_IMAGE_LAYOUT_GENERAL, blockCopyQueue.size(), blockCopyQueue.data());
        PLACE_TIMESTAMP();
        cmdTransLayoutBarrier (commandBuffer, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
            &originBlockPalette[previousFrame]);
        cmdTransLayoutBarrier (commandBuffer, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
            &originBlockPalette[ currentFrame]);
    }
    VkBufferImageCopy copyRegion = {};
        copyRegion.imageExtent.width = settings.world_size.x;
        copyRegion.imageExtent.height = settings.world_size.y;
        copyRegion.imageExtent.depth = settings.world_size.z;
        copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copyRegion.imageSubresource.layerCount = 1;
    cmdTransLayoutBarrier (commandBuffer, VK_IMAGE_LAYOUT_GENERAL,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
        &world);
    vkCmdCopyBufferToImage (commandBuffer, stagingWorld[currentFrame].buffer, world.image, VK_IMAGE_LAYOUT_GENERAL, 1, &copyRegion);
    cmdTransLayoutBarrier (commandBuffer, VK_IMAGE_LAYOUT_GENERAL,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
        &world);
}

void Renderer::start_map() {
    VkCommandBuffer& commandBuffer = computeCommandBuffers[currentFrame];
    vkCmdBindPipeline (commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, mapPipe.line);
    vkCmdBindDescriptorSets (commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, mapPipe.lineLayout, 0, 1, &mapPipe.sets[currentFrame], 0, 0);
    PLACE_TIMESTAMP();
}

void Renderer::map_mesh (Mesh* mesh) {
    VkCommandBuffer& commandBuffer = computeCommandBuffers[currentFrame];
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
    vkCmdPushDescriptorSetKHR (commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, mapPipe.lineLayout, 1, descriptorWrites.size(), descriptorWrites.data());
    dmat4 rotate = toMat4 ((*mesh).rot);
    dmat4 shift = translate (identity<mat4>(), (*mesh).shift);
    dmat4 transform = shift * rotate;
    struct AABB border_in_voxel = get_shift (transform, (*mesh).size);
    struct {ivec3 min; ivec3 max;} border;
    border.min = ivec3 (floor (border_in_voxel.min)) - ivec3 (0); // no -ivec3(1) cause IT STRETCHES MODELS;
    border.max = ivec3 ( ceil (border_in_voxel.max)) + ivec3 (0); // no +ivec3(1) cause IT STRETCHES MODELS;
    //todo: clamp here not in shader
    ivec3 map_area = (border.max - border.min);
    struct {mat4 trans; ivec4 shift;} itrans_shift = {mat4 (inverse (transform)), ivec4 (border.min, 0)};
    vkCmdPushConstants (commandBuffer, mapPipe.lineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof (itrans_shift), &itrans_shift);
    // ivec3 adjusted_size = ivec3(vec3(mesh.size) * vec3(2.0));
    vkCmdDispatch (commandBuffer, (map_area.x * 3 + 3) / 4, (map_area.y * 3 + 3) / 4, (map_area.z * 3 + 3) / 4);
}

void Renderer::end_map() {
    VkCommandBuffer& commandBuffer = computeCommandBuffers[currentFrame];
    cmdTransLayoutBarrier (commandBuffer, VK_IMAGE_LAYOUT_GENERAL,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
        &originBlockPalette[currentFrame]);
    PLACE_TIMESTAMP();
}

void Renderer::end_compute() {
    VkCommandBuffer& commandBuffer = computeCommandBuffers[currentFrame];
}

void Renderer::start_lightmap() {
    VkCommandBuffer& commandBuffer = lightmapCommandBuffers[currentFrame];
    PLACE_TIMESTAMP();
    struct unicopy {mat4 trans;} unicopy = {lightTransform};
    vkCmdUpdateBuffer (commandBuffer, lightUniform[currentFrame].buffer, 0, sizeof (unicopy), &unicopy);
    cmdPipelineBarrier (commandBuffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT,
        lightUniform[currentFrame]);
    VkClearValue clear_depth = {};
    clear_depth.depthStencil.depth = 1;
    vector<VkClearValue> clearColors = {
        clear_depth
    };
    VkRenderPassBeginInfo 
        renderPassInfo = {VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO};
        renderPassInfo.renderPass = lightmapRpass;
        renderPassInfo.framebuffer = lightmapFramebuffers[currentFrame];
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = lightmapExtent;
        renderPassInfo.clearValueCount = clearColors.size();
        renderPassInfo.pClearValues = clearColors.data();
    vkCmdBeginRenderPass (commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    cmdSetViewport(commandBuffer, lightmapExtent);
}

void Renderer::lightmap_start_blocks() {
    VkCommandBuffer& commandBuffer = lightmapCommandBuffers[currentFrame];
    vkCmdBindPipeline (commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, lightmapBlocksPipe.line);
    vkCmdBindDescriptorSets (commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, lightmapBlocksPipe.lineLayout, 0, 1, &lightmapBlocksPipe.sets[currentFrame], 0, 0);
}

void Renderer::lightmap_start_models() {
    VkCommandBuffer& commandBuffer = lightmapCommandBuffers[currentFrame];
    vkCmdBindPipeline (commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, lightmapModelsPipe.line);
    vkCmdBindDescriptorSets (commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, lightmapModelsPipe.lineLayout, 0, 1, &lightmapModelsPipe.sets[currentFrame], 0, 0);
}

void Renderer::end_lightmap() {
    VkCommandBuffer& commandBuffer = lightmapCommandBuffers[currentFrame];
    PLACE_TIMESTAMP();
    vkCmdEndRenderPass (commandBuffer);
}

void Renderer::start_raygen() {
    VkCommandBuffer& commandBuffer = graphicsCommandBuffers[currentFrame];
    PLACE_TIMESTAMP();
    
    struct unicopy {
        mat4 trans_w2s; vec4 campos; vec4 camdir;
        vec4 horizline_scaled; vec4 vertiline_scaled;
        vec4 globalLightDir; mat4 lightmap_proj;
        vec2 size;
        int timeseed;
    } unicopy = {
        mat4 (camera.cameraTransform), vec4 (camera.cameraPos, 0), vec4 (camera.cameraDir, 0),
        vec4 (camera.horizline* camera.viewSize.x / 2.0, 0), vec4 (camera.vertiline* camera.viewSize.y / 2.0, 0),
        vec4 (lightDir, 0), mat4 (lightTransform),
        vec2 (swapChainExtent.width, swapChainExtent.height),
        iFrame
    };
    vkCmdUpdateBuffer (commandBuffer, uniform[currentFrame].buffer, 0, sizeof (unicopy), &unicopy);
    cmdPipelineBarrier (commandBuffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT,
        uniform[currentFrame]);
    VkClearValue clear_depth = {};
        clear_depth.depthStencil.depth = 1;
    vector<VkClearValue> clearColors = {
        {},
        clear_depth //i hate my life why did i forget it
    };
    VkRenderPassBeginInfo 
        renderPassInfo = {};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = gbufferRpass;
        renderPassInfo.framebuffer = rayGenFramebuffers[currentFrame];
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = swapChainExtent;
        renderPassInfo.clearValueCount = clearColors.size();
        renderPassInfo.pClearValues = clearColors.data();
    vkCmdBeginRenderPass (commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    cmdSetViewport(commandBuffer, raytraceExtent);
}

void Renderer::raygen_start_blocks() {
    VkCommandBuffer& commandBuffer = graphicsCommandBuffers[currentFrame];
    PLACE_TIMESTAMP();
    vkCmdBindPipeline (commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, raygenBlocksPipe.line);
    vkCmdBindDescriptorSets (commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, raygenBlocksPipe.lineLayout, 0, 1, &raygenBlocksPipe.sets[currentFrame], 0, 0);
}

static VkBuffer old_buff = NULL;
static bool is_face_visible (vec3 normal, vec3 camera_dir) {
    return (dot (normal, camera_dir) < 0.0f);
}

#define CHECK_N_DRAW_BLOCK(__norm, __dir) \
if(is_face_visible(i8vec3(__norm), camera.cameraDir)) {\
    draw_block_face(__norm, (*block_mesh).triangles.__dir, block_id);\
}
void Renderer::draw_block_face (i8vec3 normal, IndexedVertices& buff, int block_id) {
    VkCommandBuffer& commandBuffer = graphicsCommandBuffers[currentFrame];
    assert (buff.indexes.data());
    assert (block_id);
    vkCmdBindIndexBuffer (commandBuffer, buff.indexes[currentFrame].buffer, 0, VK_INDEX_TYPE_UINT16);
    i8 sum = normal.x + normal.y + normal.z;
    u8 sign = (sum > 0) ? 0 : 1;
    u8vec3 absnorm = abs (normal);
    // assert(sign != 0);
    assert ((absnorm.x + absnorm.y + absnorm.z) == 1);
    u8 pbn = (
                 sign << 7 |
                 absnorm.x << 0 |
                 absnorm.y << 1 |
                 absnorm.z << 2);
    //signBit_4EmptyBits_xBit_yBit_zBit
    struct {u8vec4 inorm;} norms = {u8vec4 (pbn, 0, 0, 0)};
    vkCmdPushConstants (commandBuffer, raygenBlocksPipe.lineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        8, sizeof (norms), &norms);
    vkCmdDrawIndexed (commandBuffer, buff.icount, 1, 0, 0, 0);
}

void Renderer::raygen_block (Mesh* block_mesh, int block_id, ivec3 shift) {
    VkCommandBuffer& commandBuffer = graphicsCommandBuffers[currentFrame];
    VkBuffer vertexBuffers[] = { (*block_mesh).triangles.vertexes[currentFrame].buffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers (commandBuffer, 0, 1, vertexBuffers, offsets);
    /*
        int16_t block;
        i16vec3 shift;
        i8vec4 inorm;
    */
    struct {i16 block; i16vec3 shift;} blockshift = {i16 (block_id), i16vec3 (shift)};
    vkCmdPushConstants (commandBuffer, raygenBlocksPipe.lineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        0, sizeof (blockshift), &blockshift);
    CHECK_N_DRAW_BLOCK (i8vec3 (+1, 0, 0), Pzz);
    CHECK_N_DRAW_BLOCK (i8vec3 (-1, 0, 0), Nzz);
    CHECK_N_DRAW_BLOCK (i8vec3 (0, +1, 0), zPz);
    CHECK_N_DRAW_BLOCK (i8vec3 (0, -1, 0), zNz);
    CHECK_N_DRAW_BLOCK (i8vec3 (0, 0, +1), zzP);
    CHECK_N_DRAW_BLOCK (i8vec3 (0, 0, -1), zzN);
}

void Renderer::raygen_start_models() {
    VkCommandBuffer& commandBuffer = graphicsCommandBuffers[currentFrame];
    vkCmdNextSubpass (commandBuffer, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline (commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, raygenModelsPipe.line);
    vkCmdBindDescriptorSets (commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, raygenModelsPipe.lineLayout, 0, 1, &raygenModelsPipe.sets[currentFrame], 0, 0);
}

#define CHECK_N_DRAW_MODEL(__norm, __dir) \
if(is_face_visible(model_mesh->rot*__norm, camera.cameraDir)) {\
    draw_model_face(__norm, (*model_mesh).triangles.__dir);\
}
void Renderer::draw_model_face (vec3 normal, IndexedVertices& buff) {
    VkCommandBuffer& commandBuffer = graphicsCommandBuffers[currentFrame];
    assert (buff.indexes.data());
    vkCmdBindIndexBuffer (commandBuffer, buff.indexes[currentFrame].buffer, 0, VK_INDEX_TYPE_UINT16);
    struct {vec4 fnorm;} norms = {vec4 (normal, 0)};
    vkCmdPushConstants (commandBuffer, raygenModelsPipe.lineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        32, sizeof (norms), &norms);
    vkCmdDrawIndexed (commandBuffer, buff.icount, 1, 0, 0, 0);
}

void Renderer::raygen_model (Mesh* model_mesh) {
    VkCommandBuffer& commandBuffer = graphicsCommandBuffers[currentFrame];
    VkBuffer vertexBuffers[] = { (*model_mesh).triangles.vertexes[currentFrame].buffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers (commandBuffer, 0, 1, vertexBuffers, offsets);
    /*
        vec4 rot;
        vec4 shift;
        vec4 fnormal; //not encoded
    */
    struct {quat rot; vec4 shift;} rotshift = {model_mesh->rot, vec4 (model_mesh->shift, 0)};
    vkCmdPushConstants (commandBuffer, raygenModelsPipe.lineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        0, sizeof (rotshift), &rotshift);
    VkDescriptorImageInfo
        modelVoxelsInfo = {};
        modelVoxelsInfo.imageView = (*model_mesh).voxels[currentFrame].view; //CHANGE ME
        modelVoxelsInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        modelVoxelsInfo.sampler = unnormNearest;
    VkWriteDescriptorSet
        modelVoxelsWrite = {VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET};
        modelVoxelsWrite.dstSet = NULL;
        modelVoxelsWrite.dstBinding = 0;
        modelVoxelsWrite.dstArrayElement = 0;
        modelVoxelsWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        modelVoxelsWrite.descriptorCount = 1;
        modelVoxelsWrite.pImageInfo = &modelVoxelsInfo;
    vector<VkWriteDescriptorSet> descriptorWrites = {modelVoxelsWrite};
    vkCmdPushDescriptorSetKHR (commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, raygenModelsPipe.lineLayout, 1, descriptorWrites.size(), descriptorWrites.data());
    CHECK_N_DRAW_MODEL (vec3 (+1, 0, 0), Pzz);
    CHECK_N_DRAW_MODEL (vec3 (-1, 0, 0), Nzz);
    CHECK_N_DRAW_MODEL (vec3 (0, +1, 0), zPz);
    CHECK_N_DRAW_MODEL (vec3 (0, -1, 0), zNz);
    CHECK_N_DRAW_MODEL (vec3 (0, 0, +1), zzP);
    CHECK_N_DRAW_MODEL (vec3 (0, 0, -1), zzN);
}

#define CHECK_N_LIGHTMAP_BLOCK(__norm, __dir) \
if(is_face_visible(i8vec3(__norm), lightDir)){\
    lightmap_block_face(__norm, (*block_mesh).triangles.__dir, block_id);\
}
void Renderer::lightmap_block_face (i8vec3 normal, IndexedVertices& buff, int block_id) {
    VkCommandBuffer& lightmap_commandBuffer = lightmapCommandBuffers[currentFrame];
    vkCmdBindIndexBuffer (lightmap_commandBuffer, buff.indexes[currentFrame].buffer, 0, VK_INDEX_TYPE_UINT16);
    vkCmdDrawIndexed (lightmap_commandBuffer, buff.icount, 1, 0, 0, 0);
}

void Renderer::lightmap_block (Mesh* block_mesh, int block_id, ivec3 shift) {
    VkCommandBuffer& lightmap_commandBuffer = lightmapCommandBuffers[currentFrame];
    VkBuffer vertexBuffers[] = { (*block_mesh).triangles.vertexes[currentFrame].buffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers (lightmap_commandBuffer, 0, 1, vertexBuffers, offsets);
    /*
        int16_t block;
        i16vec3 shift;
        i8vec4 inorm;
    */
    struct {i16vec4 shift;} blockshift = {i16vec4 (shift, 0)};
    vkCmdPushConstants (lightmap_commandBuffer, lightmapBlocksPipe.lineLayout, VK_SHADER_STAGE_VERTEX_BIT,
        0, sizeof (blockshift), &blockshift);
    CHECK_N_LIGHTMAP_BLOCK (i8vec3 (+1, 0, 0), Pzz);
    CHECK_N_LIGHTMAP_BLOCK (i8vec3 (-1, 0, 0), Nzz);
    CHECK_N_LIGHTMAP_BLOCK (i8vec3 (0, +1, 0), zPz);
    CHECK_N_LIGHTMAP_BLOCK (i8vec3 (0, -1, 0), zNz);
    CHECK_N_LIGHTMAP_BLOCK (i8vec3 (0, 0, +1), zzP);
    CHECK_N_LIGHTMAP_BLOCK (i8vec3 (0, 0, -1), zzN);
}

#define CHECK_N_LIGHTMAP_MODEL(__norm, __dir) \
if(is_face_visible(model_mesh->rot*(__norm), lightDir)){\
    lightmap_model_face(__norm, (*model_mesh).triangles.__dir);\
}
void Renderer::lightmap_model_face (vec3 normal, IndexedVertices& buff) {
    VkCommandBuffer& lightmap_commandBuffer = lightmapCommandBuffers[currentFrame];
    vkCmdBindIndexBuffer (lightmap_commandBuffer, buff.indexes[currentFrame].buffer, 0, VK_INDEX_TYPE_UINT16);
    vkCmdDrawIndexed (lightmap_commandBuffer, buff.icount, 1, 0, 0, 0);
}

void Renderer::lightmap_model (Mesh* model_mesh) {
    VkCommandBuffer& lightmap_commandBuffer = lightmapCommandBuffers[currentFrame];
    VkBuffer vertexBuffers[] = { (*model_mesh).triangles.vertexes[currentFrame].buffer};
    VkDeviceSize offsets[] = {0};
    vkCmdBindVertexBuffers (lightmap_commandBuffer, 0, 1, vertexBuffers, offsets);
    /*
        vec4 rot;
        vec4 shift;
        vec4 fnormal; //not encoded
    */
    struct {quat rot; vec4 shift;} rotshift = {model_mesh->rot, vec4 (model_mesh->shift, 0)};
    vkCmdPushConstants (lightmap_commandBuffer, lightmapModelsPipe.lineLayout, VK_SHADER_STAGE_VERTEX_BIT,
        0, sizeof (rotshift), &rotshift);
    CHECK_N_LIGHTMAP_MODEL (vec3 (+1, 0, 0), Pzz);
    CHECK_N_LIGHTMAP_MODEL (vec3 (-1, 0, 0), Nzz);
    CHECK_N_LIGHTMAP_MODEL (vec3 (0, +1, 0), zPz);
    CHECK_N_LIGHTMAP_MODEL (vec3 (0, -1, 0), zNz);
    CHECK_N_LIGHTMAP_MODEL (vec3 (0, 0, +1), zzP);
    CHECK_N_LIGHTMAP_MODEL (vec3 (0, 0, -1), zzN);
}

void Renderer::update_particles() {
    int write_index = 0;
    for (int i = 0; i < particles.size(); i++) {
        bool should_keep = particles[i].lifeTime > 0;
        if (should_keep) {
            particles[write_index] = particles[i];
            // delta_time = 1/75.0;
            particles[write_index].pos += particles[write_index].vel * float (deltaTime);
            particles[write_index].lifeTime -= deltaTime;
            write_index++;
        }
    }
    particles.resize (write_index);
    int capped_particle_count = glm::clamp (int (particles.size()), 0, settings.maxParticleCount);
    memcpy (gpuParticlesMapped[currentFrame], particles.data(), sizeof (Particle)*capped_particle_count);
    vmaFlushAllocation (VMAllocator, gpuParticles[currentFrame].alloc, 0, sizeof (Particle)*capped_particle_count);
}

void Renderer::raygen_map_particles() {
    VkCommandBuffer& commandBuffer = graphicsCommandBuffers[currentFrame];
    //go to next no matter what
    vkCmdNextSubpass (commandBuffer, VK_SUBPASS_CONTENTS_INLINE);
    PLACE_TIMESTAMP();
    if (!particles.empty()) { //just for safity
        vkCmdBindPipeline (commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, raygenParticlesPipe.line);
        vkCmdBindDescriptorSets (commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, raygenParticlesPipe.lineLayout, 0, 1, &raygenParticlesPipe.sets[currentFrame], 0, 0);
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers (commandBuffer, 0, 1, &gpuParticles[currentFrame].buffer, offsets);
        vkCmdDraw (commandBuffer, particles.size(), 1, 0, 0);
    }
    PLACE_TIMESTAMP();
}

void Renderer::raygen_start_grass() {
    VkCommandBuffer& commandBuffer = graphicsCommandBuffers[currentFrame];
    PLACE_TIMESTAMP();
    vkCmdNextSubpass (commandBuffer, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline (commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, raygenGrassPipe.line);
    vkCmdBindDescriptorSets (commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, raygenGrassPipe.lineLayout, 0, 1, &raygenGrassPipe.sets[currentFrame], 0, 0);
}

//DOES NOT run in renderpass. Placed here cause spatially makes sense
void Renderer::updade_grass (vec2 windDirection) {
    VkCommandBuffer& commandBuffer = computeCommandBuffers[currentFrame];
    vkCmdBindPipeline (commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, updateGrassPipe.line);
    struct {vec2 wd, cp; float dt;} pushconstant = {windDirection, {}, float (iFrame)};
    vkCmdPushConstants (commandBuffer, updateGrassPipe.lineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof (pushconstant), &pushconstant);
    vkCmdBindDescriptorSets (commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, updateGrassPipe.lineLayout, 0, 1, &updateGrassPipe.sets[0], 0, 0);
    cmdPipelineBarrier (commandBuffer,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_WRITE_BIT,
        grassState);
    cmdPipelineBarrier (commandBuffer,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
        grassState);
    PLACE_TIMESTAMP();
    vkCmdDispatch (commandBuffer, (settings.world_size.x * 2 + 7) / 8, (settings.world_size.y * 2 + 7) / 8, 1); //2x8 2x8 1x1
    PLACE_TIMESTAMP();
}

void Renderer::updade_water() {
    VkCommandBuffer& commandBuffer = computeCommandBuffers[currentFrame];
    vkCmdBindPipeline (commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, updateWaterPipe.line);
    struct {vec2 wd; float dt;} pushconstant = {{}, float (iFrame)};
    vkCmdPushConstants (commandBuffer, updateWaterPipe.lineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof (pushconstant), &pushconstant);
    vkCmdBindDescriptorSets (commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, updateWaterPipe.lineLayout, 0, 1, &updateWaterPipe.sets[0], 0, 0);
    cmdPipelineBarrier (commandBuffer,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_WRITE_BIT,
        waterState);
    cmdPipelineBarrier (commandBuffer,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
        waterState);
    PLACE_TIMESTAMP();
    vkCmdDispatch (commandBuffer, (settings.world_size.x * 2 + 7) / 8, (settings.world_size.y * 2 + 7) / 8, 1); //2x8 2x8 1x1
    PLACE_TIMESTAMP();
}

//blade is hardcoded but it does not really increase perfomance
//done this way for simplicity, can easilly be replaced with any vertex buffer
void Renderer::raygen_map_grass (vec4 shift, int size) {
    VkCommandBuffer& commandBuffer = graphicsCommandBuffers[currentFrame];
    bool x_flip = camera.cameraDir.x < 0;
    bool y_flip = camera.cameraDir.y < 0;
    struct {vec4 _shift; int _size, _time; int xf, yf;} raygen_pushconstant = {shift, size, iFrame, x_flip, y_flip};
    vkCmdPushConstants (commandBuffer, raygenGrassPipe.lineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof (raygen_pushconstant), &raygen_pushconstant);
    const int verts_per_blade = 6; //for triangle strip
    const int blade_per_instance = 1; //for triangle strip
    vkCmdDraw (commandBuffer,
               verts_per_blade* blade_per_instance,
               (size* size + (blade_per_instance - 1)) / blade_per_instance,
               0, 0);
}

void Renderer::raygen_start_water() {
    VkCommandBuffer& commandBuffer = graphicsCommandBuffers[currentFrame];
    PLACE_TIMESTAMP();
    PLACE_TIMESTAMP();
    vkCmdNextSubpass (commandBuffer, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline (commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, raygenWaterPipe.line);
    vkCmdBindDescriptorSets (commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, raygenWaterPipe.lineLayout, 0, 1, &raygenWaterPipe.sets[currentFrame], 0, 0);
}

void Renderer::raygen_map_water (vec4 shift, int qualitySize) {
    VkCommandBuffer& commandBuffer = graphicsCommandBuffers[currentFrame];
    struct {vec4 _shift; int _size, _time;} raygen_pushconstant = {shift, qualitySize, iFrame};
    vkCmdPushConstants (commandBuffer, raygenWaterPipe.lineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof (raygen_pushconstant), &raygen_pushconstant);
    const int verts_per_water_tape = qualitySize * 2 + 2;
    const int tapes_per_block = qualitySize;
    vkCmdDraw (commandBuffer,
               verts_per_water_tape,
               tapes_per_block,
               0, 0);
}

void Renderer::end_raygen() {
    VkCommandBuffer& commandBuffer = graphicsCommandBuffers[currentFrame];
    vkCmdEndRenderPass (commandBuffer);
}

void Renderer::start_2nd_spass() {
    VkCommandBuffer& commandBuffer = graphicsCommandBuffers[currentFrame];
    VkResult result = vkAcquireNextImageKHR (device, swapchain, UINT64_MAX, imageAvailableSemaphores[currentFrame], VK_NULL_HANDLE, &imageIndex);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR) {
        // recreate_SwapchainDependent();
        resized = true;
        // return; // can be avoided, but it is just 1 frame
    } else if ((result != VK_SUCCESS)) {
        printf (KRED "failed to acquire swap chain image!\n" KEND);
        exit (result);
    }
    vector<AoLut> ao_lut = generateLUT (8, 16.0 / 1000.0,
                        dvec2 (swapChainExtent.width, swapChainExtent.height),
                        dvec3 (camera.horizline* camera.viewSize.x / 2.0), dvec3 (camera.vertiline* camera.viewSize.y / 2.0));
    vkCmdUpdateBuffer (commandBuffer, aoLutUniform[currentFrame].buffer, 0, sizeof (AoLut)*ao_lut.size(), ao_lut.data());
    cmdPipelineBarrier (commandBuffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT,
        aoLutUniform[currentFrame]);
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
    VkClearValue
        clear_depth = {};
        clear_depth.depthStencil.depth = 1;
    vector<VkClearValue> clearColors = {
        {},
        {},
        {},
        {},
        far,
        near,
    };
    VkRenderPassBeginInfo 
        renderPassInfo = {};
        renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        renderPassInfo.renderPass = shadeRpass;
        renderPassInfo.framebuffer = altFramebuffers[imageIndex];
        renderPassInfo.renderArea.offset = {0, 0};
        renderPassInfo.renderArea.extent = {raytraceExtent.width, raytraceExtent.height};
        renderPassInfo.clearValueCount = clearColors.size();
        renderPassInfo.pClearValues = clearColors.data();
    vkCmdBeginRenderPass (commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);

    cmdSetViewport(commandBuffer, raytraceExtent);
}

void Renderer::diffuse() {
    VkCommandBuffer& commandBuffer = graphicsCommandBuffers[currentFrame];
    PLACE_TIMESTAMP();
    vkCmdBindPipeline (commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, diffusePipe.line);
    vkCmdBindDescriptorSets (commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, diffusePipe.lineLayout, 0, 1, &diffusePipe.sets[currentFrame], 0, 0);
    struct rtpc {vec4 v1, v2; mat4 lp;} pushconstant = {vec4 (camera.cameraPos, intBitsToFloat (iFrame)), vec4 (camera.cameraDir, 0), lightTransform};
    vkCmdPushConstants (commandBuffer, diffusePipe.lineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof (pushconstant), &pushconstant);
    PLACE_TIMESTAMP();
    vkCmdDraw (commandBuffer, 3, 1, 0, 0);
    PLACE_TIMESTAMP();
}

void Renderer::ambient_occlusion() {
    VkCommandBuffer& commandBuffer = graphicsCommandBuffers[currentFrame];
    vkCmdNextSubpass (commandBuffer, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline (commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, aoPipe.line);
    vkCmdBindDescriptorSets (commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, aoPipe.lineLayout, 0, 1, &aoPipe.sets[currentFrame], 0, 0);
    PLACE_TIMESTAMP();
    vkCmdDraw (commandBuffer, 3, 1, 0, 0);
    PLACE_TIMESTAMP();
}

void Renderer::glossy_raygen() {
    VkCommandBuffer& commandBuffer = graphicsCommandBuffers[currentFrame];
    vkCmdNextSubpass (commandBuffer, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline (commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, fillStencilGlossyPipe.line);
    vkCmdBindDescriptorSets (commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, fillStencilGlossyPipe.lineLayout, 0, 1, &fillStencilGlossyPipe.sets[currentFrame], 0, 0);
    PLACE_TIMESTAMP();
    vkCmdDraw (commandBuffer, 3, 1, 0, 0);
    PLACE_TIMESTAMP();
}

void Renderer::smoke_raygen() {
    VkCommandBuffer& commandBuffer = graphicsCommandBuffers[currentFrame];
    vkCmdNextSubpass (commandBuffer, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline (commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, fillStencilSmokePipe.line);
    vkCmdBindDescriptorSets (commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, fillStencilSmokePipe.lineLayout, 0, 1, &fillStencilSmokePipe.sets[currentFrame], 0, 0);
    struct rtpc {vec4 centerSize;} pushconstant = {vec4 (vec3 (11, 11, 1.5) * 16.f, 32)};
    specialRadianceUpdates.push_back (ivec4 (11, 11, 1, 0));
    vkCmdPushConstants (commandBuffer, fillStencilSmokePipe.lineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof (pushconstant), &pushconstant);
    PLACE_TIMESTAMP();
    vkCmdDraw (commandBuffer, 36, 1, 0, 0);
    PLACE_TIMESTAMP();
}

void Renderer::smoke() {
    VkCommandBuffer& commandBuffer = graphicsCommandBuffers[currentFrame];
    vkCmdNextSubpass (commandBuffer, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline (commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, smokePipe.line);
    vkCmdBindDescriptorSets (commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, smokePipe.lineLayout, 0, 1, &smokePipe.sets[currentFrame], 0, 0);
    PLACE_TIMESTAMP();
    vkCmdDraw (commandBuffer, 3, 1, 0, 0);
    PLACE_TIMESTAMP();
}

void Renderer::glossy() {
    VkCommandBuffer& commandBuffer = graphicsCommandBuffers[currentFrame];
    vkCmdNextSubpass (commandBuffer, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline (commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, glossyPipe.line);
    vkCmdBindDescriptorSets (commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, glossyPipe.lineLayout, 0, 1, &glossyPipe.sets[currentFrame], 0, 0);
    struct rtpc {vec4 v1, v2;} pushconstant = {vec4 (camera.cameraPos, 0), vec4 (camera.cameraDir, 0)};
    vkCmdPushConstants (commandBuffer, glossyPipe.lineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof (pushconstant), &pushconstant);
    PLACE_TIMESTAMP();
    vkCmdDraw (commandBuffer, 3, 1, 0, 0);
    PLACE_TIMESTAMP();
}

void Renderer::tonemap() {
    VkCommandBuffer& commandBuffer = graphicsCommandBuffers[currentFrame];
    vkCmdNextSubpass (commandBuffer, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline (commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, tonemapPipe.line);
    vkCmdBindDescriptorSets (commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, tonemapPipe.lineLayout, 0, 1, &tonemapPipe.sets[currentFrame], 0, 0);
    PLACE_TIMESTAMP();
    vkCmdDraw (commandBuffer, 3, 1, 0, 0);
    PLACE_TIMESTAMP();
}

void Renderer::end_2nd_spass() {
    VkCommandBuffer& commandBuffer = graphicsCommandBuffers[currentFrame];
    // vkCmdEndRenderPass(commandBuffer);
}

void Renderer::start_ui() {
    VkCommandBuffer& commandBuffer = graphicsCommandBuffers[currentFrame];
    PLACE_TIMESTAMP();
    vkCmdNextSubpass (commandBuffer, VK_SUBPASS_CONTENTS_INLINE);
    vkCmdBindPipeline (commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, overlayPipe.line);
    VkViewport 
        viewport = {};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (float) (swapChainExtent.width );
        viewport.height = (float) (swapChainExtent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
    vkCmdSetViewport (commandBuffer, 0, 1, &viewport);
        ui_render_interface->last_scissors.offset = {0, 0};
        ui_render_interface->last_scissors.extent = swapChainExtent;
    vkCmdSetScissor (commandBuffer, 0, 1, &ui_render_interface->last_scissors);
}

void Renderer::end_ui() {
    VkCommandBuffer& commandBuffer = graphicsCommandBuffers[currentFrame];
    vkCmdEndRenderPass (commandBuffer); //end of blur2present rpass
    PLACE_TIMESTAMP();
}

void Renderer::present() {
    VK_CHECK (vkEndCommandBuffer (computeCommandBuffers[currentFrame]));
    VK_CHECK (vkEndCommandBuffer (lightmapCommandBuffers[currentFrame]));
    VK_CHECK (vkEndCommandBuffer (graphicsCommandBuffers[currentFrame]));
    VK_CHECK (vkEndCommandBuffer (copyCommandBuffers[currentFrame]));
    vector<VkSemaphore> signalSemaphores = {renderFinishedSemaphores};
    vector<VkSemaphore> waitSemaphores = {imageAvailableSemaphores[currentFrame]};
    VkPipelineStageFlags waitStages[] = {
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
    };
    vector<VkCommandBuffer> commandBuffers = {
        copyCommandBuffers[currentFrame],
        computeCommandBuffers[currentFrame],
        lightmapCommandBuffers[currentFrame],
        graphicsCommandBuffers[currentFrame]
    };
    // vector<VkCommandBuffer> commandBuffers = {computeCommandBuffers[currentFrame], graphicsCommandBuffers[currentFrame]};
    VkSubmitInfo 
        submitInfo = {};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.waitSemaphoreCount = waitSemaphores.size();
        submitInfo.pWaitSemaphores = waitSemaphores.data();
        submitInfo.pWaitDstStageMask = waitStages;
        submitInfo.commandBufferCount = commandBuffers.size();
        submitInfo.pCommandBuffers = commandBuffers.data();
        submitInfo.signalSemaphoreCount = signalSemaphores.size();
        submitInfo.pSignalSemaphores = signalSemaphores.data();
    VkSwapchainKHR swapchains[] = {swapchain};
    VkPresentInfoKHR 
        presentInfo = {};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.waitSemaphoreCount = signalSemaphores.size();
        presentInfo.pWaitSemaphores = signalSemaphores.data();
        presentInfo.swapchainCount = 1;
        presentInfo.pSwapchains = swapchains;
        presentInfo.pImageIndices = &imageIndex;
        presentInfo.pResults = NULL;
    VK_CHECK (vkQueueSubmit (graphicsQueue, 1, &submitInfo, frameInFlightFences[currentFrame]));
    VkResult result = vkQueuePresentKHR (presentQueue, &presentInfo);
    if (result == VK_ERROR_OUT_OF_DATE_KHR || result == VK_SUBOPTIMAL_KHR || resized) {
        resized = false;
        // cout << KRED"failed to present swap chain image!\n" KEND;
        recreateSwapchainDependent();
    } else if (result != VK_SUCCESS) {
        cout << KRED"failed to present swap chain image!\n" KEND;
        exit (result);
    }
}

void Renderer::end_frame() {
    // printl(currentTimestamp)
    if (iFrame != 0) {
        //this was causing basically device wait idle
        vkGetQueryPoolResults (
            device,
            queryPoolTimestamps[previousFrame],
            0,
            currentTimestamp,
            currentTimestamp* sizeof (uint64_t),
            timestamps.data(),
            sizeof (uint64_t),
            VK_QUERY_RESULT_64_BIT | VK_QUERY_RESULT_WAIT_BIT);
    }
    double timestampPeriod = physicalDeviceProperties.limits.timestampPeriod;
    timeTakenByRadiance = float (timestamps[1] - timestamps[0]) * timestampPeriod / 1000000.0f;
    for (int i = 0; i < timestampCount; i++) {
        ftimestamps[i] = double (timestamps[i]) * timestampPeriod / 1000000.0;
    }
    if (iFrame > 5) {
        for (int i = 0; i < timestampCount; i++) {
            average_ftimestamps[i] = glm::mix (average_ftimestamps[i], ftimestamps[i], 0.1);
        }
    } else {
        for (int i = 0; i < timestampCount; i++) {
            average_ftimestamps[i] = ftimestamps[i];
        }
    }
    previousFrame = currentFrame;
    currentFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    nextFrame = (currentFrame + 1) % MAX_FRAMES_IN_FLIGHT;
    iFrame++;
    processDeletionQueue();
    // #ifdef MEASURE_PERFOMANCE
    // #endif
    // printl(timeTakenByRadiance)
}

void Renderer::cmdPipelineBarrier (VkCommandBuffer commandBuffer,
                VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask,
                VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask,
                Buffer buffer) {
    VkBufferMemoryBarrier 
        barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_BUFFER_MEMORY_BARRIER;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.srcAccessMask = srcAccessMask;
        barrier.dstAccessMask = dstAccessMask;
        barrier.size = VK_WHOLE_SIZE;
        barrier.buffer = buffer.buffer; // buffer buffer buffer
    vkCmdPipelineBarrier (
        commandBuffer,
        srcStageMask, dstStageMask,
        0,
        0, NULL,
        1, &barrier,
        0, NULL
    );
}

void Renderer::cmdPipelineBarrier (VkCommandBuffer commandBuffer,
                VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask,
                VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask,
                Image image) {
    VkImageMemoryBarrier 
        barrier = {};
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
    vkCmdPipelineBarrier (
        commandBuffer,
        srcStageMask, dstStageMask,
        0,
        0, NULL,
        0, NULL,
        1, &barrier
    );
}

void Renderer::cmdPipelineBarrier (VkCommandBuffer commandBuffer, VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask) {
    vkCmdPipelineBarrier (
        commandBuffer,
        srcStageMask, dstStageMask,
        0,
        0, NULL,
        0, NULL,
        0, NULL
    );
}

#define get_layout_helper_define(layout) case layout: return #layout;

static const char* get_layout_name (VkImageLayout layout) {
    switch (layout) {
            get_layout_helper_define (VK_IMAGE_LAYOUT_UNDEFINED)
            get_layout_helper_define (VK_IMAGE_LAYOUT_GENERAL)
            get_layout_helper_define (VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL)
            get_layout_helper_define (VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
            get_layout_helper_define (VK_IMAGE_LAYOUT_DEPTH_STENCIL_READ_ONLY_OPTIMAL)
            get_layout_helper_define (VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
            get_layout_helper_define (VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL)
            get_layout_helper_define (VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
        default: return "invalid layout";
    }
}

void Renderer::cmdTransLayoutBarrier (VkCommandBuffer commandBuffer, VkImageLayout targetLayout,
                    VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask,
                    Image* image) {
    VkImageMemoryBarrier 
        barrier = {};
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
    vkCmdPipelineBarrier (
        commandBuffer,
        srcStageMask, dstStageMask,
        0,
        0, NULL,
        0, NULL,
        1, &barrier
    );
    (*image).layout = targetLayout;
}

void Renderer::cmdTransLayoutBarrier (VkCommandBuffer commandBuffer, VkImageLayout srcLayout, VkImageLayout targetLayout,
                    VkPipelineStageFlags srcStageMask, VkPipelineStageFlags dstStageMask, VkAccessFlags srcAccessMask, VkAccessFlags dstAccessMask,
                    Image* image) {
    VkImageMemoryBarrier 
        barrier = {};
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
    vkCmdPipelineBarrier (
        commandBuffer,
        srcStageMask, dstStageMask,
        0,
        0, NULL,
        0, NULL,
        1, &barrier
    );
    (*image).layout = targetLayout;
}


void Renderer::createSamplers() {
    VkSamplerCreateInfo 
        samplerInfo = {};
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
    VK_CHECK (vkCreateSampler (device, &samplerInfo, NULL, &nearestSampler));
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
    VK_CHECK (vkCreateSampler (device, &samplerInfo, NULL, &linearSampler));
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
    VK_CHECK (vkCreateSampler (device, &samplerInfo, NULL, &linearSampler_tiled));
        samplerInfo.magFilter = VK_FILTER_NEAREST;
        samplerInfo.minFilter = VK_FILTER_NEAREST;
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    VK_CHECK (vkCreateSampler (device, &samplerInfo, NULL, &overlaySampler));
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;
        samplerInfo.magFilter = VK_FILTER_LINEAR;
        samplerInfo.minFilter = VK_FILTER_LINEAR;
    VK_CHECK (vkCreateSampler (device, &samplerInfo, NULL, &unnormLinear));
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_REPEAT;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;
        samplerInfo.magFilter = VK_FILTER_NEAREST;
        samplerInfo.minFilter = VK_FILTER_NEAREST;
    VK_CHECK (vkCreateSampler (device, &samplerInfo, NULL, &unnormNearest));
        samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_MIRRORED_REPEAT;
        samplerInfo.unnormalizedCoordinates = VK_FALSE;
        samplerInfo.magFilter = VK_FILTER_NEAREST;
        samplerInfo.minFilter = VK_FILTER_NEAREST;
        samplerInfo.compareEnable = VK_TRUE;
        samplerInfo.borderColor = VK_BORDER_COLOR_FLOAT_OPAQUE_WHITE;
        samplerInfo.compareOp = VK_COMPARE_OP_LESS;
    VK_CHECK (vkCreateSampler (device, &samplerInfo, NULL, &shadowSampler));
}

void Renderer::copyBufferSingleTime (VkBuffer srcBuffer, VkBuffer dstBuffer, VkDeviceSize size) {
    VkCommandBuffer commandBuffer = beginSingleTimeCommands();
    VkBufferCopy copyRegion{};
    copyRegion.size = size;
    vkCmdCopyBuffer (commandBuffer, srcBuffer, dstBuffer, 1, &copyRegion);
    endSingleTimeCommands (commandBuffer);
}

void Renderer::copyBufferSingleTime (VkBuffer srcBuffer, Image* image, uvec3 size) {
    VkCommandBuffer commandBuffer = beginSingleTimeCommands();
    VkImageLayout initial = (*image).layout;
    assert (initial != VK_IMAGE_LAYOUT_UNDEFINED);
    cmdTransLayoutBarrier (commandBuffer, VK_IMAGE_LAYOUT_GENERAL, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT, image);
    VkBufferImageCopy 
        copyRegion = {};
        copyRegion.imageExtent.width = size.x;
        copyRegion.imageExtent.height = size.y;
        copyRegion.imageExtent.depth = size.z;
        copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copyRegion.imageSubresource.layerCount = 1;
    vkCmdCopyBufferToImage (commandBuffer, srcBuffer, (*image).image, VK_IMAGE_LAYOUT_GENERAL, 1, &copyRegion);
    cmdTransLayoutBarrier (commandBuffer, initial, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT, image);
    endSingleTimeCommands (commandBuffer);
}

VkCommandBuffer Renderer::beginSingleTimeCommands() {
    VkCommandBufferAllocateInfo 
        allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
        allocInfo.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
        allocInfo.commandPool = commandPool;
        allocInfo.commandBufferCount = 1;
    VkCommandBuffer commandBuffer;
    vkAllocateCommandBuffers (device, &allocInfo, &commandBuffer);
    VkCommandBufferBeginInfo 
        beginInfo{};
        beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    vkBeginCommandBuffer (commandBuffer, &beginInfo);
    return commandBuffer;
}

void Renderer::endSingleTimeCommands (VkCommandBuffer commandBuffer) {
    vkEndCommandBuffer (commandBuffer);
    VkSubmitInfo 
        submitInfo{};
        submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submitInfo.commandBufferCount = 1;
        submitInfo.pCommandBuffers = &commandBuffer;
    //TODO: change to barrier
    vkQueueSubmit (graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    vkQueueWaitIdle (graphicsQueue);
    vkFreeCommandBuffers (device, commandPool, 1, &commandBuffer);
}

void Renderer::transitionImageLayoutSingletime (Image* image, VkImageLayout newLayout, int mipmaps) {
    VkCommandBuffer commandBuffer = beginSingleTimeCommands();
    VkImageMemoryBarrier 
        barrier{};
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
        barrier.srcAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
    vkCmdPipelineBarrier (
        commandBuffer,
        VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        0,
        0, NULL,
        0, NULL,
        1, &barrier
    );
    (*image).layout = newLayout;
    endSingleTimeCommands (commandBuffer);
}

void Renderer::generateMipmaps (VkCommandBuffer commandBuffer, VkImage image, int32_t texWidth, int32_t texHeight, uint32_t mipLevels, VkImageAspectFlags aspect) {
    // VkCommandBuffer commandBuffer = begin_Single_Time_Commands();
    VkImageMemoryBarrier 
        barrier{};
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
        vkCmdPipelineBarrier (commandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT, 0,
            0, nullptr,
            0, nullptr,
            1, &barrier);
        VkImageBlit 
            blit{};
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
        vkCmdBlitImage (commandBuffer,
            image, VK_IMAGE_LAYOUT_GENERAL,
            image, VK_IMAGE_LAYOUT_GENERAL,
            1, &blit,
            (aspect == VK_IMAGE_ASPECT_COLOR_BIT) ? VK_FILTER_LINEAR : VK_FILTER_NEAREST);
                barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
                barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
                barrier.srcAccessMask = VK_ACCESS_TRANSFER_READ_BIT;
                barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier (commandBuffer,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
            0, nullptr,
            0, nullptr,
            1, &barrier);
        if (mipWidth > 1) { mipWidth /= 2; }
        if (mipHeight > 1) { mipHeight /= 2; }
    }
        barrier.subresourceRange.baseMipLevel = mipLevels - 1;
        barrier.oldLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_GENERAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
    vkCmdPipelineBarrier (commandBuffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT, 0,
        0, nullptr,
        0, nullptr,
        1, &barrier);
    // end_Single_Time_Commands(commandBuffer);
}

void Renderer::copyWholeImage (VkExtent3D extent, VkCommandBuffer cmdbuf, VkImage src, VkImage dst) {
    VkImageCopy 
        copy_op = {};
        copy_op.srcSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copy_op.dstSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copy_op.srcSubresource.layerCount = 1;
        copy_op.dstSubresource.layerCount = 1;
        copy_op.srcSubresource.baseArrayLayer = 0;
        copy_op.dstSubresource.baseArrayLayer = 0;
        copy_op.srcSubresource.mipLevel = 0;
        copy_op.dstSubresource.mipLevel = 0;
        copy_op.srcOffset = {0, 0, 0};
        copy_op.dstOffset = {0, 0, 0};
        copy_op.extent = extent;
    vkCmdCopyImage (cmdbuf,
                    src,
                    VK_IMAGE_LAYOUT_GENERAL, //TODO:
                    dst,
                    VK_IMAGE_LAYOUT_GENERAL, //TODO:
                    1,
                    &copy_op
                   );
    VkImageMemoryBarrier 
        barrier{};
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
    vkCmdPipelineBarrier (
        cmdbuf,
        VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        0,
        0, NULL,
        0, NULL,
        1, &barrier
    );
}

tuple<int, int> get_block_xy (int N) {
    int x = N % BLOCK_PALETTE_SIZE_X;
    int y = N / BLOCK_PALETTE_SIZE_X;
    assert (y <= BLOCK_PALETTE_SIZE_Y);
    return tuple (x, y);
}

VkFormat Renderer::findSupportedFormat (vector<VkFormat> candidates,
                        VkImageType type,
                        VkImageTiling tiling,
                        VkImageUsageFlags usage) {
    for (VkFormat format : candidates) {
        VkImageFormatProperties imageFormatProperties;
        VkResult result = vkGetPhysicalDeviceImageFormatProperties (
            physicalDevice,
            format,
            type,
            tiling,
            usage,
            0, // No flags
      &     imageFormatProperties);
        if (result == VK_SUCCESS) {
            return format;
        }
    }
    crash (Failed to find supported format!);
}

void Renderer::cmdSetViewport(VkCommandBuffer commandBuffer, int width, int height){
    VkViewport 
        viewport = {};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (float) (width );
        viewport.height = (float) (height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
    vkCmdSetViewport (commandBuffer, 0, 1, &viewport);
    VkRect2D 
        scissor = {};
        scissor.offset = {0, 0};
        scissor.extent.width = width;
        scissor.extent.height = height;
    vkCmdSetScissor (commandBuffer, 0, 1, &scissor);
}
void Renderer::cmdSetViewport(VkCommandBuffer commandBuffer, VkExtent2D extent){
    cmdSetViewport(commandBuffer, extent.width, extent.height);
}