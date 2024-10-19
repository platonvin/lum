#ifdef __EMSCRIPTEN__
#error Lum does not compile for web
#endif

#include "render.hpp"
#define VMA_IMPLEMENTATION
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#include "defines/macros.hpp"

#include <glm/gtx/quaternion.hpp>
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



void LumRenderer::init (Settings settings) {
    world_size = ivec3(48, 48, 16);
    static_block_palette_size = 15;
    maxParticleCount = 8128;

    render.init(settings);

    origin_world.allocate (world_size);
    current_world.allocate (world_size);
    lightmapExtent = {1024, 1024};
    // lightmapExtent = {768,768};
    //to fix weird mesh culling problems for identity
    // camera.updateCamera();
    
    ui_render_interface = new(MyRenderInterface);
    ui_render_interface->render = this;
    

    DEPTH_FORMAT = render.findSupportedFormat ({DEPTH_FORMAT_PREFERED, DEPTH_FORMAT_SPARE}, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL,
                                        VK_IMAGE_USAGE_TRANSFER_SRC_BIT | 
                                        VK_IMAGE_USAGE_TRANSFER_DST_BIT | 
                                        VK_IMAGE_USAGE_SAMPLED_BIT | 
                                        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | 
                                        VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT);
    createSamplers();
    createImages();
    createSwapchainDependent();

    // render.createCommandBuffers(&computeCommandBuffers, render.settings.fif);
    // render.createCommandBuffers(&graphicsCommandBuffers, render.settings.fif);
    // render.createCommandBuffers(&copyCommandBuffers, render.settings.fif);
    // render.createCommandBuffers(&lightmapCommandBuffers, render.settings.fif);

    render.mainCommandBuffers = &computeCommandBuffers;
    render.extraCommandBuffers = &copyCommandBuffers;
    render.createSwapchainDependent = [this]() -> VkResult {return LumRenderer::createSwapchainDependent();};
    render.cleanupSwapchainDependent = [this]() -> VkResult {return LumRenderer::cleanupSwapchainDependent();};

println
    gen_perlin_2d();
println
    gen_perlin_3d();
println
}

void LumRenderer::createImages() {
    render.createBufferStorages (&stagingWorld,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        world_size.x* world_size.y* world_size.z* sizeof (BlockID_t), true);
    render.createImageStorages (&world,
        VK_IMAGE_TYPE_3D,
        VK_FORMAT_R16_SINT,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT,
        extent3d(world_size)); //TODO: dynamic
    render.createImageStorages (&lightmap,
        VK_IMAGE_TYPE_2D,
        LIGHTMAPS_FORMAT,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        VK_IMAGE_ASPECT_DEPTH_BIT,
    {lightmapExtent.width, lightmapExtent.height, 1});
println
    render.createImageStorages (&radianceCache,
        VK_IMAGE_TYPE_3D,
        RADIANCE_FORMAT,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT,
    extent3d(world_size)); //TODO: dynamic
println
    render.createImageStorages (&originBlockPalette,
        VK_IMAGE_TYPE_3D,
        VK_FORMAT_R8_UINT,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT,
        // {16*BLOCK_PALETTE_SIZE_X, 16*BLOCK_PALETTE_SIZE_Y, 32}); //TODO: dynamic
    {16 * BLOCK_PALETTE_SIZE_X, 16 * BLOCK_PALETTE_SIZE_Y, 16}); //TODO: dynamic
    render.createImageStorages (&materialPalette,
        VK_IMAGE_TYPE_2D,
        VK_FORMAT_R32_SFLOAT, //try R32G32
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT,
    {6, 256, 1}); //TODO: dynamic, text formats, pack Material into smth other than 6 floats :)
    render.createImageStorages (&grassState,
        VK_IMAGE_TYPE_2D,
        VK_FORMAT_R16G16_SFLOAT,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        0,
        VK_IMAGE_ASPECT_COLOR_BIT,
    {world_size.x * 2, world_size.y * 2, 1}); //for quality
    render.createImageStorages (&waterState,
        VK_IMAGE_TYPE_2D,
        VK_FORMAT_R16G16B16A16_SFLOAT,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        0,
        VK_IMAGE_ASPECT_COLOR_BIT,
    {world_size.x * 2, world_size.y * 2, 1}); //for quality
    render.createImageStorages (&perlinNoise2d,
        VK_IMAGE_TYPE_2D,
        VK_FORMAT_R16G16_SNORM,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        0,
        VK_IMAGE_ASPECT_COLOR_BIT,
    {world_size.x, world_size.y, 1}); //does not matter than much
    render.createImageStorages (&perlinNoise3d,
        VK_IMAGE_TYPE_3D,
        VK_FORMAT_R16G16B16A16_UNORM,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        0,
        VK_IMAGE_ASPECT_COLOR_BIT,
    {32, 32, 32}); //does not matter than much
    render.createBufferStorages (&gpuParticles,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        maxParticleCount* sizeof (Particle), true);
    render.createBufferStorages (&uniform,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        220, false); //no way i write it with sizeof's
    render.createBufferStorages (&lightUniform,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        sizeof (mat4), false);
    render.createBufferStorages (&aoLutUniform,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        sizeof (AoLut) * 8, false); //TODO DYNAMIC AO SAMPLE COUNT
    render.createBufferStorages (&gpuRadianceUpdates,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        sizeof (i8vec4)*world_size.x* world_size.y* world_size.z, false); //TODO test extra mem
    render.createBufferStorages (&stagingRadianceUpdates,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        sizeof (ivec4)*world_size.x* world_size.y* world_size.z, true); //TODO test extra mem
        
    render.mapBufferStorages (&gpuParticles);
    render.mapBufferStorages (&stagingWorld);
    render.mapBufferStorages (&stagingRadianceUpdates);
println
}

void LumRenderer::setupDescriptors() {
    render.createDescriptorSetLayout ({
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT},//in texture (ui)
    }, &overlayPipe.setLayout,
    VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR);
    render.deferDescriptorSetup (&lightmapBlocksPipe.setLayout, &lightmapBlocksPipe.sets, {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, RD_CURRENT, (&lightUniform), {/*empty*/}, NO_SAMPLER, NO_LAYOUT},
    }, VK_SHADER_STAGE_VERTEX_BIT);
    render.deferDescriptorSetup (&lightmapModelsPipe.setLayout, &lightmapModelsPipe.sets, {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, RD_CURRENT, (&lightUniform), {/*empty*/}, NO_SAMPLER, NO_LAYOUT},
    }, VK_SHADER_STAGE_VERTEX_BIT);
println
    render.deferDescriptorSetup (&radiancePipe.setLayout, &radiancePipe.sets, {
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, RD_FIRST, {/*empty*/}, &world, unnormNearest, VK_IMAGE_LAYOUT_GENERAL},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, RD_CURRENT, {/*empty*/}, (&originBlockPalette), unnormNearest, VK_IMAGE_LAYOUT_GENERAL},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, RD_CURRENT, {/*empty*/}, (&materialPalette), nearestSampler, VK_IMAGE_LAYOUT_GENERAL},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, RD_FIRST, {/*empty*/}, &radianceCache, unnormLinear, VK_IMAGE_LAYOUT_GENERAL},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, RD_FIRST, {/*empty*/}, &radianceCache, NO_SAMPLER, VK_IMAGE_LAYOUT_GENERAL},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, RD_FIRST, &gpuRadianceUpdates, {}, NO_SAMPLER, NO_LAYOUT},
    }, VK_SHADER_STAGE_COMPUTE_BIT);
println
    render.deferDescriptorSetup (&diffusePipe.setLayout, &diffusePipe.sets, {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, RD_CURRENT, (&uniform), {/*empty*/}, NO_SAMPLER, NO_LAYOUT, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT},
        {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, RD_FIRST, {/*empty*/}, &highresMatNorm, NO_SAMPLER, VK_IMAGE_LAYOUT_GENERAL},
        {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, RD_FIRST, {/*empty*/}, &highresDepthStencil, NO_SAMPLER, VK_IMAGE_LAYOUT_GENERAL},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, RD_FIRST, {/*empty*/}, &materialPalette, nearestSampler, VK_IMAGE_LAYOUT_GENERAL},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, RD_FIRST, {/*empty*/}, &radianceCache, unnormLinear, VK_IMAGE_LAYOUT_GENERAL},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, RD_FIRST, {/*empty*/}, &lightmap, shadowSampler, VK_IMAGE_LAYOUT_GENERAL},
        // {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, RD_FIRST, {/*empty*/}, {lightmap}, linearSampler, VK_IMAGE_LAYOUT_GENERAL},
    }, VK_SHADER_STAGE_FRAGMENT_BIT);
println
    render.deferDescriptorSetup (&aoPipe.setLayout, &aoPipe.sets, {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, RD_CURRENT, (&uniform), {/*empty*/}, NO_SAMPLER, NO_LAYOUT},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, RD_CURRENT, (&aoLutUniform), {/*empty*/}, NO_SAMPLER, NO_LAYOUT},
        {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, RD_FIRST, {/*empty*/}, &highresMatNorm, NO_SAMPLER, VK_IMAGE_LAYOUT_GENERAL},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, RD_FIRST, {/*empty*/}, &highresDepthStencil, nearestSampler, VK_IMAGE_LAYOUT_GENERAL},
    }, VK_SHADER_STAGE_FRAGMENT_BIT);
println
    render.deferDescriptorSetup (&tonemapPipe.setLayout, &tonemapPipe.sets, {
        {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, RD_FIRST, {/*empty*/}, &highresFrame, NO_SAMPLER, VK_IMAGE_LAYOUT_GENERAL},
    }, VK_SHADER_STAGE_FRAGMENT_BIT);
println
    render.deferDescriptorSetup (&fillStencilGlossyPipe.setLayout, &fillStencilGlossyPipe.sets, {
        {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, RD_FIRST, {/*empty*/}, &highresMatNorm, NO_SAMPLER, VK_IMAGE_LAYOUT_GENERAL},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, RD_CURRENT, {/*empty*/}, (&materialPalette), nearestSampler, VK_IMAGE_LAYOUT_GENERAL},
    }, VK_SHADER_STAGE_FRAGMENT_BIT);
println
    render.deferDescriptorSetup (&fillStencilSmokePipe.setLayout, &fillStencilSmokePipe.sets, {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, RD_CURRENT, (&uniform), {/*empty*/}, NO_SAMPLER, NO_LAYOUT},
    }, VK_SHADER_STAGE_VERTEX_BIT);
println
    render.deferDescriptorSetup (&glossyPipe.setLayout, &glossyPipe.sets, {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, RD_CURRENT, (&uniform), {/*empty*/}, NO_SAMPLER, NO_LAYOUT},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, RD_FIRST, {/*empty*/}, &highresMatNorm, nearestSampler, VK_IMAGE_LAYOUT_GENERAL},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, RD_FIRST, {/*empty*/}, &highresDepthStencil, nearestSampler, VK_IMAGE_LAYOUT_GENERAL},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, RD_FIRST, {/*empty*/}, &world, unnormNearest, VK_IMAGE_LAYOUT_GENERAL},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, RD_CURRENT, {/*empty*/}, (&originBlockPalette), unnormNearest, VK_IMAGE_LAYOUT_GENERAL},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, RD_CURRENT, {/*empty*/}, (&materialPalette), nearestSampler, VK_IMAGE_LAYOUT_GENERAL},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, RD_FIRST, {/*empty*/}, &radianceCache, unnormLinear, VK_IMAGE_LAYOUT_GENERAL},
    }, VK_SHADER_STAGE_FRAGMENT_BIT);
println
    render.deferDescriptorSetup (&smokePipe.setLayout, &smokePipe.sets, {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, RD_CURRENT, (&uniform), {/*empty*/}, NO_SAMPLER, NO_LAYOUT},
        {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, RD_FIRST, {/*empty*/}, &farDepth, NO_SAMPLER, VK_IMAGE_LAYOUT_GENERAL},
        {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, RD_FIRST, {/*empty*/}, &nearDepth, NO_SAMPLER, VK_IMAGE_LAYOUT_GENERAL},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, RD_FIRST, {/*empty*/}, &radianceCache, NO_SAMPLER, VK_IMAGE_LAYOUT_GENERAL},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, RD_FIRST, {/*empty*/}, &perlinNoise3d, linearSampler_tiled, VK_IMAGE_LAYOUT_GENERAL},
    }, VK_SHADER_STAGE_FRAGMENT_BIT);
println
    // create_DescriptorSetLayout({
    // VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, // per-quad attributes
    // },
    // VK_SHADER_STAGE_VERTEX_BIT, &blocksPushLayout,
    // VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR);
    render.deferDescriptorSetup (&raygenBlocksPipe.setLayout, &raygenBlocksPipe.sets, {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, RD_CURRENT, (&uniform), {/*empty*/}, NO_SAMPLER, NO_LAYOUT},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, RD_CURRENT, {}, &originBlockPalette, unnormNearest, VK_IMAGE_LAYOUT_GENERAL},
    }, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    // setup_RayGen_Particles_Descriptors();
    render.createDescriptorSetLayout ({
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT} // model voxels
    }, &raygenModelsPushLayout,
    VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR);
    // 0);
    render.deferDescriptorSetup (&raygenModelsPipe.setLayout, &raygenModelsPipe.sets, {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, RD_CURRENT, (&uniform), {/*empty*/}, NO_SAMPLER, NO_LAYOUT},
    }, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
println
    render.deferDescriptorSetup (&raygenParticlesPipe.setLayout, &raygenParticlesPipe.sets, {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, RD_CURRENT, (&uniform), {/*empty*/}, NO_SAMPLER, NO_LAYOUT},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, RD_FIRST, {/*empty*/}, &world, NO_SAMPLER, VK_IMAGE_LAYOUT_GENERAL},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, RD_CURRENT, {/*empty*/}, (&originBlockPalette), NO_SAMPLER, VK_IMAGE_LAYOUT_GENERAL},
    }, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT);
    render.deferDescriptorSetup (&updateGrassPipe.setLayout, &updateGrassPipe.sets, {
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, RD_FIRST, {}, &grassState, NO_SAMPLER, VK_IMAGE_LAYOUT_GENERAL},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, RD_FIRST, {}, &perlinNoise2d, linearSampler_tiled, VK_IMAGE_LAYOUT_GENERAL},
    }, VK_SHADER_STAGE_COMPUTE_BIT);
    render.deferDescriptorSetup (&updateWaterPipe.setLayout, &updateWaterPipe.sets, {
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, RD_FIRST, {}, &waterState, NO_SAMPLER, VK_IMAGE_LAYOUT_GENERAL},
    }, VK_SHADER_STAGE_COMPUTE_BIT);
    render.deferDescriptorSetup (&raygenGrassPipe.setLayout, &raygenGrassPipe.sets, {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, RD_CURRENT, (&uniform), {/*empty*/}, NO_SAMPLER, NO_LAYOUT},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, RD_FIRST, {}, &grassState, linearSampler, VK_IMAGE_LAYOUT_GENERAL},
    }, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT);
    render.deferDescriptorSetup (&raygenWaterPipe.setLayout, &raygenWaterPipe.sets, {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, RD_CURRENT, (&uniform), {/*empty*/}, NO_SAMPLER, NO_LAYOUT},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, RD_FIRST, {}, &waterState, linearSampler_tiled, VK_IMAGE_LAYOUT_GENERAL},
    }, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT);
    render.deferDescriptorSetup (&genPerlin2dPipe.setLayout, &genPerlin2dPipe.sets, {
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, RD_FIRST, {}, &perlinNoise2d, NO_SAMPLER, VK_IMAGE_LAYOUT_GENERAL},
    }, VK_SHADER_STAGE_COMPUTE_BIT);
    render.deferDescriptorSetup (&genPerlin3dPipe.setLayout, &genPerlin3dPipe.sets, {
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, RD_FIRST, {}, &perlinNoise3d, NO_SAMPLER, VK_IMAGE_LAYOUT_GENERAL},
    }, VK_SHADER_STAGE_COMPUTE_BIT);
    render.createDescriptorSetLayout ({
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,VK_SHADER_STAGE_COMPUTE_BIT} // model voxels
    }, &mapPushLayout,
    VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR);
    // 0);
    render.deferDescriptorSetup (&mapPipe.setLayout, &mapPipe.sets, {
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, RD_FIRST, {}, &world, NO_SAMPLER, VK_IMAGE_LAYOUT_GENERAL},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, RD_CURRENT, {}, &originBlockPalette, NO_SAMPLER, VK_IMAGE_LAYOUT_GENERAL},
    }, VK_SHADER_STAGE_COMPUTE_BIT);
    // render.deferDescriptorsetup(&dfxPipe.setLayout, &dfxPipe.sets, {
    // {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, RD_FIRST, {}, {originBlockPalette}, NO_SAMPLER, VK_IMAGE_LAYOUT_GENERAL},
    // {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, RD_FIRST, {}, {distancePalette   }, NO_SAMPLER, VK_IMAGE_LAYOUT_GENERAL},
    // }, VK_SHADER_STAGE_COMPUTE_BIT);
    // render.deferDescriptorsetup(&dfyPipe.setLayout, &dfyPipe.sets, {
    // {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, RD_FIRST, {}, {originBlockPalette}, NO_SAMPLER, VK_IMAGE_LAYOUT_GENERAL},
    // {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, RD_FIRST, {}, {distancePalette   }, NO_SAMPLER, VK_IMAGE_LAYOUT_GENERAL},
    // }, VK_SHADER_STAGE_COMPUTE_BIT);
    // render.deferDescriptorsetup(&dfzPipe.setLayout, &dfzPipe.sets, {
    // {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, RD_FIRST, {}, {originBlockPalette}, NO_SAMPLER, VK_IMAGE_LAYOUT_GENERAL},
    // {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, RD_FIRST, {}, {distancePalette   }, NO_SAMPLER, VK_IMAGE_LAYOUT_GENERAL},
    // }, VK_SHADER_STAGE_COMPUTE_BIT);
    // render.deferDescriptorsetup(&bitmaskPipe.setLayout, &bitmaskPipe.sets, {
    // {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, RD_FIRST, {}, {originBlockPalette}, NO_SAMPLER, VK_IMAGE_LAYOUT_GENERAL},
    // {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, RD_FIRST, {}, {bitPalette}, NO_SAMPLER, VK_IMAGE_LAYOUT_GENERAL},
    // }, VK_SHADER_STAGE_COMPUTE_BIT);
    render.flushDescriptorSetup();
}

void LumRenderer::createPipilines() {
    //that is why NOT abstracting vulkan is also an option
    //if you cannot guess what things mean by just looking at them maybe read old (0.0.3) release src
println
    lightmapBlocksPipe.subpassId = 0;
    render.createRasterPipeline (&lightmapBlocksPipe, 0, {
        {"shaders/compiled/lightmapBlocks.vert.spv", VK_SHADER_STAGE_VERTEX_BIT},
        //doesnt need fragment
    }, {
        {VK_FORMAT_R8G8B8_UINT, offsetof (PackedVoxelCircuit, pos)},
    },
    sizeof (PackedVoxelCircuit), VK_VERTEX_INPUT_RATE_VERTEX, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    lightmapExtent, {NO_BLEND}, (sizeof (i16vec4)), DEPTH_TEST_READ_BIT|DEPTH_TEST_WRITE_BIT, VK_COMPARE_OP_LESS, VK_CULL_MODE_NONE, NO_DISCARD, NO_STENCIL);
    lightmapModelsPipe.subpassId = 0;
    render.createRasterPipeline (&lightmapModelsPipe, 0, {
        {"shaders/compiled/lightmapModels.vert.spv", VK_SHADER_STAGE_VERTEX_BIT},
        //doesnt need fragment
    }, {
        {VK_FORMAT_R8G8B8_UINT, offsetof (PackedVoxelCircuit, pos)},
    },
    sizeof (PackedVoxelCircuit), VK_VERTEX_INPUT_RATE_VERTEX, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    lightmapExtent, {NO_BLEND}, (sizeof (quat) + sizeof (vec4)), DEPTH_TEST_READ_BIT|DEPTH_TEST_WRITE_BIT, VK_COMPARE_OP_LESS, VK_CULL_MODE_NONE, NO_DISCARD, NO_STENCIL);
    raygenBlocksPipe.subpassId = 0;
    render.createRasterPipeline (&raygenBlocksPipe, 0, {
        {"shaders/compiled/rayGenBlocks.vert.spv", VK_SHADER_STAGE_VERTEX_BIT},
        {"shaders/compiled/rayGenBlocks.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT},
    }, {
        {VK_FORMAT_R8G8B8_UINT, offsetof (PackedVoxelCircuit, pos)},
        // {VK_FORMAT_R8G8B8_SINT, offsetof(VoxelVertex, norm)},
        // {VK_FORMAT_R8_UINT, offsetof(PackedVoxelVertex, matID)},
    },
    sizeof (PackedVoxelCircuit), VK_VERTEX_INPUT_RATE_VERTEX, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    render.swapChainExtent, {NO_BLEND}, (12), DEPTH_TEST_READ_BIT|DEPTH_TEST_WRITE_BIT, VK_COMPARE_OP_LESS, VK_CULL_MODE_NONE, NO_DISCARD, NO_STENCIL);
println
    raygenModelsPipe.subpassId = 1;
    render.createRasterPipeline (&raygenModelsPipe, raygenModelsPushLayout, {
        {"shaders/compiled/rayGenModels.vert.spv", VK_SHADER_STAGE_VERTEX_BIT},
        {"shaders/compiled/rayGenModels.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT},
    }, {
        {VK_FORMAT_R8G8B8_UINT, offsetof (PackedVoxelCircuit, pos)},
    },
    sizeof (PackedVoxelCircuit), VK_VERTEX_INPUT_RATE_VERTEX, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    render.swapChainExtent, {NO_BLEND}, (sizeof (vec4) * 3), DEPTH_TEST_READ_BIT|DEPTH_TEST_WRITE_BIT, VK_COMPARE_OP_LESS, VK_CULL_MODE_NONE, NO_DISCARD, NO_STENCIL);
println
    raygenParticlesPipe.subpassId = 2;
    render.createRasterPipeline (&raygenParticlesPipe, 0, {
        {"shaders/compiled/rayGenParticles.vert.spv", VK_SHADER_STAGE_VERTEX_BIT},
        {"shaders/compiled/rayGenParticles.geom.spv", VK_SHADER_STAGE_GEOMETRY_BIT},
        {"shaders/compiled/rayGenParticles.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT},
    }, {
        {VK_FORMAT_R32G32B32_SFLOAT, offsetof (Particle, pos)},
        {VK_FORMAT_R32G32B32_SFLOAT, offsetof (Particle, vel)},
        {VK_FORMAT_R32_SFLOAT, offsetof (Particle, lifeTime)},
        {VK_FORMAT_R8_UINT, offsetof (Particle, matID)},
    },
    sizeof (Particle), VK_VERTEX_INPUT_RATE_VERTEX, VK_PRIMITIVE_TOPOLOGY_POINT_LIST,
    render.swapChainExtent, {NO_BLEND}, 0, DEPTH_TEST_READ_BIT|DEPTH_TEST_WRITE_BIT, VK_COMPARE_OP_LESS, VK_CULL_MODE_NONE, NO_DISCARD, NO_STENCIL);
println
    raygenGrassPipe.subpassId = 3;
    render.createRasterPipeline (&raygenGrassPipe, 0, {
        {"shaders/compiled/grass.vert.spv", VK_SHADER_STAGE_VERTEX_BIT},
        {"shaders/compiled/grass.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT},
    }, {/*empty*/},
    0, VK_VERTEX_INPUT_RATE_VERTEX, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
    render.swapChainExtent, {NO_BLEND}, sizeof (vec4) + sizeof (int) * 2 + sizeof (int) * 2, DEPTH_TEST_READ_BIT|DEPTH_TEST_WRITE_BIT, VK_COMPARE_OP_LESS, VK_CULL_MODE_NONE, NO_DISCARD, NO_STENCIL);
println
    raygenWaterPipe.subpassId = 4;
    render.createRasterPipeline (&raygenWaterPipe, 0, {
        {"shaders/compiled/water.vert.spv", VK_SHADER_STAGE_VERTEX_BIT},
        {"shaders/compiled/water.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT},
    }, {/*empty*/},
    0, VK_VERTEX_INPUT_RATE_VERTEX, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
    render.swapChainExtent, {NO_BLEND}, sizeof (vec4) + sizeof (int) * 2, DEPTH_TEST_READ_BIT|DEPTH_TEST_WRITE_BIT, VK_COMPARE_OP_LESS, VK_CULL_MODE_NONE, NO_DISCARD, NO_STENCIL);
println
    diffusePipe.subpassId = 0;
    render.createRasterPipeline (&diffusePipe, 0, {
        {"shaders/compiled/fullscreenTriag.vert.spv", VK_SHADER_STAGE_VERTEX_BIT},
        {"shaders/compiled/diffuse.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT},
    }, {/*fullscreen pass*/},
    0, VK_VERTEX_INPUT_RATE_VERTEX, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    render.swapChainExtent, {NO_BLEND}, sizeof (ivec4) + sizeof (vec4) * 4 + sizeof (mat4), DEPTH_TEST_NONE_BIT, VK_COMPARE_OP_LESS, VK_CULL_MODE_NONE, NO_DISCARD, NO_STENCIL);
    aoPipe.subpassId = 1;
    render.createRasterPipeline (&aoPipe, 0, {
        {"shaders/compiled/fullscreenTriag.vert.spv", VK_SHADER_STAGE_VERTEX_BIT},
        {"shaders/compiled/hbao.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT},
    }, {/*fullscreen pass*/},
    0, VK_VERTEX_INPUT_RATE_VERTEX, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    render.swapChainExtent, {BLEND_MIX}, 0, DEPTH_TEST_NONE_BIT, VK_COMPARE_OP_LESS, VK_CULL_MODE_NONE, NO_DISCARD, NO_STENCIL);
println
    fillStencilGlossyPipe.subpassId = 2;
    render.createRasterPipeline (&fillStencilGlossyPipe, 0, {
        {"shaders/compiled/fullscreenTriag.vert.spv", VK_SHADER_STAGE_VERTEX_BIT},
        {"shaders/compiled/fillStencilGlossy.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT},
    }, {/*fullscreen pass*/},
    0, VK_VERTEX_INPUT_RATE_VERTEX, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    render.swapChainExtent, {NO_BLEND}, 0, DEPTH_TEST_NONE_BIT, VK_COMPARE_OP_LESS, VK_CULL_MODE_NONE, NO_DISCARD, {
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
    render.createRasterPipeline (&fillStencilSmokePipe, 0, {
        {"shaders/compiled/fillStencilSmoke.vert.spv", VK_SHADER_STAGE_VERTEX_BIT},
        {"shaders/compiled/fillStencilSmoke.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT},
    }, {/*passed as push constants lol*/},
    0, VK_VERTEX_INPUT_RATE_VERTEX, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    render.swapChainExtent, {BLEND_REPLACE_IF_GREATER, BLEND_REPLACE_IF_LESS}, sizeof (vec3) + sizeof (int) + sizeof (vec4), DEPTH_TEST_READ_BIT, VK_COMPARE_OP_LESS, VK_CULL_MODE_NONE, NO_DISCARD, {
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
    render.createRasterPipeline (&glossyPipe, 0, {
        {"shaders/compiled/fullscreenTriag.vert.spv", VK_SHADER_STAGE_VERTEX_BIT},
        {"shaders/compiled/glossy.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT},
    }, {/*fullscreen pass*/},
    0, VK_VERTEX_INPUT_RATE_VERTEX, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    render.swapChainExtent, {BLEND_MIX}, sizeof (vec4) + sizeof (vec4), DEPTH_TEST_NONE_BIT, VK_COMPARE_OP_LESS, VK_CULL_MODE_NONE, NO_DISCARD, {
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
    render.createRasterPipeline (&smokePipe, 0, {
        {"shaders/compiled/fullscreenTriag.vert.spv", VK_SHADER_STAGE_VERTEX_BIT},
        {"shaders/compiled/smoke.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT},
    }, {/*fullscreen pass*/},
    0, VK_VERTEX_INPUT_RATE_VERTEX, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    render.swapChainExtent, {BLEND_MIX}, 0, DEPTH_TEST_NONE_BIT, VK_COMPARE_OP_LESS, VK_CULL_MODE_NONE, NO_DISCARD, {
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
    render.createRasterPipeline (&tonemapPipe, 0, {
        {"shaders/compiled/fullscreenTriag.vert.spv", VK_SHADER_STAGE_VERTEX_BIT},
        {"shaders/compiled/tonemap.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT},
    }, {/*fullscreen pass*/},
    0, VK_VERTEX_INPUT_RATE_VERTEX, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    render.swapChainExtent, {NO_BLEND}, 0, DEPTH_TEST_NONE_BIT, VK_COMPARE_OP_LESS, VK_CULL_MODE_NONE, NO_DISCARD, NO_STENCIL);
println
    overlayPipe.subpassId = 7;
    render.createRasterPipeline (&overlayPipe, 0, {
        {"shaders/compiled/overlay.vert.spv", VK_SHADER_STAGE_VERTEX_BIT},
        {"shaders/compiled/overlay.frag.spv", VK_SHADER_STAGE_FRAGMENT_BIT},
    }, {
        {VK_FORMAT_R32G32_SFLOAT, offsetof (Rml::Vertex, position)},
        {VK_FORMAT_R8G8B8A8_UNORM, offsetof (Rml::Vertex, colour)},
        {VK_FORMAT_R32G32_SFLOAT, offsetof (Rml::Vertex, tex_coord)},
    },
    sizeof (Rml::Vertex), VK_VERTEX_INPUT_RATE_VERTEX, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    render.swapChainExtent, {BLEND_MIX}, sizeof (vec4) + sizeof (mat4), DEPTH_TEST_NONE_BIT, VK_COMPARE_OP_LESS, VK_CULL_MODE_NONE, NO_DISCARD, NO_STENCIL);
println
    render.createComputePipeline (&radiancePipe, 0, "shaders/compiled/radiance.comp.spv", sizeof (int) * 4, VK_PIPELINE_CREATE_DISPATCH_BASE_BIT);
println
    render.createComputePipeline (&updateGrassPipe, 0, "shaders/compiled/updateGrass.comp.spv", sizeof (vec2) * 2 + sizeof (float), 0);
println
    render.createComputePipeline (&updateWaterPipe, 0, "shaders/compiled/updateWater.comp.spv", sizeof (float) + sizeof (vec2) * 2, 0);
println
    render.createComputePipeline (&genPerlin2dPipe, 0, "shaders/compiled/perlin2.comp.spv", 0, 0);
println
    render.createComputePipeline (&genPerlin3dPipe, 0, "shaders/compiled/perlin3.comp.spv", 0, 0);
println
    // render.createComputePipeline(&dfxPipe,0, "shaders/compiled./dfx.spv", 0, 0);
    // render.createComputePipeline(&dfyPipe,0, "shaders/compiled./dfy.spv", 0, 0);
    // render.createComputePipeline(&dfzPipe,0, "shaders/compiled./dfz.spv", 0, 0);
    // render.createComputePipeline(&bitmaskPipe,0, "shaders/compiled/bit.mask.spv", 0, 0);
    render.createComputePipeline (&mapPipe, mapPushLayout, "shaders/compiled/map.comp.spv", sizeof (mat4) + sizeof (ivec4), 0);
println
}

VkResult LumRenderer::createSwapchainDependent() {
    render.deviceWaitIdle();

    // vkResetCommandPool(render.device, render.commandPool, VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT);
    vkResetCommandPool(render.device, render.commandPool, 0);

    render.createCommandBuffers(&computeCommandBuffers, render.settings.fif);
    render.createCommandBuffers(&graphicsCommandBuffers, render.settings.fif);
    render.createCommandBuffers(&lightmapCommandBuffers, render.settings.fif);
    render.createCommandBuffers(&copyCommandBuffers, render.settings.fif);

    render.mainCommandBuffers = &computeCommandBuffers;
    render.extraCommandBuffers = &copyCommandBuffers;

    createSwapchainDependentImages();

        render.createRenderPass({
            {&lightmap, Clear, Store, DontCare, DontCare, {.depthStencil = {.depth = 1}}}
        }, {
            {{&lightmapBlocksPipe, &lightmapModelsPipe}, {}, {}, &lightmap}
        }, &lightmapRpass);
println
    render.createRenderPass({
            {&highresMatNorm, DontCare, Store, DontCare, DontCare},
            {&highresDepthStencil, Clear, Store, Clear, Store, {.depthStencil = {.depth = 1}}},
        }, {
            {{&raygenBlocksPipe}, {}, {&highresMatNorm}, &highresDepthStencil},
            {{&raygenModelsPipe}, {}, {&highresMatNorm}, &highresDepthStencil},
            {{&raygenParticlesPipe}, {}, {&highresMatNorm}, &highresDepthStencil},
            {{&raygenGrassPipe}, {}, {&highresMatNorm}, &highresDepthStencil},
            {{&raygenWaterPipe}, {}, {&highresMatNorm}, &highresDepthStencil},
        }, &gbufferRpass);
println
    render.createRenderPass({
            {&highresMatNorm,      Load    , DontCare, DontCare, DontCare},
            {&highresFrame,        DontCare, DontCare, DontCare, DontCare},
            {&render.swapchainImages,  DontCare, Store   , DontCare, DontCare, {}, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR},
            {&highresDepthStencil, Load    , DontCare, Load    , DontCare, {.depthStencil = {.depth = 1}}},
            {&farDepth,           Clear   , DontCare, DontCare, DontCare, {.color = {.float32 = {-1000.0,-1000.0,-1000.0,-1000.0}}}},
            {&nearDepth,            Clear   , DontCare, DontCare, DontCare, {.color = {.float32 = {+1000.0,+1000.0,+1000.0,+1000.0}}}},
        }, {
            {{&diffusePipe},           {&highresMatNorm, &highresDepthStencil}, {&highresFrame},         NULL},
            {{&aoPipe},                {&highresMatNorm, &highresDepthStencil}, {&highresFrame},         NULL},
            {{&fillStencilGlossyPipe}, {&highresMatNorm},                       {/*empty*/},             &highresDepthStencil},
            {{&fillStencilSmokePipe }, {/*empty*/},                             {&farDepth, &nearDepth}, &highresDepthStencil},
            {{&glossyPipe},            {/*empty*/},                             {&highresFrame},         &highresDepthStencil},
            {{&smokePipe},             {&nearDepth, &farDepth},                 {&highresFrame},         &highresDepthStencil},
            {{&tonemapPipe},           {&highresFrame},                         {&render.swapchainImages},   NULL},
            {{&overlayPipe},           {/*empty*/},                             {&render.swapchainImages},   NULL},
        }, &shadeRpass);
println
    setupDescriptors();
println
    createPipilines();
    
    return VK_SUCCESS;
}

void LumRenderer::createSwapchainDependentImages() {
    // int mipmaps;
println
    render.createImageStorages (&highresMatNorm,
        VK_IMAGE_TYPE_2D,
        MATNORM_FORMAT,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT,
    {render.swapChainExtent.width, render.swapChainExtent.height, 1});
    render.createImageStorages (&highresDepthStencil,
        VK_IMAGE_TYPE_2D,
        DEPTH_FORMAT,
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT,
    {render.swapChainExtent.width, render.swapChainExtent.height, 1});
    render.createImageStorages (&highresFrame,
        VK_IMAGE_TYPE_2D,
        FRAME_FORMAT,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT,
    {render.swapChainExtent.width, render.swapChainExtent.height, 1});
        //set them to the same ptrs
    // lowresMatNorm = highresMatNorm;
    // highresDepthStencil = highresDepthStencil;

    stencilViewForDS.allocate(render.settings.fif);
    for(int i=0; i<render.settings.fif; i++){
        VkImageViewCreateInfo 
            viewInfo = {};
            viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            viewInfo.image = highresDepthStencil[i].image;
            viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
            viewInfo.format = DEPTH_FORMAT;
            viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_STENCIL_BIT; //create stencil view yourself
            viewInfo.subresourceRange.baseMipLevel = 0;
            viewInfo.subresourceRange.baseArrayLayer = 0;
            viewInfo.subresourceRange.levelCount = 1;
            viewInfo.subresourceRange.layerCount = 1;
        VK_CHECK (vkCreateImageView (render.device, &viewInfo, NULL, &stencilViewForDS[i]));
    }
    //required anyways
    render.createImageStorages (&farDepth, //smoke depth
        VK_IMAGE_TYPE_2D,
        SECONDARY_DEPTH_FORMAT,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT,
    {render.swapChainExtent.width, render.swapChainExtent.height, 1});
    render.createImageStorages (&nearDepth, //smoke depth
        VK_IMAGE_TYPE_2D,
        SECONDARY_DEPTH_FORMAT,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT,
    {render.swapChainExtent.width, render.swapChainExtent.height, 1});
    // render.transitionImageLayoutSingletime(&farDepth[0], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

void LumRenderer::cleanup() {
    delete ui_render_interface;
    render.destroyImages (&radianceCache);
    render.destroyImages (&world);
    render.destroyImages (&originBlockPalette);
    // render.deleteImages(&distancePalette);
    // render.deleteImages(&bitPalette);
    render.destroyImages (&materialPalette);
    render.destroyImages (&perlinNoise2d);
    render.destroyImages (&perlinNoise3d);
    render.destroyImages (&grassState);
    render.destroyImages (&waterState);
    render.destroyImages (&lightmap);
    render.destroySampler (nearestSampler);
    render.destroySampler (linearSampler);
    render.destroySampler (linearSampler_tiled);
    render.destroySampler (linearSampler_tiled_mirrored);
    render.destroySampler (overlaySampler);
    render.destroySampler (shadowSampler);
    render.destroySampler (unnormLinear);
    render.destroySampler (unnormNearest);
    for (int i = 0; i < MAX_FRAMES_IN_FLIGHT; i++) {
        vmaUnmapMemory (render.VMAllocator, stagingWorld[i].alloc);
        vmaUnmapMemory (render.VMAllocator, gpuParticles[i].alloc);
        vmaUnmapMemory (render.VMAllocator, stagingRadianceUpdates[i].alloc);
        vmaDestroyBuffer (render.VMAllocator, stagingWorld[i].buffer, stagingWorld[i].alloc);
        vmaDestroyBuffer (render.VMAllocator, gpuParticles[i].buffer, gpuParticles[i].alloc);
        vmaDestroyBuffer (render.VMAllocator, uniform[i].buffer, uniform[i].alloc);
        vmaDestroyBuffer (render.VMAllocator, lightUniform[i].buffer, lightUniform[i].alloc);
        vmaDestroyBuffer (render.VMAllocator, aoLutUniform[i].buffer, aoLutUniform[i].alloc);
        vmaDestroyBuffer (render.VMAllocator, stagingRadianceUpdates[i].buffer, stagingRadianceUpdates[i].alloc);
        // render.deleteBuffers()
        vmaDestroyBuffer (render.VMAllocator, gpuRadianceUpdates[i].buffer, gpuRadianceUpdates[i].alloc);
    }
    cleanupSwapchainDependent();

    render.cleanup();
}

VkResult LumRenderer::cleanupSwapchainDependent() {
    render.deviceWaitIdle();
    vkResetCommandPool(render.device, render.commandPool, 0);

    render.destroyRenderPass (&lightmapRpass);
    render.destroyRenderPass (&gbufferRpass);
    render.destroyRenderPass (&shadeRpass);
println
    render.destroyComputePipeline ( &mapPipe);
    vkDestroyDescriptorSetLayout (render.device, mapPushLayout, NULL);
    render.destroyComputePipeline ( &raytracePipe);
    render.destroyComputePipeline ( &radiancePipe);
    render.destroyComputePipeline ( &updateGrassPipe);
    render.destroyComputePipeline ( &updateWaterPipe);
    render.destroyComputePipeline ( &genPerlin2dPipe);
    render.destroyComputePipeline ( &genPerlin3dPipe);
    // render.destroyComputePipeline(   &dfxPipe);
    // render.destroyComputePipeline(   &dfyPipe);
    // render.destroyComputePipeline(   &dfzPipe);
    // render.destroyComputePipeline(   &bitmaskPipe);
    render.destroyRasterPipeline (&lightmapBlocksPipe);
    render.destroyRasterPipeline (&lightmapModelsPipe);
    vkDestroyDescriptorSetLayout (render.device, raygenModelsPushLayout, NULL);
    render.destroyRasterPipeline (&raygenBlocksPipe);
    render.destroyRasterPipeline (&raygenModelsPipe);
    render.destroyRasterPipeline (&raygenParticlesPipe);
    render.destroyRasterPipeline (&raygenGrassPipe);
    render.destroyRasterPipeline (&raygenWaterPipe);
    render.destroyRasterPipeline (&diffusePipe);
    render.destroyRasterPipeline (&aoPipe);
    render.destroyRasterPipeline (&fillStencilSmokePipe);
    render.destroyRasterPipeline (&fillStencilGlossyPipe);
    render.destroyRasterPipeline (&glossyPipe);
    render.destroyRasterPipeline (&smokePipe);
    render.destroyRasterPipeline (&tonemapPipe);
    render.destroyRasterPipeline (&overlayPipe);
    
println
    render.destroyImages (&highresFrame);
println
    render.destroyImages (&highresDepthStencil);
println
    for(auto v : stencilViewForDS){
        vkDestroyImageView (render.device, v, NULL);
    }
println
    render.destroyImages (&highresMatNorm);
println
    render.destroyImages (&farDepth);
println
    render.destroyImages (&nearDepth);
println

    return VK_SUCCESS;
}

void LumRenderer::gen_perlin_2d() {
    VkCommandBuffer commandBuffer = render.beginSingleTimeCommands();
    // render.cmdBindPipe(commandBuffer, genPerlin2dPipe);
    vkCmdBindPipeline (commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, genPerlin2dPipe.line);
    for(int i=0; i<render.settings.fif; i++){
        vkCmdBindDescriptorSets (commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, genPerlin2dPipe.lineLayout, 0, 1, &genPerlin2dPipe.sets[i], 0, 0);
        render.cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
            perlinNoise2d.current());
        vkCmdDispatch (commandBuffer, world_size.x / 8, world_size.y / 8, 1);
        render.cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
            perlinNoise2d.current());
    }
    render.endSingleTimeCommands (commandBuffer);
}

void LumRenderer::gen_perlin_3d() {
    VkCommandBuffer commandBuffer = render.beginSingleTimeCommands();

    vkCmdBindPipeline (commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, genPerlin3dPipe.line);
    for(int i=0; i<render.settings.fif; i++){
        vkCmdBindDescriptorSets (commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, genPerlin3dPipe.lineLayout, 0, 1, &genPerlin3dPipe.sets[i], 0, 0);
        render.cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
            perlinNoise3d.current());
        vkCmdDispatch (commandBuffer, 64 / 4, 64 / 4, 64 / 4);
        render.cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
            perlinNoise3d.current());
    }
    render.endSingleTimeCommands (commandBuffer);
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

void LumRenderer::update_light_transform() {
    dvec3 horizon = normalize (cross (dvec3 (1, 0, 0), lightDir));
    dvec3 up = normalize (cross (horizon, lightDir)); // Up vector
    // dvec3 light_pos = dvec3(dvec2(world_size*16),0)/2.0 - 5*16.0*lightDir;
    dvec3 light_pos = dvec3 (dvec2 (dvec3(world_size) * 16.0), 0) / 2.0 - 1 * 16.0 * lightDir;
    dmat4 view = glm::lookAt (light_pos, light_pos + lightDir, up);
    const double voxel_in_pixels = 5.0; //we want voxel to be around 10 pixels in width / height
    const double view_width_in_voxels = 3000.0 / voxel_in_pixels; //todo make dynamic and use spec constnats
    const double view_height_in_voxels = 3000.0 / voxel_in_pixels;
    dmat4 projection = glm::ortho (-view_width_in_voxels / 2.0, view_width_in_voxels / 2.0, view_height_in_voxels / 2.0, -view_height_in_voxels / 2.0, -350.0, +350.0); // => *(2000.0/2) for decoding
    dmat4 worldToScreen = projection * view;
    lightTransform = worldToScreen;
}

// #include <glm/gtx/string_cast.hpp>
void LumRenderer::start_frame() {
    println
    camera.updateCamera();
    println
    update_light_transform();
    println
    assert(computeCommandBuffers.current());
    assert(graphicsCommandBuffers.current());
    assert(copyCommandBuffers.current());
    assert(lightmapCommandBuffers.current());
    println
    render.start_frame({
        computeCommandBuffers.current(),
        graphicsCommandBuffers.current(),
        copyCommandBuffers.current(),
        lightmapCommandBuffers.current(),
    });
    println
}

void LumRenderer::start_blockify() {
    VkCommandBuffer& commandBuffer = computeCommandBuffers.current();
    blockCopyQueue.clear();
    paletteCounter = static_block_palette_size;
    size_t size_to_copy = world_size.x * world_size.y * world_size.z * sizeof (BlockID_t);
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
void LumRenderer::blockify_mesh (Mesh* mesh) {
    mat4 rotate = toMat4 ((*mesh).rot);
    mat4 shift = translate (identity<mat4>(), (*mesh).shift);
    struct AABB border_in_voxel = get_shift (shift* rotate, (*mesh).size);
    struct {ivec3 min; ivec3 max;} border;
    border.min = (ivec3 (border_in_voxel.min) - ivec3 (1)) / ivec3 (16);
    border.max = (ivec3 (border_in_voxel.max) + ivec3 (1)) / ivec3 (16);
    border.min = glm::clamp (border.min, ivec3 (0), ivec3 (world_size - u32(1)));
    border.max = glm::clamp (border.max, ivec3 (0), ivec3 (world_size - u32(1)));
    for (int xx = border.min.x; xx <= border.max.x; xx++) {
        for (int yy = border.min.y; yy <= border.max.y; yy++) {
            for (int zz = border.min.z; zz <= border.max.z; zz++) {
                //if inside model TODO
                //zero clear TODO
                int current_block = current_world (xx, yy, zz);
                if (current_block < static_block_palette_size) { // static
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

void LumRenderer::end_blockify() {
    size_t size_to_copy = world_size.x * world_size.y * world_size.z * sizeof (BlockID_t);
    assert (size_to_copy != 0);
    memcpy (stagingWorld.current().mapped, current_world.data(), size_to_copy);
    vmaFlushAllocation (render.VMAllocator, stagingWorld.current().alloc, 0, size_to_copy);
}

void LumRenderer::blockify_custom (void* ptr) {
    size_t size_to_copy = world_size.x * world_size.y * world_size.z * sizeof (BlockID_t);
    memcpy (stagingWorld.current().mapped, ptr, size_to_copy);
    vmaFlushAllocation (render.VMAllocator, stagingWorld.current().alloc, 0, size_to_copy);
}

#include <glm/gtx/hash.hpp>
void LumRenderer::update_radiance() {
    VkCommandBuffer& commandBuffer = computeCommandBuffers.current();
    robin_hood::unordered_set<i8vec3> set = {};
    radianceUpdates.clear();
    for (int xx = 0; xx < world_size.x; xx++) {
        for (int yy = 0; yy < world_size.y; yy++) {
            for (int zz = 0; zz < world_size.z; zz++) {
                // int block_id = current_world(xx,yy,zz);
                //yeah kinda slow... but who cares on less then a million blocks?
                int sum_of_neighbours = 0;
                for (int dx = -1; (dx <= +1); dx++) {
                    for (int dy = -1; (dy <= +1); dy++) {
                        for (int dz = -1; (dz <= +1); dz++) {
                            ivec3 test_block = ivec3 (xx + dx, yy + dy, zz + dz);
                            // test_block = clamp(test_block, ivec3(0), world_size-1);
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
    memcpy (stagingRadianceUpdates.current().mapped, radianceUpdates.data(), bufferSize);

    VkBufferCopy
        copyRegion = {};
        copyRegion.size = bufferSize;
    vkCmdCopyBuffer (commandBuffer, stagingRadianceUpdates.current().buffer, gpuRadianceUpdates.current().buffer, 1, &copyRegion);
    render.cmdPipelineBarrier (commandBuffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
        gpuRadianceUpdates.current());
    render.cmdBindPipe(commandBuffer, radiancePipe);
    vkCmdBindDescriptorSets (commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, radiancePipe.lineLayout, 0, 1, &radiancePipe.sets.current(), 0, 0);
    int magic_number = render.iFrame % 2;
    //if fast than increase work
    if (timeTakenByRadiance < 1.8) {
        magicSize --;
        //if slow than decrease work
    } else if (timeTakenByRadiance > 2.2) {
        magicSize ++;
    }
    magicSize = glm::max (magicSize, 1); //never remove
    magicSize = glm::min (magicSize, 10);
    struct rtpc {int time, iters, size, shift;} pushconstant = {render.iFrame, 0, magicSize, render.iFrame % magicSize};
    vkCmdPushConstants (commandBuffer, radiancePipe.lineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof (pushconstant), &pushconstant);
    int wg_count = radianceUpdates.size() / magicSize;
    PLACE_TIMESTAMP_OUTSIDE(commandBuffer);
    vkCmdDispatch (commandBuffer, wg_count, 1, 1);
    PLACE_TIMESTAMP_OUTSIDE(commandBuffer);
    render.cmdPipelineBarrier (commandBuffer,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_WRITE_BIT,
        radianceCache.current());
    render.cmdPipelineBarrier (commandBuffer,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
        radianceCache.current());
}

void LumRenderer::recalculate_df() {
    VkCommandBuffer& commandBuffer = computeCommandBuffers.current();
    render.cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
        distancePalette.current());
    render.cmdBindPipe(commandBuffer, dfxPipe);
    vkCmdBindDescriptorSets (commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, dfxPipe.lineLayout, 0, 1, &dfxPipe.sets.current(), 0, 0);
    vkCmdDispatch (commandBuffer, 1, 1, 16 * paletteCounter);
    render.cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
        distancePalette.current());
    render.cmdBindPipe(commandBuffer, dfyPipe);
    vkCmdBindDescriptorSets (commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, dfyPipe.lineLayout, 0, 1, &dfyPipe.sets.current(), 0, 0);
    vkCmdDispatch (commandBuffer, 1, 1, 16 * paletteCounter);
    render.cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
        distancePalette.current());
    render.cmdBindPipe(commandBuffer, dfzPipe);
    vkCmdBindDescriptorSets (commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, dfzPipe.lineLayout, 0, 1, &dfzPipe.sets.current(), 0, 0);
    vkCmdDispatch (commandBuffer, 1, 1, 16 * paletteCounter);
    render.cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
        distancePalette.current());
}

void LumRenderer::recalculate_bit() {
    VkCommandBuffer& commandBuffer = computeCommandBuffers.current();
    render.cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
        bitPalette.current());
    render.cmdBindPipe(commandBuffer, bitmaskPipe);
    vkCmdBindDescriptorSets (commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, bitmaskPipe.lineLayout, 0, 1, &bitmaskPipe.sets.current(), 0, 0);
    vkCmdDispatch (commandBuffer, 1, 1, 8 * paletteCounter);
    render.cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
        bitPalette.current());
}

void LumRenderer::exec_copies() {
    VkCommandBuffer& commandBuffer = computeCommandBuffers.current();
    if (blockCopyQueue.size() != 0) {
        //we can copy from previous image, cause static blocks are same in both palettes. Additionaly, it gives src and dst optimal layouts
        render.cmdExplicitTransLayoutBarrier (commandBuffer, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
            &originBlockPalette.current());
        render.cmdExplicitTransLayoutBarrier (commandBuffer, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
            &originBlockPalette.current());
        PLACE_TIMESTAMP_OUTSIDE(commandBuffer);
        vkCmdCopyImage (commandBuffer, originBlockPalette.previous().image, VK_IMAGE_LAYOUT_GENERAL, originBlockPalette.current().image, VK_IMAGE_LAYOUT_GENERAL, blockCopyQueue.size(), blockCopyQueue.data());
        PLACE_TIMESTAMP_OUTSIDE(commandBuffer);
        render.cmdExplicitTransLayoutBarrier (commandBuffer, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
            &originBlockPalette.previous());
        render.cmdExplicitTransLayoutBarrier (commandBuffer, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
            &originBlockPalette.previous());
    }
    VkBufferImageCopy copyRegion = {};
        copyRegion.imageExtent.width = world_size.x;
        copyRegion.imageExtent.height = world_size.y;
        copyRegion.imageExtent.depth = world_size.z;
        copyRegion.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copyRegion.imageSubresource.layerCount = 1;
    render.cmdExplicitTransLayoutBarrier (commandBuffer, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
        &world.current());
    vkCmdCopyBufferToImage (commandBuffer, stagingWorld.current().buffer, world.current().image, VK_IMAGE_LAYOUT_GENERAL, 1, &copyRegion);
    render.cmdExplicitTransLayoutBarrier (commandBuffer, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
        &world.current());
}

void LumRenderer::start_map() {
    VkCommandBuffer& commandBuffer = computeCommandBuffers.current();
    render.cmdBindPipe(commandBuffer, mapPipe);
    vkCmdBindDescriptorSets (commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, mapPipe.lineLayout, 0, 1, &mapPipe.sets.current(), 0, 0);
    PLACE_TIMESTAMP_OUTSIDE(commandBuffer);
}

void LumRenderer::map_mesh (Mesh* mesh) {
    VkCommandBuffer& commandBuffer = computeCommandBuffers.current();
    VkDescriptorImageInfo
        modelVoxelsInfo = {};
        modelVoxelsInfo.imageView = (*mesh).voxels.current().view; //CHANGE ME
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

void LumRenderer::end_map() {
    VkCommandBuffer& commandBuffer = computeCommandBuffers.current();
    render.cmdExplicitTransLayoutBarrier (commandBuffer, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
        &originBlockPalette.current());
    PLACE_TIMESTAMP_OUTSIDE(commandBuffer);
}

void LumRenderer::end_compute() {
    VkCommandBuffer& commandBuffer = computeCommandBuffers.current();
}

void LumRenderer::start_lightmap() {
    VkCommandBuffer& commandBuffer = lightmapCommandBuffers.current();
    PLACE_TIMESTAMP_OUTSIDE(commandBuffer);
    struct unicopy {mat4 trans;} unicopy = {lightTransform};
    vkCmdUpdateBuffer (commandBuffer, lightUniform.current().buffer, 0, sizeof (unicopy), &unicopy);
    render.cmdPipelineBarrier (commandBuffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT,
        lightUniform.current());
    render.cmdBeginRenderPass(commandBuffer, &lightmapRpass);
}

void LumRenderer::lightmap_start_blocks() {
    VkCommandBuffer& commandBuffer = lightmapCommandBuffers.current();
    render.cmdBindPipe(commandBuffer, lightmapBlocksPipe);
    vkCmdBindDescriptorSets (commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, lightmapBlocksPipe.lineLayout, 0, 1, &lightmapBlocksPipe.sets.current(), 0, 0);
}

void LumRenderer::lightmap_start_models() {
    VkCommandBuffer& commandBuffer = lightmapCommandBuffers.current();
    render.cmdBindPipe(commandBuffer, lightmapModelsPipe);
    vkCmdBindDescriptorSets (commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, lightmapModelsPipe.lineLayout, 0, 1, &lightmapModelsPipe.sets.current(), 0, 0);
}

void LumRenderer::end_lightmap() {
    VkCommandBuffer& commandBuffer = lightmapCommandBuffers.current();
    PLACE_TIMESTAMP_OUTSIDE(commandBuffer);
    render.cmdEndRenderPass (commandBuffer, &lightmapRpass);
}

void LumRenderer::start_raygen() {
    VkCommandBuffer& commandBuffer = graphicsCommandBuffers.current();
    PLACE_TIMESTAMP_OUTSIDE(commandBuffer);
    
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
        vec2 (render.swapChainExtent.width, render.swapChainExtent.height),
        render.iFrame
    };
    vkCmdUpdateBuffer (commandBuffer, uniform.current().buffer, 0, sizeof (unicopy), &unicopy);
    render.cmdPipelineBarrier (commandBuffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT,
        uniform.current());
    render.cmdBeginRenderPass(commandBuffer, &gbufferRpass);
}

void LumRenderer::raygen_start_blocks() {
    VkCommandBuffer& commandBuffer = graphicsCommandBuffers.current();
    PLACE_TIMESTAMP_OUTSIDE(commandBuffer);
    render.cmdBindPipe(commandBuffer, raygenBlocksPipe);
    vkCmdBindDescriptorSets (commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, raygenBlocksPipe.lineLayout, 0, 1, &raygenBlocksPipe.sets.current(), 0, 0);
}

static VkBuffer old_buff = NULL;
static bool is_face_visible (vec3 normal, vec3 camera_dir) {
    return (dot (normal, camera_dir) < 0.0f);
}

#define CHECK_N_DRAW_BLOCK(__norm, __dir) \
if(is_face_visible(i8vec3(__norm), camera.cameraDir)) {\
    draw_block_face(__norm, (*block_mesh).triangles.__dir, block_id);\
}
void LumRenderer::draw_block_face (i8vec3 normal, IndexedVertices& buff, int block_id) {
    VkCommandBuffer& commandBuffer = graphicsCommandBuffers.current();
    assert (buff.indexes.data());
    assert (block_id);
    vkCmdBindIndexBuffer (commandBuffer, buff.indexes.current().buffer, 0, VK_INDEX_TYPE_UINT16);
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

void LumRenderer::raygen_block (Mesh* block_mesh, int block_id, ivec3 shift) {
    VkCommandBuffer& commandBuffer = graphicsCommandBuffers.current();
    VkBuffer vertexBuffers[] = { (*block_mesh).triangles.vertexes.current().buffer};
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

void LumRenderer::raygen_start_models() {
    VkCommandBuffer& commandBuffer = graphicsCommandBuffers.current();
    vkCmdNextSubpass (commandBuffer, VK_SUBPASS_CONTENTS_INLINE);
    render.cmdBindPipe(commandBuffer, raygenModelsPipe);
    vkCmdBindDescriptorSets (commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, raygenModelsPipe.lineLayout, 0, 1, &raygenModelsPipe.sets.current(), 0, 0);
}

#define CHECK_N_DRAW_MODEL(__norm, __dir) \
if(is_face_visible(model_mesh->rot*__norm, camera.cameraDir)) {\
    draw_model_face(__norm, (*model_mesh).triangles.__dir);\
}
void LumRenderer::draw_model_face (vec3 normal, IndexedVertices& buff) {
    VkCommandBuffer& commandBuffer = graphicsCommandBuffers.current();
    assert (buff.indexes.data());
    vkCmdBindIndexBuffer (commandBuffer, buff.indexes.current().buffer, 0, VK_INDEX_TYPE_UINT16);
    struct {vec4 fnorm;} norms = {vec4 (normal, 0)};
    vkCmdPushConstants (commandBuffer, raygenModelsPipe.lineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        32, sizeof (norms), &norms);
    vkCmdDrawIndexed (commandBuffer, buff.icount, 1, 0, 0, 0);
}

void LumRenderer::raygen_model (Mesh* model_mesh) {
    VkCommandBuffer& commandBuffer = graphicsCommandBuffers.current();
    VkBuffer vertexBuffers[] = { (*model_mesh).triangles.vertexes.current().buffer};
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
        modelVoxelsInfo.imageView = (*model_mesh).voxels.current().view; //CHANGE ME
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
void LumRenderer::lightmap_block_face (i8vec3 normal, IndexedVertices& buff, int block_id) {
    VkCommandBuffer& lightmap_commandBuffer = lightmapCommandBuffers.current();
    vkCmdBindIndexBuffer (lightmap_commandBuffer, buff.indexes.current().buffer, 0, VK_INDEX_TYPE_UINT16);
    vkCmdDrawIndexed (lightmap_commandBuffer, buff.icount, 1, 0, 0, 0);
}

void LumRenderer::lightmap_block (Mesh* block_mesh, int block_id, ivec3 shift) {
    VkCommandBuffer& lightmap_commandBuffer = lightmapCommandBuffers.current();
    VkBuffer vertexBuffers[] = { (*block_mesh).triangles.vertexes.current().buffer};
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
void LumRenderer::lightmap_model_face (vec3 normal, IndexedVertices& buff) {
    VkCommandBuffer& lightmap_commandBuffer = lightmapCommandBuffers.current();
    vkCmdBindIndexBuffer (lightmap_commandBuffer, buff.indexes.current().buffer, 0, VK_INDEX_TYPE_UINT16);
    vkCmdDrawIndexed (lightmap_commandBuffer, buff.icount, 1, 0, 0, 0);
}

void LumRenderer::lightmap_model (Mesh* model_mesh) {
    VkCommandBuffer& lightmap_commandBuffer = lightmapCommandBuffers.current();
    VkBuffer vertexBuffers[] = { (*model_mesh).triangles.vertexes.current().buffer};
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

void LumRenderer::update_particles() {
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
    int capped_particle_count = glm::clamp (int (particles.size()), 0, maxParticleCount);
    memcpy (gpuParticles.current().mapped, particles.data(), sizeof (Particle)*capped_particle_count);
    vmaFlushAllocation (render.VMAllocator, gpuParticles.current().alloc, 0, sizeof (Particle)*capped_particle_count);
}

void LumRenderer::raygen_map_particles() {
    VkCommandBuffer& commandBuffer = graphicsCommandBuffers.current();
    //go to next no matter what
    vkCmdNextSubpass (commandBuffer, VK_SUBPASS_CONTENTS_INLINE);
    PLACE_TIMESTAMP_OUTSIDE(commandBuffer);
    if (!particles.empty()) { //just for safity
        render.cmdBindPipe(commandBuffer, raygenParticlesPipe);
        vkCmdBindDescriptorSets (commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, raygenParticlesPipe.lineLayout, 0, 1, &raygenParticlesPipe.sets.current(), 0, 0);
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers (commandBuffer, 0, 1, &gpuParticles.current().buffer, offsets);
        vkCmdDraw (commandBuffer, particles.size(), 1, 0, 0);
    }
    PLACE_TIMESTAMP_OUTSIDE(commandBuffer);
}

void LumRenderer::raygen_start_grass() {
    VkCommandBuffer& commandBuffer = graphicsCommandBuffers.current();
    PLACE_TIMESTAMP_OUTSIDE(commandBuffer);
    vkCmdNextSubpass (commandBuffer, VK_SUBPASS_CONTENTS_INLINE);
    render.cmdBindPipe(commandBuffer, raygenGrassPipe);
    vkCmdBindDescriptorSets (commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, raygenGrassPipe.lineLayout, 0, 1, &raygenGrassPipe.sets.current(), 0, 0);
}

//DOES NOT run in renderpass. Placed here cause spatially makes sense
void LumRenderer::updade_grass (vec2 windDirection) {
    VkCommandBuffer& commandBuffer = computeCommandBuffers.current();
    render.cmdBindPipe(commandBuffer, updateGrassPipe);
    struct {vec2 wd, cp; float dt;} pushconstant = {windDirection, {}, float (render.iFrame)};
    vkCmdPushConstants (commandBuffer, updateGrassPipe.lineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof (pushconstant), &pushconstant);
    vkCmdBindDescriptorSets (commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, updateGrassPipe.lineLayout, 0, 1, &updateGrassPipe.sets[0], 0, 0);
    render.cmdPipelineBarrier (commandBuffer,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_WRITE_BIT,
        grassState.current());
    render.cmdPipelineBarrier (commandBuffer,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
        grassState.current());
    PLACE_TIMESTAMP_OUTSIDE(commandBuffer);
    vkCmdDispatch (commandBuffer, (world_size.x * 2 + 7) / 8, (world_size.y * 2 + 7) / 8, 1); //2x8 2x8 1x1
    PLACE_TIMESTAMP_OUTSIDE(commandBuffer);
}

void LumRenderer::updade_water() {
    VkCommandBuffer& commandBuffer = computeCommandBuffers.current();
    render.cmdBindPipe(commandBuffer, updateWaterPipe);
    struct {vec2 wd; float dt;} pushconstant = {{}, float (render.iFrame)};
    vkCmdPushConstants (commandBuffer, updateWaterPipe.lineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof (pushconstant), &pushconstant);
    vkCmdBindDescriptorSets (commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, updateWaterPipe.lineLayout, 0, 1, &updateWaterPipe.sets[0], 0, 0);
    render.cmdPipelineBarrier (commandBuffer,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_WRITE_BIT,
        waterState.current());
    render.cmdPipelineBarrier (commandBuffer,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
        waterState.current());
    PLACE_TIMESTAMP_OUTSIDE(commandBuffer);
    vkCmdDispatch (commandBuffer, (world_size.x * 2 + 7) / 8, (world_size.y * 2 + 7) / 8, 1); //2x8 2x8 1x1
    PLACE_TIMESTAMP_OUTSIDE(commandBuffer);
}

//blade is hardcoded but it does not really increase perfomance
//done this way for simplicity, can easilly be replaced with any vertex buffer
void LumRenderer::raygen_map_grass (vec4 shift, int size) {
    VkCommandBuffer& commandBuffer = graphicsCommandBuffers.current();
    bool x_flip = camera.cameraDir.x < 0;
    bool y_flip = camera.cameraDir.y < 0;
    struct {vec4 _shift; int _size, _time; int xf, yf;} raygen_pushconstant = {shift, size, render.iFrame, x_flip, y_flip};
    vkCmdPushConstants (commandBuffer, raygenGrassPipe.lineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof (raygen_pushconstant), &raygen_pushconstant);
    const int verts_per_blade = 6; //for triangle strip
    const int blade_per_instance = 1; //for triangle strip
    vkCmdDraw (commandBuffer,
               verts_per_blade* blade_per_instance,
               (size* size + (blade_per_instance - 1)) / blade_per_instance,
               0, 0);
}

void LumRenderer::raygen_start_water() {
    VkCommandBuffer& commandBuffer = graphicsCommandBuffers.current();
    PLACE_TIMESTAMP_OUTSIDE(commandBuffer);
    PLACE_TIMESTAMP_OUTSIDE(commandBuffer);
    vkCmdNextSubpass (commandBuffer, VK_SUBPASS_CONTENTS_INLINE);
    render.cmdBindPipe(commandBuffer, raygenWaterPipe);
    vkCmdBindDescriptorSets (commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, raygenWaterPipe.lineLayout, 0, 1, &raygenWaterPipe.sets.current(), 0, 0);
}

void LumRenderer::raygen_map_water (vec4 shift, int qualitySize) {
    VkCommandBuffer& commandBuffer = graphicsCommandBuffers.current();
    struct {vec4 _shift; int _size, _time;} raygen_pushconstant = {shift, qualitySize, render.iFrame};
    vkCmdPushConstants (commandBuffer, raygenWaterPipe.lineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof (raygen_pushconstant), &raygen_pushconstant);
    const int verts_per_water_tape = qualitySize * 2 + 2;
    const int tapes_per_block = qualitySize;
    vkCmdDraw (commandBuffer,
               verts_per_water_tape,
               tapes_per_block,
               0, 0);
}

void LumRenderer::end_raygen() {
    VkCommandBuffer& commandBuffer = graphicsCommandBuffers.current();
    render.cmdEndRenderPass (commandBuffer, &gbufferRpass);
}

void LumRenderer::start_2nd_spass() {
    VkCommandBuffer& commandBuffer = graphicsCommandBuffers.current();
    vector<AoLut> ao_lut = generateLUT (8, 16.0 / 1000.0,
                        dvec2 (render.swapChainExtent.width, render.swapChainExtent.height),
                        dvec3 (camera.horizline* camera.viewSize.x / 2.0), dvec3 (camera.vertiline* camera.viewSize.y / 2.0));
    vkCmdUpdateBuffer (commandBuffer, aoLutUniform.current().buffer, 0, sizeof (AoLut)*ao_lut.size(), ao_lut.data());
    render.cmdPipelineBarrier (commandBuffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT,
        aoLutUniform.current());
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
    // VkRenderPassBeginInfo 
    //     renderPassInfo = {};
    //     renderPassInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
    //     renderPassInfo.renderPass = shadeRpass;
    //     renderPassInfo.framebuffer = altFramebuffers[imageIndex];
    //     renderPassInfo.renderArea.offset = {0, 0};
    //     renderPassInfo.renderArea.extent = {render.swapChainExtent.width, render.swapChainExtent.height};
    //     renderPassInfo.clearValueCount = clearColors.size();
    //     renderPassInfo.pClearValues = clearColors.data();
    // vkCmdBeginRenderPass (commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    render.cmdBeginRenderPass(commandBuffer, &shadeRpass);

    // cmdSetViewport(commandBuffer, render.swapChainExtent);
}

void LumRenderer::diffuse() {
    VkCommandBuffer& commandBuffer = graphicsCommandBuffers.current();
    PLACE_TIMESTAMP_OUTSIDE(commandBuffer);
    render.cmdBindPipe(commandBuffer, diffusePipe);
    vkCmdBindDescriptorSets (commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, diffusePipe.lineLayout, 0, 1, &diffusePipe.sets.current(), 0, 0);
    struct rtpc {vec4 v1, v2; mat4 lp;} pushconstant = {vec4 (camera.cameraPos, intBitsToFloat (render.iFrame)), vec4 (camera.cameraDir, 0), lightTransform};
    vkCmdPushConstants (commandBuffer, diffusePipe.lineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof (pushconstant), &pushconstant);
    PLACE_TIMESTAMP_OUTSIDE(commandBuffer);
    vkCmdDraw (commandBuffer, 3, 1, 0, 0);
    PLACE_TIMESTAMP_OUTSIDE(commandBuffer);
}

void LumRenderer::ambient_occlusion() {
    VkCommandBuffer& commandBuffer = graphicsCommandBuffers.current();
    vkCmdNextSubpass (commandBuffer, VK_SUBPASS_CONTENTS_INLINE);
    render.cmdBindPipe(commandBuffer, aoPipe);
    vkCmdBindDescriptorSets (commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, aoPipe.lineLayout, 0, 1, &aoPipe.sets.current(), 0, 0);
    PLACE_TIMESTAMP_OUTSIDE(commandBuffer);
    vkCmdDraw (commandBuffer, 3, 1, 0, 0);
    PLACE_TIMESTAMP_OUTSIDE(commandBuffer);
}

void LumRenderer::glossy_raygen() {
    VkCommandBuffer& commandBuffer = graphicsCommandBuffers.current();
    vkCmdNextSubpass (commandBuffer, VK_SUBPASS_CONTENTS_INLINE);
    render.cmdBindPipe(commandBuffer, fillStencilGlossyPipe);
    vkCmdBindDescriptorSets (commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, fillStencilGlossyPipe.lineLayout, 0, 1, &fillStencilGlossyPipe.sets.current(), 0, 0);
    PLACE_TIMESTAMP_OUTSIDE(commandBuffer);
    vkCmdDraw (commandBuffer, 3, 1, 0, 0);
    PLACE_TIMESTAMP_OUTSIDE(commandBuffer);
}

void LumRenderer::smoke_raygen() {
    VkCommandBuffer& commandBuffer = graphicsCommandBuffers.current();
    vkCmdNextSubpass (commandBuffer, VK_SUBPASS_CONTENTS_INLINE);
    render.cmdBindPipe(commandBuffer, fillStencilSmokePipe);
    vkCmdBindDescriptorSets (commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, fillStencilSmokePipe.lineLayout, 0, 1, &fillStencilSmokePipe.sets.current(), 0, 0);
    struct rtpc {vec4 centerSize;} pushconstant = {vec4 (vec3 (11, 11, 1.5) * 16.f, 32)};
    specialRadianceUpdates.push_back (ivec4 (11, 11, 1, 0));
    vkCmdPushConstants (commandBuffer, fillStencilSmokePipe.lineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof (pushconstant), &pushconstant);
    PLACE_TIMESTAMP_OUTSIDE(commandBuffer);
    vkCmdDraw (commandBuffer, 36, 1, 0, 0);
    PLACE_TIMESTAMP_OUTSIDE(commandBuffer);
}

void LumRenderer::smoke() {
    VkCommandBuffer& commandBuffer = graphicsCommandBuffers.current();
    vkCmdNextSubpass (commandBuffer, VK_SUBPASS_CONTENTS_INLINE);
    render.cmdBindPipe(commandBuffer, smokePipe);
    vkCmdBindDescriptorSets (commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, smokePipe.lineLayout, 0, 1, &smokePipe.sets.current(), 0, 0);
    PLACE_TIMESTAMP_OUTSIDE(commandBuffer);
    vkCmdDraw (commandBuffer, 3, 1, 0, 0);
    PLACE_TIMESTAMP_OUTSIDE(commandBuffer);
}

void LumRenderer::glossy() {
    VkCommandBuffer& commandBuffer = graphicsCommandBuffers.current();
    vkCmdNextSubpass (commandBuffer, VK_SUBPASS_CONTENTS_INLINE);
    render.cmdBindPipe(commandBuffer, glossyPipe);
    vkCmdBindDescriptorSets (commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, glossyPipe.lineLayout, 0, 1, &glossyPipe.sets.current(), 0, 0);
    struct rtpc {vec4 v1, v2;} pushconstant = {vec4 (camera.cameraPos, 0), vec4 (camera.cameraDir, 0)};
    vkCmdPushConstants (commandBuffer, glossyPipe.lineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof (pushconstant), &pushconstant);
    PLACE_TIMESTAMP_OUTSIDE(commandBuffer);
    vkCmdDraw (commandBuffer, 3, 1, 0, 0);
    PLACE_TIMESTAMP_OUTSIDE(commandBuffer);
}

void LumRenderer::tonemap() {
    VkCommandBuffer& commandBuffer = graphicsCommandBuffers.current();
    vkCmdNextSubpass (commandBuffer, VK_SUBPASS_CONTENTS_INLINE);
    render.cmdBindPipe(commandBuffer, tonemapPipe);
    vkCmdBindDescriptorSets (commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, tonemapPipe.lineLayout, 0, 1, &tonemapPipe.sets.current(), 0, 0);
    PLACE_TIMESTAMP_OUTSIDE(commandBuffer);
    vkCmdDraw (commandBuffer, 3, 1, 0, 0);
    PLACE_TIMESTAMP_OUTSIDE(commandBuffer);
}

void LumRenderer::end_2nd_spass() {
    VkCommandBuffer& commandBuffer = graphicsCommandBuffers.current();
    // render.cmdEndRenderPass(commandBuffer);
}

void LumRenderer::start_ui() {
    VkCommandBuffer& commandBuffer = graphicsCommandBuffers.current();
    PLACE_TIMESTAMP_OUTSIDE(commandBuffer);
    vkCmdNextSubpass (commandBuffer, VK_SUBPASS_CONTENTS_INLINE);
    //no dset
    vkCmdBindPipeline (commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, overlayPipe.line);
    VkViewport 
        viewport = {};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (float) (render.swapChainExtent.width );
        viewport.height = (float) (render.swapChainExtent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
    vkCmdSetViewport (commandBuffer, 0, 1, &viewport);
        ui_render_interface->last_scissors.offset = {0, 0};
        ui_render_interface->last_scissors.extent = render.swapChainExtent;
    vkCmdSetScissor (commandBuffer, 0, 1, &ui_render_interface->last_scissors);
}

void LumRenderer::end_ui() {
    VkCommandBuffer& commandBuffer = graphicsCommandBuffers.current();
    render.cmdEndRenderPass (commandBuffer, &shadeRpass); //end of blur2present rpass
    PLACE_TIMESTAMP_OUTSIDE(commandBuffer);
}

void LumRenderer::end_frame() {
    render.end_frame({
        // "Special" cmb used by UI copies & layout transitions HAS to be first
        // Otherwise image state is LAYOUT_UNDEFINED
        copyCommandBuffers.current(),
        computeCommandBuffers.current(),
        lightmapCommandBuffers.current(),
        graphicsCommandBuffers.current(),
    });

println
    copyCommandBuffers.move(); //runtime copies for ui. Also does first frame resources
println
    computeCommandBuffers.move();
println
    lightmapCommandBuffers.move();
println
    graphicsCommandBuffers.move();
println
    lightmap.move();
println
    highresFrame.move();
println
    highresDepthStencil.move();
println
    highresMatNorm.move();
println
    
    stencilViewForDS.move();
println
    farDepth.move(); //represents how much should smoke traversal for
println
    nearDepth.move(); //represents how much should smoke traversal for
println
    // maskFrame.move(); //where lowres renders to. Blends with highres afterwards
println
    stagingWorld.move();
println
    world.move(); //can i really use just one?
println
    radianceCache.move();
println
    originBlockPalette.move();
println
    // distancePalette.move();
println
    // bitPalette.move(); //bitmask of originBlockPalette
println
    materialPalette.move();
println
    lightUniform.move();
println
    uniform.move();
println
    aoLutUniform.move();
println
    gpuRadianceUpdates.move();
println
    stagingRadianceUpdates.move();
println
    gpuParticles.move(); //multiple because cpu-related work
println
    perlinNoise2d.move(); //full-world grass shift (~direction) texture sampled in grass
println
    grassState.move(); //full-world grass shift (~direction) texture sampled in grass
println
    waterState.move(); //~same but water
println
    perlinNoise3d.move(); //4 channels of different tileable noise for volumetrics
println

    // stagingWorldMapped.move();
    // stagingRadianceUpdatesMapped.move();
    // gpuParticlesMapped.move(); //multiple because cpu-related work
    // double timestampPeriod = physicalDeviceProperties.limits.timestampPeriod;
    // timeTakenByRadiance = float (timestamps[1] - timestamps[0]) * timestampPeriod / 1000000.0f;

    // #ifdef MEASURE_PERFOMANCE
    // #endif
    // printl(timeTakenByRadiance)
println
}

tuple<int, int> get_block_xy (int N) {
    int x = N % BLOCK_PALETTE_SIZE_X;
    int y = N / BLOCK_PALETTE_SIZE_X;
    assert (y <= BLOCK_PALETTE_SIZE_Y);
    return tuple (x, y);
}