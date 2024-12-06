#ifdef __EMSCRIPTEN__
#error Lum does not compile for web
#endif

#include "internal_render.hpp"
#define VMA_IMPLEMENTATION
#define VMA_STATIC_VULKAN_FUNCTIONS 0
#include "defines/macros.hpp"

#include <fstream>

#include "ao_lut.hpp"
#include "../../engine/parallel/parallel.hpp"

#include <glm/gtx/quaternion.hpp>
#include <glm/gtx/hash.hpp>

using std::vector;

using glm::u8, glm::u16, glm::u16, glm::u32;
using glm::i8, glm::i16, glm::i32;
using glm::f32, glm::f64;
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
using glm::identity;
using glm::intBitsToFloat;

#define PLACE_TIMESTAMP_OUTSIDE(commandBuffer) do {\
    if(lumal.settings.profile){\
        lumal.timestampNames[lumal.currentTimestamp] = __func__;\
        vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, lumal.queryPoolTimestamps.current(), lumal.currentTimestamp++);\
    }\
} while(0)

ivec2 get_block_xy (int N);

vector<char> readFile (const char* filename);

void LumInternal::LumInternalRenderer::init (LumSettings settings) {
    world_size = settings.world_size;
    static_block_palette_size = settings.static_block_palette_size;
    maxParticleCount = settings.maxParticleCount;
TRACE();

    lumal.init(settings);
TRACE();

    origin_world.allocate (world_size);
    current_world.allocate (world_size);
    lightmapExtent = {1024, 1024};
    // lightmapExtent = {768,768};
    //to fix weird mesh culling problems for identity
    // camera.updateCamera();
    
    ui_render_interface = new(MyRenderInterface);
    ui_render_interface->render = this;
    

    DEPTH_FORMAT = lumal.findSupportedFormat ({DEPTH_FORMAT_PREFERED, DEPTH_FORMAT_SPARE}, VK_IMAGE_TYPE_2D, VK_IMAGE_TILING_OPTIMAL,
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

    lumal.mainCommandBuffers = &computeCommandBuffers;
    lumal.extraCommandBuffers = &copyCommandBuffers;
    lumal.createSwapchainDependent = [this]() -> VkResult {return LumInternal::LumInternalRenderer::createSwapchainDependent();};
    lumal.cleanupSwapchainDependent = [this]() -> VkResult {return LumInternal::LumInternalRenderer::cleanupSwapchainDependent();};

TRACE();
    gen_perlin_2d();
TRACE();
    gen_perlin_3d();
TRACE();
}

void LumInternal::LumInternalRenderer::createImages() {
    lumal.createBufferStorages (&stagingWorld,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        world_size.x* world_size.y* world_size.z* sizeof (BlockID_t), true);
    lumal.createImageStorages (&world,
        VK_IMAGE_TYPE_3D,
        VK_FORMAT_R16_SINT,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT,
        Lumal::extent3d(world_size)); //TODO: dynamic
    lumal.createImageStorages (&lightmap,
        VK_IMAGE_TYPE_2D,
        LIGHTMAPS_FORMAT,
        VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        VK_IMAGE_ASPECT_DEPTH_BIT,
    {lightmapExtent.width, lightmapExtent.height, 1});
TRACE();
    lumal.createImageStorages (&radianceCache,
        VK_IMAGE_TYPE_3D,
        RADIANCE_FORMAT,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT,
    Lumal::extent3d(world_size)); //TODO: dynamic
TRACE();
    lumal.createImageStorages (&originBlockPalette,
        VK_IMAGE_TYPE_3D,
        VK_FORMAT_R8_UINT,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT,
        // {16*BLOCK_PALETTE_SIZE_X, 16*BLOCK_PALETTE_SIZE_Y, 32}); //TODO: dynamic
    {16 * BLOCK_PALETTE_SIZE_X, 16 * BLOCK_PALETTE_SIZE_Y, 16}); //TODO: dynamic
    lumal.createImageStorages (&materialPalette,
        VK_IMAGE_TYPE_2D,
        VK_FORMAT_R32_SFLOAT, //try R32G32
        VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT,
    {6, 256, 1}); //TODO: dynamic, text formats, pack Material into smth other than 6 floats :)
    lumal.createImageStorages (&grassState,
        VK_IMAGE_TYPE_2D,
        VK_FORMAT_R16G16_SFLOAT,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        0,
        VK_IMAGE_ASPECT_COLOR_BIT,
    {world_size.x * 2, world_size.y * 2, 1}); //for quality
    lumal.createImageStorages (&waterState,
        VK_IMAGE_TYPE_2D,
        VK_FORMAT_R16G16B16A16_SFLOAT,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        0,
        VK_IMAGE_ASPECT_COLOR_BIT,
    {world_size.x * 2, world_size.y * 2, 1}); //for quality
    lumal.createImageStorages (&perlinNoise2d,
        VK_IMAGE_TYPE_2D,
        VK_FORMAT_R16G16_SNORM,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        0,
        VK_IMAGE_ASPECT_COLOR_BIT,
    {world_size.x, world_size.y, 1}); //does not matter than much
    lumal.createImageStorages (&perlinNoise3d,
        VK_IMAGE_TYPE_3D,
        VK_FORMAT_R16G16B16A16_UNORM,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        0,
        VK_IMAGE_ASPECT_COLOR_BIT,
    {32, 32, 32}); //does not matter than much
    lumal.createBufferStorages (&gpuParticles,
        VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
        maxParticleCount* sizeof (Particle), true);
    lumal.createBufferStorages (&uniform,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        220, false); //no way i write it with sizeof's
    lumal.createBufferStorages (&lightUniform,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        sizeof (mat4), false);
    lumal.createBufferStorages (&aoLutUniform,
        VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        sizeof (AoLut) * 8, false); //TODO DYNAMIC AO SAMPLE COUNT
    lumal.createBufferStorages (&gpuRadianceUpdates,
        VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT,
        sizeof (i8vec4)*world_size.x* world_size.y* world_size.z, false); //TODO test extra mem
    lumal.createBufferStorages (&stagingRadianceUpdates,
        VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
        sizeof (ivec4)*world_size.x* world_size.y* world_size.z, true); //TODO test extra mem
        
    lumal.mapBufferStorages (&gpuParticles);
    lumal.mapBufferStorages (&stagingWorld);
    lumal.mapBufferStorages (&stagingRadianceUpdates);
TRACE();
}

void LumInternal::LumInternalRenderer::setupFoliageDescriptors() {
    for (auto* fol : registered_foliage){
        lumal.deferDescriptorSetup (&fol->pipe.setLayout, &fol->pipe.sets, {
            {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, Lumal::RD_CURRENT, (&uniform), {/*empty*/}, NO_SAMPLER, NO_LAYOUT},
            {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, Lumal::RD_FIRST, {}, &grassState, linearSampler, VK_IMAGE_LAYOUT_GENERAL},
        }, VK_SHADER_STAGE_VERTEX_BIT);
TRACE();
    }
}

void LumInternal::LumInternalRenderer::setupDescriptors() {
    // called inside cause dependent
    setupFoliageDescriptors();
    
    lumal.createDescriptorSetLayout ({
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT},//in texture (ui)
    }, &overlayPipe.setLayout,
    VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR);
    lumal.deferDescriptorSetup (&lightmapBlocksPipe.setLayout, &lightmapBlocksPipe.sets, {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, Lumal::RD_CURRENT, (&lightUniform), {/*empty*/}, NO_SAMPLER, NO_LAYOUT},
    }, VK_SHADER_STAGE_VERTEX_BIT);
    lumal.deferDescriptorSetup (&lightmapModelsPipe.setLayout, &lightmapModelsPipe.sets, {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, Lumal::RD_CURRENT, (&lightUniform), {/*empty*/}, NO_SAMPLER, NO_LAYOUT},
    }, VK_SHADER_STAGE_VERTEX_BIT);
TRACE();
    lumal.deferDescriptorSetup (&radiancePipe.setLayout, &radiancePipe.sets, {
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, Lumal::RD_FIRST, {/*empty*/}, &world, unnormNearest, VK_IMAGE_LAYOUT_GENERAL},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, Lumal::RD_CURRENT, {/*empty*/}, (&originBlockPalette), unnormNearest, VK_IMAGE_LAYOUT_GENERAL},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, Lumal::RD_CURRENT, {/*empty*/}, (&materialPalette), nearestSampler, VK_IMAGE_LAYOUT_GENERAL},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, Lumal::RD_FIRST, {/*empty*/}, &radianceCache, unnormLinear, VK_IMAGE_LAYOUT_GENERAL},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, Lumal::RD_FIRST, {/*empty*/}, &radianceCache, NO_SAMPLER, VK_IMAGE_LAYOUT_GENERAL},
        {VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, Lumal::RD_FIRST, &gpuRadianceUpdates, {}, NO_SAMPLER, NO_LAYOUT},
    }, VK_SHADER_STAGE_COMPUTE_BIT);
TRACE();
    lumal.deferDescriptorSetup (&diffusePipe.setLayout, &diffusePipe.sets, {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, Lumal::RD_CURRENT, (&uniform), {/*empty*/}, NO_SAMPLER, NO_LAYOUT, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT},
        {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, Lumal::RD_FIRST, {/*empty*/}, &highresMatNorm, NO_SAMPLER, VK_IMAGE_LAYOUT_GENERAL},
        {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, Lumal::RD_FIRST, {/*empty*/}, &highresDepthStencil, NO_SAMPLER, VK_IMAGE_LAYOUT_GENERAL},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, Lumal::RD_FIRST, {/*empty*/}, &materialPalette, nearestSampler, VK_IMAGE_LAYOUT_GENERAL},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, Lumal::RD_FIRST, {/*empty*/}, &radianceCache, unnormLinear, VK_IMAGE_LAYOUT_GENERAL},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, Lumal::RD_FIRST, {/*empty*/}, &lightmap, shadowSampler, VK_IMAGE_LAYOUT_GENERAL},
        // {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, RD_FIRST, {/*empty*/}, {lightmap}, linearSampler, VK_IMAGE_LAYOUT_GENERAL},
    }, VK_SHADER_STAGE_FRAGMENT_BIT);
TRACE();
    lumal.deferDescriptorSetup (&aoPipe.setLayout, &aoPipe.sets, {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, Lumal::RD_CURRENT, (&uniform), {/*empty*/}, NO_SAMPLER, NO_LAYOUT},
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, Lumal::RD_CURRENT, (&aoLutUniform), {/*empty*/}, NO_SAMPLER, NO_LAYOUT},
        {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, Lumal::RD_FIRST, {/*empty*/}, &highresMatNorm, NO_SAMPLER, VK_IMAGE_LAYOUT_GENERAL},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, Lumal::RD_FIRST, {/*empty*/}, &highresDepthStencil, nearestSampler, VK_IMAGE_LAYOUT_GENERAL},
    }, VK_SHADER_STAGE_FRAGMENT_BIT);
TRACE();
    lumal.deferDescriptorSetup (&tonemapPipe.setLayout, &tonemapPipe.sets, {
        {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, Lumal::RD_FIRST, {/*empty*/}, &highresFrame, NO_SAMPLER, VK_IMAGE_LAYOUT_GENERAL},
    }, VK_SHADER_STAGE_FRAGMENT_BIT);
TRACE();
    lumal.deferDescriptorSetup (&fillStencilGlossyPipe.setLayout, &fillStencilGlossyPipe.sets, {
        {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, Lumal::RD_FIRST, {/*empty*/}, &highresMatNorm, NO_SAMPLER, VK_IMAGE_LAYOUT_GENERAL},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, Lumal::RD_CURRENT, {/*empty*/}, (&materialPalette), nearestSampler, VK_IMAGE_LAYOUT_GENERAL},
    }, VK_SHADER_STAGE_FRAGMENT_BIT);
TRACE();
    lumal.deferDescriptorSetup (&fillStencilSmokePipe.setLayout, &fillStencilSmokePipe.sets, {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, Lumal::RD_CURRENT, (&uniform), {/*empty*/}, NO_SAMPLER, NO_LAYOUT},
    }, VK_SHADER_STAGE_VERTEX_BIT);
TRACE();
    lumal.deferDescriptorSetup (&glossyPipe.setLayout, &glossyPipe.sets, {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, Lumal::RD_CURRENT, (&uniform), {/*empty*/}, NO_SAMPLER, NO_LAYOUT},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, Lumal::RD_FIRST, {/*empty*/}, &highresMatNorm, nearestSampler, VK_IMAGE_LAYOUT_GENERAL},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, Lumal::RD_FIRST, {/*empty*/}, &highresDepthStencil, nearestSampler, VK_IMAGE_LAYOUT_GENERAL},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, Lumal::RD_FIRST, {/*empty*/}, &world, unnormNearest, VK_IMAGE_LAYOUT_GENERAL},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, Lumal::RD_CURRENT, {/*empty*/}, (&originBlockPalette), unnormNearest, VK_IMAGE_LAYOUT_GENERAL},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, Lumal::RD_CURRENT, {/*empty*/}, (&materialPalette), nearestSampler, VK_IMAGE_LAYOUT_GENERAL},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, Lumal::RD_FIRST, {/*empty*/}, &radianceCache, unnormLinear, VK_IMAGE_LAYOUT_GENERAL},
    }, VK_SHADER_STAGE_FRAGMENT_BIT);
TRACE();
    lumal.deferDescriptorSetup (&smokePipe.setLayout, &smokePipe.sets, {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, Lumal::RD_CURRENT, (&uniform), {/*empty*/}, NO_SAMPLER, NO_LAYOUT},
        {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, Lumal::RD_FIRST, {/*empty*/}, &farDepth, NO_SAMPLER, VK_IMAGE_LAYOUT_GENERAL},
        {VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, Lumal::RD_FIRST, {/*empty*/}, &nearDepth, NO_SAMPLER, VK_IMAGE_LAYOUT_GENERAL},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, Lumal::RD_FIRST, {/*empty*/}, &radianceCache, NO_SAMPLER, VK_IMAGE_LAYOUT_GENERAL},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, Lumal::RD_FIRST, {/*empty*/}, &perlinNoise3d, linearSampler_tiled, VK_IMAGE_LAYOUT_GENERAL},
    }, VK_SHADER_STAGE_FRAGMENT_BIT);
TRACE();
    // create_DescriptorSetLayout({
    // VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, // per-quad attributes
    // },
    // VK_SHADER_STAGE_VERTEX_BIT, &blocksPushLayout,
    // VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR);
    lumal.deferDescriptorSetup (&raygenBlocksPipe.setLayout, &raygenBlocksPipe.sets, {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, Lumal::RD_CURRENT, (&uniform), {/*empty*/}, NO_SAMPLER, NO_LAYOUT},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, Lumal::RD_CURRENT, {}, &originBlockPalette, unnormNearest, VK_IMAGE_LAYOUT_GENERAL},
    }, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    // setup_RayGen_Particles_Descriptors();
    lumal.createDescriptorSetLayout ({
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, VK_SHADER_STAGE_FRAGMENT_BIT} // model voxels
    }, &raygenModelsPushLayout,
    VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR);
    // 0);
    lumal.deferDescriptorSetup (&raygenModelsPipe.setLayout, &raygenModelsPipe.sets, {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, Lumal::RD_CURRENT, (&uniform), {/*empty*/}, NO_SAMPLER, NO_LAYOUT},
    }, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
TRACE();
    lumal.deferDescriptorSetup (&raygenParticlesPipe.setLayout, &raygenParticlesPipe.sets, {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, Lumal::RD_CURRENT, (&uniform), {/*empty*/}, NO_SAMPLER, NO_LAYOUT},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, Lumal::RD_FIRST, {/*empty*/}, &world, NO_SAMPLER, VK_IMAGE_LAYOUT_GENERAL},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, Lumal::RD_CURRENT, {/*empty*/}, (&originBlockPalette), NO_SAMPLER, VK_IMAGE_LAYOUT_GENERAL},
    }, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_GEOMETRY_BIT);
    lumal.deferDescriptorSetup (&updateGrassPipe.setLayout, &updateGrassPipe.sets, {
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, Lumal::RD_FIRST, {}, &grassState, NO_SAMPLER, VK_IMAGE_LAYOUT_GENERAL},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, Lumal::RD_FIRST, {}, &perlinNoise2d, linearSampler_tiled, VK_IMAGE_LAYOUT_GENERAL},
    }, VK_SHADER_STAGE_COMPUTE_BIT);
    lumal.deferDescriptorSetup (&updateWaterPipe.setLayout, &updateWaterPipe.sets, {
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, Lumal::RD_FIRST, {}, &waterState, NO_SAMPLER, VK_IMAGE_LAYOUT_GENERAL},
    }, VK_SHADER_STAGE_COMPUTE_BIT);
    // dynamic now
    // render.deferDescriptorSetup (&raygenGrassPipe.setLayout, &raygenGrassPipe.sets, {
    //     {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, Lumal::RD_CURRENT, (&uniform), {/*empty*/}, NO_SAMPLER, NO_LAYOUT},
    //     {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, Lumal::RD_FIRST, {}, &grassState, linearSampler, VK_IMAGE_LAYOUT_GENERAL},
    // }, VK_SHADER_STAGE_VERTEX_BIT);
    lumal.deferDescriptorSetup (&raygenWaterPipe.setLayout, &raygenWaterPipe.sets, {
        {VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, Lumal::RD_CURRENT, (&uniform), {/*empty*/}, NO_SAMPLER, NO_LAYOUT},
        {VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, Lumal::RD_FIRST, {}, &waterState, linearSampler_tiled, VK_IMAGE_LAYOUT_GENERAL},
    }, VK_SHADER_STAGE_VERTEX_BIT);
    lumal.deferDescriptorSetup (&genPerlin2dPipe.setLayout, &genPerlin2dPipe.sets, {
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, Lumal::RD_FIRST, {}, &perlinNoise2d, NO_SAMPLER, VK_IMAGE_LAYOUT_GENERAL},
    }, VK_SHADER_STAGE_COMPUTE_BIT);
    lumal.deferDescriptorSetup (&genPerlin3dPipe.setLayout, &genPerlin3dPipe.sets, {
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, Lumal::RD_FIRST, {}, &perlinNoise3d, NO_SAMPLER, VK_IMAGE_LAYOUT_GENERAL},
    }, VK_SHADER_STAGE_COMPUTE_BIT);
    lumal.createDescriptorSetLayout ({
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE,VK_SHADER_STAGE_COMPUTE_BIT} // model voxels
    }, &mapPushLayout,
    VK_DESCRIPTOR_SET_LAYOUT_CREATE_PUSH_DESCRIPTOR_BIT_KHR);
    // 0);
    lumal.deferDescriptorSetup (&mapPipe.setLayout, &mapPipe.sets, {
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, Lumal::RD_FIRST, {}, &world, NO_SAMPLER, VK_IMAGE_LAYOUT_GENERAL},
        {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, Lumal::RD_CURRENT, {}, &originBlockPalette, NO_SAMPLER, VK_IMAGE_LAYOUT_GENERAL},
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
TRACE();
    lumal.flushDescriptorSetup();
TRACE();
}

// try to find shader in multiple places just in case if lum is launched incorrectly
std::string find_shader(const std::string& shader_name) {
    // unity places all assets into Assets/
    std::string paths[] = {
        shader_name,
        "../" + shader_name,
        "shaders/compiled/" + shader_name,
        "../shaders/compiled/" + shader_name,
        // for unity
        "Assets/shaders/compiled/" + shader_name,
        "Assets/Shaders/compiled/" + shader_name,
    };

    for (const auto& path : paths) {
        // LOG(path)
        std::ifstream file(path);
        if (file.good()) {
            return path;
        }
    }

    std::cout << "Shader file not found: " << shader_name << std::endl;
    crash();
    // std::unreachable();
}

void LumInternal::LumInternalRenderer::createFoliagePipilines() {
    for (auto* fol : registered_foliage){
        // we need to create fol->pipe
        // TODO: automatic subpassId?
        fol->pipe.subpassId = 3;
        lumal.createRasterPipeline (&fol->pipe, 0, {
            {find_shader(fol->vertex_shader_file).c_str(), VK_SHADER_STAGE_VERTEX_BIT},
            {find_shader("grass.frag.spv").c_str(), VK_SHADER_STAGE_FRAGMENT_BIT},
        }, {/*empty*/},
        0, VK_VERTEX_INPUT_RATE_VERTEX, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
        lumal.swapChainExtent, {Lumal::NO_BLEND}, sizeof (vec4) + sizeof (int) * 2 + sizeof (int) * 2, Lumal::DEPTH_TEST_READ_BIT|Lumal::DEPTH_TEST_WRITE_BIT, VK_COMPARE_OP_LESS, VK_CULL_MODE_NONE, Lumal::NO_DISCARD, Lumal::NO_STENCIL);
TRACE();
    }
}

void LumInternal::LumInternalRenderer::createPipilines() {
    createFoliagePipilines();
    //that is why NOT abstracting vulkan is also an option
    //if you cannot guess what things mean by just looking at them maybe read old (0.0.3) release src
TRACE();
    lightmapBlocksPipe.subpassId = 0;
    lumal.createRasterPipeline (&lightmapBlocksPipe, 0, {
        {find_shader("lightmapBlocks.vert.spv").c_str(), VK_SHADER_STAGE_VERTEX_BIT},
        //doesnt need fragment
    }, {
        {VK_FORMAT_R8G8B8_UINT, offsetof (PackedVoxelCircuit, pos)},
    },
    sizeof (PackedVoxelCircuit), VK_VERTEX_INPUT_RATE_VERTEX, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    lightmapExtent, {Lumal::NO_BLEND}, (sizeof (i16vec4)), Lumal::DEPTH_TEST_READ_BIT|Lumal::DEPTH_TEST_WRITE_BIT, VK_COMPARE_OP_LESS, VK_CULL_MODE_NONE, Lumal::NO_DISCARD, Lumal::NO_STENCIL);
    lightmapModelsPipe.subpassId = 0;
    lumal.createRasterPipeline (&lightmapModelsPipe, 0, {
        {find_shader("lightmapModels.vert.spv").c_str(), VK_SHADER_STAGE_VERTEX_BIT},
        //doesnt need fragment
    }, {
        {VK_FORMAT_R8G8B8_UINT, offsetof (PackedVoxelCircuit, pos)},
    },
    sizeof (PackedVoxelCircuit), VK_VERTEX_INPUT_RATE_VERTEX, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    lightmapExtent, {Lumal::NO_BLEND}, (sizeof (quat) + sizeof (vec4)), Lumal::DEPTH_TEST_READ_BIT|Lumal::DEPTH_TEST_WRITE_BIT, VK_COMPARE_OP_LESS, VK_CULL_MODE_NONE, Lumal::NO_DISCARD, Lumal::NO_STENCIL);
    raygenBlocksPipe.subpassId = 0;
    lumal.createRasterPipeline (&raygenBlocksPipe, 0, {
        {find_shader("rayGenBlocks.vert.spv").c_str(), VK_SHADER_STAGE_VERTEX_BIT},
        {find_shader("rayGenBlocks.frag.spv").c_str(), VK_SHADER_STAGE_FRAGMENT_BIT},
    }, {
        {VK_FORMAT_R8G8B8_UINT, offsetof (PackedVoxelCircuit, pos)},
        // {VK_FORMAT_R8G8B8_SINT, offsetof(VoxelVertex, norm)},
        // {VK_FORMAT_R8_UINT, offsetof(PackedVoxelVertex, matID)},
    },
    sizeof (PackedVoxelCircuit), VK_VERTEX_INPUT_RATE_VERTEX, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    lumal.swapChainExtent, {Lumal::NO_BLEND}, (12), Lumal::DEPTH_TEST_READ_BIT|Lumal::DEPTH_TEST_WRITE_BIT, VK_COMPARE_OP_LESS, VK_CULL_MODE_NONE, Lumal::NO_DISCARD, Lumal::NO_STENCIL);
TRACE();
    raygenModelsPipe.subpassId = 1;
    lumal.createRasterPipeline (&raygenModelsPipe, raygenModelsPushLayout, {
        {find_shader("rayGenModels.vert.spv").c_str(), VK_SHADER_STAGE_VERTEX_BIT},
        {find_shader("rayGenModels.frag.spv").c_str(), VK_SHADER_STAGE_FRAGMENT_BIT},
    }, {
        {VK_FORMAT_R8G8B8_UINT, offsetof (PackedVoxelCircuit, pos)},
    },
    sizeof (PackedVoxelCircuit), VK_VERTEX_INPUT_RATE_VERTEX, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    lumal.swapChainExtent, {Lumal::NO_BLEND}, (sizeof (vec4) * 3), Lumal::DEPTH_TEST_READ_BIT|Lumal::DEPTH_TEST_WRITE_BIT, VK_COMPARE_OP_LESS, VK_CULL_MODE_NONE, Lumal::NO_DISCARD, Lumal::NO_STENCIL);
TRACE();
    raygenParticlesPipe.subpassId = 2;
    lumal.createRasterPipeline (&raygenParticlesPipe, 0, {
        {find_shader("rayGenParticles.vert.spv").c_str(), VK_SHADER_STAGE_VERTEX_BIT},
        {find_shader("rayGenParticles.geom.spv").c_str(), VK_SHADER_STAGE_GEOMETRY_BIT},
        {find_shader("rayGenParticles.frag.spv").c_str(), VK_SHADER_STAGE_FRAGMENT_BIT},
    }, {
        {VK_FORMAT_R32G32B32_SFLOAT, offsetof (Particle, pos)},
        {VK_FORMAT_R32G32B32_SFLOAT, offsetof (Particle, vel)},
        {VK_FORMAT_R32_SFLOAT, offsetof (Particle, lifeTime)},
        {VK_FORMAT_R8_UINT, offsetof (Particle, matID)},
    },
    sizeof (Particle), VK_VERTEX_INPUT_RATE_VERTEX, VK_PRIMITIVE_TOPOLOGY_POINT_LIST,
    lumal.swapChainExtent, {Lumal::NO_BLEND}, 0, Lumal::DEPTH_TEST_READ_BIT|Lumal::DEPTH_TEST_WRITE_BIT, VK_COMPARE_OP_LESS, VK_CULL_MODE_NONE, Lumal::NO_DISCARD, Lumal::NO_STENCIL);
TRACE();
//     raygenGrassPipe.subpassId = 3;
//     render.createRasterPipeline (&raygenGrassPipe, 0, {
//         {find_shader("grass.vert.spv").c_str(), VK_SHADER_STAGE_VERTEX_BIT},
//         {find_shader("grass.frag.spv").c_str(), VK_SHADER_STAGE_FRAGMENT_BIT},
//     }, {/*empty*/},
//     0, VK_VERTEX_INPUT_RATE_VERTEX, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
//     render.swapChainExtent, {Lumal::NO_BLEND}, sizeof (vec4) + sizeof (int) * 2 + sizeof (int) * 2, Lumal::DEPTH_TEST_READ_BIT|Lumal::DEPTH_TEST_WRITE_BIT, VK_COMPARE_OP_LESS, VK_CULL_MODE_NONE, Lumal::NO_DISCARD, Lumal::NO_STENCIL);
// TRACE();
    raygenWaterPipe.subpassId = 4;
    lumal.createRasterPipeline (&raygenWaterPipe, 0, {
        {find_shader("water.vert.spv").c_str(), VK_SHADER_STAGE_VERTEX_BIT},
        {find_shader("water.frag.spv").c_str(), VK_SHADER_STAGE_FRAGMENT_BIT},
    }, {/*empty*/},
    0, VK_VERTEX_INPUT_RATE_VERTEX, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP,
    lumal.swapChainExtent, {Lumal::NO_BLEND}, sizeof (vec4) + sizeof (int) * 2, Lumal::DEPTH_TEST_READ_BIT|Lumal::DEPTH_TEST_WRITE_BIT, VK_COMPARE_OP_LESS, VK_CULL_MODE_NONE, Lumal::NO_DISCARD, Lumal::NO_STENCIL);
TRACE();
    diffusePipe.subpassId = 0;
    lumal.createRasterPipeline (&diffusePipe, 0, {
        {find_shader("fullscreenTriag.vert.spv").c_str(), VK_SHADER_STAGE_VERTEX_BIT},
        {find_shader("diffuse.frag.spv").c_str(), VK_SHADER_STAGE_FRAGMENT_BIT},
    }, {/*fullscreen pass*/},
    0, VK_VERTEX_INPUT_RATE_VERTEX, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    lumal.swapChainExtent, {Lumal::NO_BLEND}, sizeof (ivec4) + sizeof (vec4) * 4 + sizeof (mat4), Lumal::DEPTH_TEST_NONE_BIT, VK_COMPARE_OP_LESS, VK_CULL_MODE_NONE, Lumal::NO_DISCARD, Lumal::NO_STENCIL);
    aoPipe.subpassId = 1;
    lumal.createRasterPipeline (&aoPipe, 0, {
        {find_shader("fullscreenTriag.vert.spv").c_str(), VK_SHADER_STAGE_VERTEX_BIT},
        {find_shader("hbao.frag.spv").c_str(), VK_SHADER_STAGE_FRAGMENT_BIT},
    }, {/*fullscreen pass*/},
    0, VK_VERTEX_INPUT_RATE_VERTEX, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    lumal.swapChainExtent, {Lumal::BLEND_MIX}, 0, Lumal::DEPTH_TEST_NONE_BIT, VK_COMPARE_OP_LESS, VK_CULL_MODE_NONE, Lumal::NO_DISCARD, Lumal::NO_STENCIL);
TRACE();
    fillStencilGlossyPipe.subpassId = 2;
    lumal.createRasterPipeline (&fillStencilGlossyPipe, 0, {
        {find_shader("fullscreenTriag.vert.spv").c_str(), VK_SHADER_STAGE_VERTEX_BIT},
        {find_shader("fillStencilGlossy.frag.spv").c_str(), VK_SHADER_STAGE_FRAGMENT_BIT},
    }, {/*fullscreen pass*/},
    0, VK_VERTEX_INPUT_RATE_VERTEX, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    lumal.swapChainExtent, {Lumal::NO_BLEND}, 0, Lumal::DEPTH_TEST_NONE_BIT, VK_COMPARE_OP_LESS, VK_CULL_MODE_NONE, Lumal::NO_DISCARD, {
        //sets 01 bit on mat.rough < 0.5 or smth similar
        .failOp = VK_STENCIL_OP_REPLACE,
        .passOp = VK_STENCIL_OP_REPLACE,
        .depthFailOp = VK_STENCIL_OP_REPLACE,
        .compareOp = VK_COMPARE_OP_ALWAYS,
        .compareMask = 0b00,
        .writeMask = 0b01, //01 for reflection
        .reference = 0b01,
    });
TRACE();
    fillStencilSmokePipe.subpassId = 3;
    lumal.createRasterPipeline (&fillStencilSmokePipe, 0, {
        {find_shader("fillStencilSmoke.vert.spv").c_str(), VK_SHADER_STAGE_VERTEX_BIT},
        {find_shader("fillStencilSmoke.frag.spv").c_str(), VK_SHADER_STAGE_FRAGMENT_BIT},
    }, {/*passed as push constants lol*/},
    0, VK_VERTEX_INPUT_RATE_VERTEX, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    lumal.swapChainExtent, {Lumal::BLEND_REPLACE_IF_GREATER, Lumal::BLEND_REPLACE_IF_LESS}, sizeof (vec3) + sizeof (int) + sizeof (vec4), Lumal::DEPTH_TEST_READ_BIT, VK_COMPARE_OP_LESS, VK_CULL_MODE_NONE, Lumal::NO_DISCARD, {
        //sets 10 bit on rasterization
        .failOp = VK_STENCIL_OP_KEEP,
        .passOp = VK_STENCIL_OP_REPLACE,
        .depthFailOp = VK_STENCIL_OP_KEEP,
        .compareOp = VK_COMPARE_OP_ALWAYS,
        .compareMask = 0b00,
        .writeMask = 0b10, //10 for smoke
        .reference = 0b10,
    });
TRACE();
    glossyPipe.subpassId = 4;
    lumal.createRasterPipeline (&glossyPipe, 0, {
        {find_shader("fullscreenTriag.vert.spv").c_str(), VK_SHADER_STAGE_VERTEX_BIT},
        {find_shader("glossy.frag.spv").c_str(), VK_SHADER_STAGE_FRAGMENT_BIT},
    }, {/*fullscreen pass*/},
    0, VK_VERTEX_INPUT_RATE_VERTEX, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    lumal.swapChainExtent, {Lumal::BLEND_MIX}, sizeof (vec4) + sizeof (vec4), Lumal::DEPTH_TEST_NONE_BIT, VK_COMPARE_OP_LESS, VK_CULL_MODE_NONE, Lumal::NO_DISCARD, {
        .failOp = VK_STENCIL_OP_KEEP,
        .passOp = VK_STENCIL_OP_KEEP,
        .depthFailOp = VK_STENCIL_OP_KEEP,
        .compareOp = VK_COMPARE_OP_EQUAL,
        // .compareOp = VK_COMPARE_OP_ALWAYS,
        .compareMask = 0b01,
        .writeMask = 0b00, //01 for glossy
        .reference = 0b01,
    });
TRACE();
    smokePipe.subpassId = 5;
    lumal.createRasterPipeline (&smokePipe, 0, {
        {find_shader("fullscreenTriag.vert.spv").c_str(), VK_SHADER_STAGE_VERTEX_BIT},
        {find_shader("smoke.frag.spv").c_str(), VK_SHADER_STAGE_FRAGMENT_BIT},
    }, {/*fullscreen pass*/},
    0, VK_VERTEX_INPUT_RATE_VERTEX, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    lumal.swapChainExtent, {Lumal::BLEND_MIX}, 0, Lumal::DEPTH_TEST_NONE_BIT, VK_COMPARE_OP_LESS, VK_CULL_MODE_NONE, Lumal::NO_DISCARD, {
        //sets 10 bit on rasterization
        .failOp = VK_STENCIL_OP_KEEP,
        .passOp = VK_STENCIL_OP_KEEP,
        .depthFailOp = VK_STENCIL_OP_KEEP,
        .compareOp = VK_COMPARE_OP_EQUAL,
        .compareMask = 0b10,
        .writeMask = 0b00, //10 for smoke
        .reference = 0b10,
    });
TRACE();
    tonemapPipe.subpassId = 6;
    lumal.createRasterPipeline (&tonemapPipe, 0, {
        {find_shader("fullscreenTriag.vert.spv").c_str(), VK_SHADER_STAGE_VERTEX_BIT},
        {find_shader("tonemap.frag.spv").c_str(), VK_SHADER_STAGE_FRAGMENT_BIT},
    }, {/*fullscreen pass*/},
    0, VK_VERTEX_INPUT_RATE_VERTEX, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    lumal.swapChainExtent, {Lumal::NO_BLEND}, 0, Lumal::DEPTH_TEST_NONE_BIT, VK_COMPARE_OP_LESS, VK_CULL_MODE_NONE, Lumal::NO_DISCARD, Lumal::NO_STENCIL);
TRACE();
    overlayPipe.subpassId = 7;
    lumal.createRasterPipeline (&overlayPipe, 0, {
        {find_shader("overlay.vert.spv").c_str(), VK_SHADER_STAGE_VERTEX_BIT},
        {find_shader("overlay.frag.spv").c_str(), VK_SHADER_STAGE_FRAGMENT_BIT},
    }, {
        {VK_FORMAT_R32G32_SFLOAT, offsetof (Rml::Vertex, position)},
        {VK_FORMAT_R8G8B8A8_UNORM, offsetof (Rml::Vertex, colour)},
        {VK_FORMAT_R32G32_SFLOAT, offsetof (Rml::Vertex, tex_coord)},
    },
    sizeof (Rml::Vertex), VK_VERTEX_INPUT_RATE_VERTEX, VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST,
    lumal.swapChainExtent, {Lumal::BLEND_MIX}, sizeof (vec4) + sizeof (mat4), Lumal::DEPTH_TEST_NONE_BIT, VK_COMPARE_OP_LESS, VK_CULL_MODE_NONE, Lumal::NO_DISCARD, Lumal::NO_STENCIL);
TRACE();
    lumal.createComputePipeline (&radiancePipe, 0, find_shader("radiance.comp.spv").c_str(), sizeof (int) * 4, VK_PIPELINE_CREATE_DISPATCH_BASE_BIT);
TRACE();
    lumal.createComputePipeline (&updateGrassPipe, 0, find_shader("updateGrass.comp.spv").c_str(), sizeof (vec2) * 2 + sizeof (float), 0);
TRACE();
    lumal.createComputePipeline (&updateWaterPipe, 0, find_shader("updateWater.comp.spv").c_str(), sizeof (float) + sizeof (vec2) * 2, 0);
TRACE();
    lumal.createComputePipeline (&genPerlin2dPipe, 0, find_shader("perlin2.comp.spv").c_str(), 0, 0);
TRACE();
    lumal.createComputePipeline (&genPerlin3dPipe, 0, find_shader("perlin3.comp.spv").c_str(), 0, 0);
TRACE();
    // render.createComputePipeline(&dfxPipe,0, find_asset("/dfx.spv", ).c_str()0, 0);
    // render.createComputePipeline(&dfyPipe,0, find_asset("/dfy.spv", ).c_str()0, 0);
    // render.createComputePipeline(&dfzPipe,0, find_asset("/dfz.spv", ).c_str()0, 0);
    // render.createComputePipeline(&bitmaskPipe,0, find_asset("bit.mask.spv").c_str(), 0, 0);
    lumal.createComputePipeline (&mapPipe, mapPushLayout, find_shader("map.comp.spv").c_str(), sizeof (mat4) + sizeof (ivec4), 0);
TRACE();
}

VkResult LumInternal::LumInternalRenderer::createSwapchainDependent() {
    lumal.deviceWaitIdle();

    // vkResetCommandPool(render.device, render.commandPool, VK_COMMAND_POOL_RESET_RELEASE_RESOURCES_BIT);
    vkResetCommandPool(lumal.device, lumal.commandPool, 0);

    lumal.createCommandBuffers(&computeCommandBuffers, lumal.settings.fif);
    lumal.createCommandBuffers(&graphicsCommandBuffers, lumal.settings.fif);
    lumal.createCommandBuffers(&lightmapCommandBuffers, lumal.settings.fif);
    lumal.createCommandBuffers(&copyCommandBuffers, lumal.settings.fif);

    lumal.mainCommandBuffers = &computeCommandBuffers;
    lumal.extraCommandBuffers = &copyCommandBuffers;

    createSwapchainDependentImages();

    vector<Lumal::RasterPipe*> foliage_pipes = {};
    for (auto* fol : registered_foliage) {
        foliage_pipes.push_back(&fol->pipe);
    }
        lumal.createRenderPass({
            {&lightmap, Lumal::Clear, Lumal::Store, Lumal::DontCare, Lumal::DontCare, {.depthStencil = {.depth = 1}}}
        }, {
            {{&lightmapBlocksPipe, &lightmapModelsPipe}, {}, {}, &lightmap}
        }, &lightmapRpass);
TRACE();
    lumal.createRenderPass({
            {&highresMatNorm, Lumal::DontCare, Lumal::Store, Lumal::DontCare, Lumal::DontCare},
            {&highresDepthStencil, Lumal::Clear, Lumal::Store, Lumal::Clear, Lumal::Store, {.depthStencil = {.depth = 1}}},
        }, {
            {{&raygenBlocksPipe}, {}, {&highresMatNorm}, &highresDepthStencil},
            {{&raygenModelsPipe}, {}, {&highresMatNorm}, &highresDepthStencil},
            {{&raygenParticlesPipe}, {}, {&highresMatNorm}, &highresDepthStencil},
            {foliage_pipes, {}, {&highresMatNorm}, &highresDepthStencil},
            {{&raygenWaterPipe}, {}, {&highresMatNorm}, &highresDepthStencil},
        }, &gbufferRpass);
TRACE();
    lumal.createRenderPass({
            {&highresMatNorm,      Lumal::Load    , Lumal::DontCare, Lumal::DontCare, Lumal::DontCare},
            {&highresFrame,        Lumal::DontCare, Lumal::DontCare, Lumal::DontCare, Lumal::DontCare},
            {&lumal.swapchainImages,  Lumal::DontCare, Lumal::Store   , Lumal::DontCare, Lumal::DontCare, {}, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR},
            {&highresDepthStencil, Lumal::Load    , Lumal::DontCare, Lumal::Load    , Lumal::DontCare, {.depthStencil = {.depth = 1}}},
            {&farDepth,           Lumal::Clear   , Lumal::DontCare, Lumal::DontCare, Lumal::DontCare, {.color = {.float32 = {-1000.0,-1000.0,-1000.0,-1000.0}}}},
            {&nearDepth,            Lumal::Clear   , Lumal::DontCare, Lumal::DontCare, Lumal::DontCare, {.color = {.float32 = {+1000.0,+1000.0,+1000.0,+1000.0}}}},
        }, {
            {{&diffusePipe},           {&highresMatNorm, &highresDepthStencil}, {&highresFrame},         NULL},
            {{&aoPipe},                {&highresMatNorm, &highresDepthStencil}, {&highresFrame},         NULL},
            {{&fillStencilGlossyPipe}, {&highresMatNorm},                       {/*empty*/},             &highresDepthStencil},
            {{&fillStencilSmokePipe }, {/*empty*/},                             {&farDepth, &nearDepth}, &highresDepthStencil},
            {{&glossyPipe},            {/*empty*/},                             {&highresFrame},         &highresDepthStencil},
            {{&smokePipe},             {&nearDepth, &farDepth},                 {&highresFrame},         &highresDepthStencil},
            {{&tonemapPipe},           {&highresFrame},                         {&lumal.swapchainImages},   NULL},
            {{&overlayPipe},           {/*empty*/},                             {&lumal.swapchainImages},   NULL},
        }, &shadeRpass);
TRACE();
    setupDescriptors();
TRACE();
    createPipilines();
    
    return VK_SUCCESS;
}

void LumInternal::LumInternalRenderer::createSwapchainDependentImages() {
    // int mipmaps;
TRACE();
    lumal.createImageStorages (&highresMatNorm,
        VK_IMAGE_TYPE_2D,
        MATNORM_FORMAT,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT,
    {lumal.swapChainExtent.width, lumal.swapChainExtent.height, 1});
    lumal.createImageStorages (&highresDepthStencil,
        VK_IMAGE_TYPE_2D,
        DEPTH_FORMAT,
        VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT,
    {lumal.swapChainExtent.width, lumal.swapChainExtent.height, 1});
    lumal.createImageStorages (&highresFrame,
        VK_IMAGE_TYPE_2D,
        FRAME_FORMAT,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT,
    {lumal.swapChainExtent.width, lumal.swapChainExtent.height, 1});
        //set them to the same ptrs
    // lowresMatNorm = highresMatNorm;
    // highresDepthStencil = highresDepthStencil;

    stencilViewForDS.allocate(lumal.settings.fif);
    for(int i=0; i<lumal.settings.fif; i++){
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
        VK_CHECK (vkCreateImageView (lumal.device, &viewInfo, NULL, &stencilViewForDS[i]));
    }
    //required anyways
    lumal.createImageStorages (&farDepth, //smoke depth
        VK_IMAGE_TYPE_2D,
        SECONDARY_DEPTH_FORMAT,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT,
    {lumal.swapChainExtent.width, lumal.swapChainExtent.height, 1});
    lumal.createImageStorages (&nearDepth, //smoke depth
        VK_IMAGE_TYPE_2D,
        SECONDARY_DEPTH_FORMAT,
        VK_IMAGE_USAGE_STORAGE_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_INPUT_ATTACHMENT_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT,
        VMA_MEMORY_USAGE_AUTO_PREFER_DEVICE,
        VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT,
        VK_IMAGE_ASPECT_COLOR_BIT,
    {lumal.swapChainExtent.width, lumal.swapChainExtent.height, 1});
    // render.transitionImageLayoutSingletime(&farDepth[0], VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
}

void LumInternal::LumInternalRenderer::cleanup() {
    delete ui_render_interface;
TRACE();
    lumal.destroyImages (&radianceCache);
    lumal.destroyImages (&world);
    lumal.destroyImages (&originBlockPalette);
    // render.deleteImages(&distancePalette);
    // render.deleteImages(&bitPalette);
    lumal.destroyImages (&materialPalette);
    lumal.destroyImages (&perlinNoise2d);
    lumal.destroyImages (&perlinNoise3d);
    lumal.destroyImages (&grassState);
    lumal.destroyImages (&waterState);
    lumal.destroyImages (&lightmap);
TRACE();
    lumal.destroySampler (nearestSampler);
TRACE();
    lumal.destroySampler (linearSampler);
TRACE();
    lumal.destroySampler (linearSampler_tiled);
TRACE();
    lumal.destroySampler (linearSampler_tiled_mirrored);
TRACE();
    lumal.destroySampler (overlaySampler);
TRACE();
    lumal.destroySampler (shadowSampler);
TRACE();
    lumal.destroySampler (unnormLinear);
TRACE();
    lumal.destroySampler (unnormNearest);
TRACE();
    for (int i = 0; i < lumal.settings.fif; i++) {
        vmaUnmapMemory (lumal.VMAllocator, stagingWorld[i].alloc);
        vmaUnmapMemory (lumal.VMAllocator, gpuParticles[i].alloc);
        vmaUnmapMemory (lumal.VMAllocator, stagingRadianceUpdates[i].alloc);
        vmaDestroyBuffer (lumal.VMAllocator, stagingWorld[i].buffer, stagingWorld[i].alloc);
        vmaDestroyBuffer (lumal.VMAllocator, gpuParticles[i].buffer, gpuParticles[i].alloc);
        vmaDestroyBuffer (lumal.VMAllocator, uniform[i].buffer, uniform[i].alloc);
        vmaDestroyBuffer (lumal.VMAllocator, lightUniform[i].buffer, lightUniform[i].alloc);
        vmaDestroyBuffer (lumal.VMAllocator, aoLutUniform[i].buffer, aoLutUniform[i].alloc);
        vmaDestroyBuffer (lumal.VMAllocator, stagingRadianceUpdates[i].buffer, stagingRadianceUpdates[i].alloc);
        // render.deleteBuffers()
        vmaDestroyBuffer (lumal.VMAllocator, gpuRadianceUpdates[i].buffer, gpuRadianceUpdates[i].alloc);
    }
TRACE();
    cleanupSwapchainDependent();
TRACE();

    lumal.cleanup();
TRACE();
}

VkResult LumInternal::LumInternalRenderer::cleanupSwapchainDependent() {
    lumal.deviceWaitIdle();
    vkResetCommandPool(lumal.device, lumal.commandPool, 0);


    lumal.destroyRenderPass (&lightmapRpass);
    lumal.destroyRenderPass (&gbufferRpass);
    lumal.destroyRenderPass (&shadeRpass);
TRACE();
    lumal.destroyComputePipeline ( &mapPipe);
    vkDestroyDescriptorSetLayout (lumal.device, mapPushLayout, NULL);
    lumal.destroyComputePipeline ( &raytracePipe);
    lumal.destroyComputePipeline ( &radiancePipe);
    lumal.destroyComputePipeline ( &updateGrassPipe);
    lumal.destroyComputePipeline ( &updateWaterPipe);
    lumal.destroyComputePipeline ( &genPerlin2dPipe);
    lumal.destroyComputePipeline ( &genPerlin3dPipe);
    // render.destroyComputePipeline(   &dfxPipe);
    // render.destroyComputePipeline(   &dfyPipe);
    // render.destroyComputePipeline(   &dfzPipe);
    // render.destroyComputePipeline(   &bitmaskPipe);
TRACE();
    for (auto* fol : registered_foliage) {
        lumal.destroyRasterPipeline (&fol->pipe);
TRACE();
    }
TRACE();
    lumal.destroyRasterPipeline (&lightmapBlocksPipe);
    lumal.destroyRasterPipeline (&lightmapModelsPipe);
    vkDestroyDescriptorSetLayout (lumal.device, raygenModelsPushLayout, NULL);
    lumal.destroyRasterPipeline (&raygenBlocksPipe);
    lumal.destroyRasterPipeline (&raygenModelsPipe);
    lumal.destroyRasterPipeline (&raygenParticlesPipe);
    // render.destroyRasterPipeline (&raygenGrassPipe);
    lumal.destroyRasterPipeline (&raygenWaterPipe);
    lumal.destroyRasterPipeline (&diffusePipe);
    lumal.destroyRasterPipeline (&aoPipe);
    lumal.destroyRasterPipeline (&fillStencilSmokePipe);
    lumal.destroyRasterPipeline (&fillStencilGlossyPipe);
    lumal.destroyRasterPipeline (&glossyPipe);
    lumal.destroyRasterPipeline (&smokePipe);
    lumal.destroyRasterPipeline (&tonemapPipe);
    lumal.destroyRasterPipeline (&overlayPipe);
    
TRACE();
    lumal.destroyImages (&highresFrame);
TRACE();
    lumal.destroyImages (&highresDepthStencil);
TRACE();
    for(auto v : stencilViewForDS){
        vkDestroyImageView (lumal.device, v, NULL);
    }
TRACE();
    lumal.destroyImages (&highresMatNorm);
TRACE();
    lumal.destroyImages (&farDepth);
TRACE();
    lumal.destroyImages (&nearDepth);
TRACE();

    return VK_SUCCESS;
}

void LumInternal::LumInternalRenderer::gen_perlin_2d() {
    VkCommandBuffer commandBuffer = lumal.beginSingleTimeCommands();
    // render.cmdBindPipe(commandBuffer, &genPerlin2dPipe);
    vkCmdBindPipeline (commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, genPerlin2dPipe.line);
    for(int i=0; i<lumal.settings.fif; i++){
        vkCmdBindDescriptorSets (commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, genPerlin2dPipe.lineLayout, 0, 1, &genPerlin2dPipe.sets[i], 0, 0);
        lumal.cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
            perlinNoise2d.current());
        vkCmdDispatch (commandBuffer, world_size.x / 8, world_size.y / 8, 1);
        lumal.cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
            perlinNoise2d.current());
    }
    lumal.endSingleTimeCommands (commandBuffer);
}

void LumInternal::LumInternalRenderer::gen_perlin_3d() {
    VkCommandBuffer commandBuffer = lumal.beginSingleTimeCommands();

    vkCmdBindPipeline (commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, genPerlin3dPipe.line);
    for(int i=0; i<lumal.settings.fif; i++){
        vkCmdBindDescriptorSets (commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, genPerlin3dPipe.lineLayout, 0, 1, &genPerlin3dPipe.sets[i], 0, 0);
        lumal.cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
            perlinNoise3d.current());
        vkCmdDispatch (commandBuffer, 64 / 4, 64 / 4, 64 / 4);
        lumal.cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
            VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
            perlinNoise3d.current());
    }
    lumal.endSingleTimeCommands (commandBuffer);
}

void LumInternal::Camera::updateCamera() {
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

void LumInternal::LumInternalRenderer::update_light_transform() {
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
void LumInternal::LumInternalRenderer::start_frame() {
    TRACE();
    camera.updateCamera();
    TRACE();
    update_light_transform();
    TRACE();
    assert(computeCommandBuffers.current());
    assert(graphicsCommandBuffers.current());
    assert(copyCommandBuffers.current());
    assert(lightmapCommandBuffers.current());
    TRACE();
    lumal.start_frame({
        computeCommandBuffers.current(),
        graphicsCommandBuffers.current(),
        copyCommandBuffers.current(),
        lightmapCommandBuffers.current(),
    });
    TRACE();
}

void LumInternal::LumInternalRenderer::start_blockify() {
    VkCommandBuffer& commandBuffer = computeCommandBuffers.current();
    TRACE();
    blockCopyQueue.clear();
    TRACE();
    paletteCounter = static_block_palette_size;
    size_t size_to_copy = world_size.x * world_size.y * world_size.z * sizeof (BlockID_t);
    TRACE();
    memcpy (current_world.data(), origin_world.data(), size_to_copy);
    TRACE();
}

LumInternal::LumInternalRenderer::AABB LumInternal::LumInternalRenderer::get_shift (dmat4 trans, ivec3 size) {
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

// bool LumInternal::LumInternalRenderer::mesh_intersects_aabb(
//     InternalMeshModel* mesh, const mat4& transform, const AABB& block_aabb
// ) {
//     // Transform mesh vertices and check against block AABB
//     for (const vec3& vertex : mesh->vertices) {
//         vec3 transformed_vertex = vec3(transform * vec4(vertex, 1.0f));
//         if (block_aabb.contains(transformed_vertex)) {
//             return true;
//         }
//     }
//     return false;
// }


// allocates temp block in palette for every block that intersects with every mesh blockified
void LumInternal::LumInternalRenderer::blockify_mesh(InternalMeshModel* mesh, MeshTransform* trans) {
    // Compute the transformed AABB for the mesh
    // ATRACE()
    mat4 transform = translate(identity<mat4>(), trans->shift) * toMat4(trans->rot);
    AABB mesh_aabb = get_shift(transform, mesh->size);
    // ATRACE()

    // Calculate voxel grid bounds
    ivec3 min_block = glm::clamp(
        (ivec3(mesh_aabb.min) - 1) / 16,
        ivec3(0),
        ivec3(world_size) - 1
    );
    ivec3 max_block = glm::clamp(
        (ivec3(mesh_aabb.max) + 1) / 16,
        ivec3(0),
        ivec3(world_size) - 1
    );

    // ATRACE()
    // Iterate over potentially occupied blocks
    for (int x = min_block.x; x <= max_block.x; ++x) {
        for (int y = min_block.y; y <= max_block.y; ++y) {
            for (int z = min_block.z; z <= max_block.z; ++z) {
                // Calculate AABB of the block in world space
                AABB block_aabb = {
                    vec3(x, y, z) * 16.0f,
                    vec3(x + 1, y + 1, z + 1) * 16.0f
                };

                // Check if the mesh AABB intersects with the block AABB
                if (!mesh_aabb.intersects(block_aabb)) {
                    continue; // Skip if no intersection
                }

                // Process the block
                int current_block = current_world(x, y, z);
                if (current_block < static_block_palette_size) {
                    // Prepare copy operation for static blocks
                    ivec2 src_block = get_block_xy(current_block);
                    ivec2 dst_block = get_block_xy(paletteCounter);

                    VkImageCopy block_copy = {};
                    block_copy.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
                    block_copy.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
                    block_copy.extent = {16, 16, 16};
                    block_copy.srcOffset = {src_block.x * 16, src_block.y * 16, 0};
                    block_copy.dstOffset = {dst_block.x * 16, dst_block.y * 16, 0};

                    blockCopyQueue.push_back(block_copy);
                    current_world(x, y, z) = paletteCounter;
                    ++paletteCounter;
                }
                // If block is already processed, do nothing
            }
        }
    }
    // ATRACE()
}

bool LumInternal::LumInternalRenderer::AABB::contains(const vec3& point) const {
    // ATRACE()
    return (point.x >= min.x && point.x <= max.x) &&
           (point.y >= min.y && point.y <= max.y) &&
           (point.z >= min.z && point.z <= max.z);
}

bool LumInternal::LumInternalRenderer::AABB::intersects(const AABB& other) const {
    // ATRACE()
    return (min.x <= other.max.x && max.x >= other.min.x) &&
           (min.y <= other.max.y && max.y >= other.min.y) &&
           (min.z <= other.max.z && max.z >= other.min.z);
}

void LumInternal::LumInternalRenderer::end_blockify() {
    size_t size_to_copy = world_size.x * world_size.y * world_size.z * sizeof (BlockID_t);
    assert (size_to_copy != 0);
    memcpy (stagingWorld.current().mapped, current_world.data(), size_to_copy);
    vmaFlushAllocation (lumal.VMAllocator, stagingWorld.current().alloc, 0, size_to_copy);
}

void LumInternal::LumInternalRenderer::blockify_custom (void* ptr) {
    size_t size_to_copy = world_size.x * world_size.y * world_size.z * sizeof (BlockID_t);
    memcpy (stagingWorld.current().mapped, ptr, size_to_copy);
    vmaFlushAllocation (lumal.VMAllocator, stagingWorld.current().alloc, 0, size_to_copy);
}

void set_blocks(const ivec3& coord, LumInternal::table3d<bool>& set, const LumInternal::table3d<LumInternal::BlockID_t>& current_world) {
    int sum_of_neighbours = 0;
    for (int dx = -1; dx <= 1; ++dx) {
    for (int dy = -1; dy <= 1; ++dy) {
    for (int dz = -1; dz <= 1; ++dz) {
        ivec3 test_block = ivec3(dx, dy, dz) + coord;
        sum_of_neighbours += current_world(test_block);
    }}}
    if (sum_of_neighbours > 0) {
        set(coord) = true;
    }
}

void push_radiance_updates(const ivec3& size, LumInternal::table3d<bool>& set, std::vector<i8vec4>& radianceUpdates) {
    for (int xx = 0; xx < size.x; ++xx) {
    for (int yy = 0; yy < size.y; ++yy) {
    for (int zz = 0; zz < size.z; ++zz) {
        if (set(xx, yy, zz)) {
            radianceUpdates.push_back(i8vec4(xx, yy, zz, 0));
        }
    }}}
}

// TODO: finish dynamic update system, integrate with RaVE
void LumInternal::LumInternalRenderer::update_radiance() {
    VkCommandBuffer& commandBuffer = computeCommandBuffers.current();
    TRACE()
    table3d<bool> 
        set = {};
    TRACE()
        set.allocate(world_size);
    TRACE()
        set.set(false);
    TRACE()
    radianceUpdates.clear();
    
    TRACE()
    for (int xx = 0; xx < world_size.x; xx++) {
        for (int yy = 0; yy < world_size.y; yy++) {
            for (int zz = 0; zz < world_size.z; zz++) {
                // int block_id = currentcares on less then a million blocks?
                // UPD: actually, smarter algorithms resulted in less perfomance
                int sum_of_neighbours = 0;
                for (int dx = -1; (dx <= +1); dx++) {
                for (int dy = -1; (dy <= +1); dy++) {
                for (int dz = -1; (dz <= +1); dz++) {
                    ivec3 test_block = ivec3 (xx + dx, yy + dy, zz + dz);
                    // kinda slow... but who cares on less then 1m blocks
                    // safity
                    test_block = glm::clamp(test_block, ivec3(0), ivec3(world_size)-1);
                    sum_of_neighbours += current_world (test_block);
                }}}
                if(sum_of_neighbours > 0){
                    radianceUpdates.push_back (i8vec4 (xx, yy, zz, 0));
                    set(ivec3(xx, yy, zz)) = true;
                }
            }
        }
    }
    // special updates are ones requested via API
    TRACE()
    for (auto u : specialRadianceUpdates) {
        if (!set(ivec3(u.x, u.y, u.z))) {
            radianceUpdates.push_back (u);
        }
    } specialRadianceUpdates.clear();
    set.deallocate();
    VkDeviceSize bufferSize = sizeof (radianceUpdates[0]) * radianceUpdates.size();
    memcpy (stagingRadianceUpdates.current().mapped, radianceUpdates.data(), bufferSize);

    VkBufferCopy
        copyRegion = {};
        copyRegion.size = bufferSize;
    vkCmdCopyBuffer (commandBuffer, stagingRadianceUpdates.current().buffer, gpuRadianceUpdates.current().buffer, 1, &copyRegion);
    lumal.cmdPipelineBarrier (commandBuffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
        gpuRadianceUpdates.current());
    lumal.cmdBindPipe(commandBuffer, &radiancePipe);
    /**/ vkCmdBindDescriptorSets (commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, radiancePipe.lineLayout, 0, 1, &radiancePipe.sets.current(), 0, 0);
    int magic_number = lumal.iFrame % 2;
    //if fast than increase work
    if (timeTakenByRadiance < 1.8) {
        magicSize --;
        //if slow than decrease work
    } else if (timeTakenByRadiance > 2.2) {
        magicSize ++;
    }
    // LOG(timeTakenByRadiance);
    // LOG(magicSize);
    magicSize = glm::max (magicSize, 1); //never remove
    magicSize = glm::min (magicSize, 10);
    struct rtpc {int time, iters, size, shift;} pushconstant = {lumal.iFrame, 0, magicSize, lumal.iFrame % magicSize};
    vkCmdPushConstants (commandBuffer, radiancePipe.lineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof (pushconstant), &pushconstant);
    int wg_count = radianceUpdates.size() / magicSize;
    PLACE_TIMESTAMP_OUTSIDE(commandBuffer);
    vkCmdDispatch (commandBuffer, wg_count, 1, 1);
    PLACE_TIMESTAMP_OUTSIDE(commandBuffer);
    lumal.cmdPipelineBarrier (commandBuffer,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_WRITE_BIT,
        radianceCache.current());
    lumal.cmdPipelineBarrier (commandBuffer,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
        radianceCache.current());
}

void LumInternal::LumInternalRenderer::recalculate_df() {
    VkCommandBuffer& commandBuffer = computeCommandBuffers.current();
    lumal.cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
        distancePalette.current());
    lumal.cmdBindPipe(commandBuffer, &dfxPipe);
    /**/ vkCmdBindDescriptorSets (commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, dfxPipe.lineLayout, 0, 1, &dfxPipe.sets.current(), 0, 0);
    vkCmdDispatch (commandBuffer, 1, 1, 16 * paletteCounter);
    lumal.cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
        distancePalette.current());
    lumal.cmdBindPipe(commandBuffer, &dfyPipe);
    /**/ vkCmdBindDescriptorSets (commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, dfyPipe.lineLayout, 0, 1, &dfyPipe.sets.current(), 0, 0);
    vkCmdDispatch (commandBuffer, 1, 1, 16 * paletteCounter);
    lumal.cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
        distancePalette.current());
    lumal.cmdBindPipe(commandBuffer, &dfzPipe);
    /**/ vkCmdBindDescriptorSets (commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, dfzPipe.lineLayout, 0, 1, &dfzPipe.sets.current(), 0, 0);
    vkCmdDispatch (commandBuffer, 1, 1, 16 * paletteCounter);
    lumal.cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
        distancePalette.current());
}

void LumInternal::LumInternalRenderer::recalculate_bit() {
    VkCommandBuffer& commandBuffer = computeCommandBuffers.current();
    lumal.cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
        bitPalette.current());
    lumal.cmdBindPipe(commandBuffer, &bitmaskPipe);
    /**/ vkCmdBindDescriptorSets (commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, bitmaskPipe.lineLayout, 0, 1, &bitmaskPipe.sets.current(), 0, 0);
    vkCmdDispatch (commandBuffer, 1, 1, 8 * paletteCounter);
    lumal.cmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
        bitPalette.current());
}

void LumInternal::LumInternalRenderer::exec_copies() {
    VkCommandBuffer& commandBuffer = computeCommandBuffers.current();
    if (blockCopyQueue.size() != 0) {
        //we can copy from previous image, cause static blocks are same in both palettes. Additionaly, it gives src and dst optimal layouts
        lumal.cmdExplicitTransLayoutBarrier (commandBuffer, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
            &originBlockPalette.previous());
        lumal.cmdExplicitTransLayoutBarrier (commandBuffer, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL,
            VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
            VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
            &originBlockPalette.current());
        PLACE_TIMESTAMP_OUTSIDE(commandBuffer);
        vkCmdCopyImage (commandBuffer, originBlockPalette.previous().image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, originBlockPalette.current().image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, blockCopyQueue.size(), blockCopyQueue.data());
        PLACE_TIMESTAMP_OUTSIDE(commandBuffer);
        lumal.cmdExplicitTransLayoutBarrier (commandBuffer, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
            VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
            VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
            &originBlockPalette.previous());
        lumal.cmdExplicitTransLayoutBarrier (commandBuffer, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_GENERAL,
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
    lumal.cmdExplicitTransLayoutBarrier (commandBuffer, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_ACCESS_SHADER_READ_BIT, VK_ACCESS_TRANSFER_WRITE_BIT,
        &world.current());
    vkCmdCopyBufferToImage (commandBuffer, stagingWorld.current().buffer, world.current().image, VK_IMAGE_LAYOUT_GENERAL, 1, &copyRegion);
    lumal.cmdExplicitTransLayoutBarrier (commandBuffer, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
        &world.current());
}

void LumInternal::LumInternalRenderer::start_map() {
    VkCommandBuffer& commandBuffer = computeCommandBuffers.current();
    lumal.cmdBindPipe(commandBuffer, &mapPipe);
    /**/ vkCmdBindDescriptorSets (commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, mapPipe.lineLayout, 0, 1, &mapPipe.sets.current(), 0, 0);
    PLACE_TIMESTAMP_OUTSIDE(commandBuffer);
}

void LumInternal::LumInternalRenderer::map_mesh (InternalMeshModel* mesh, MeshTransform* mesh_trans) {
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
    dmat4 rotate = toMat4 ((*mesh_trans).rot);
    dmat4 shift = translate (identity<mat4>(), (*mesh_trans).shift);
    dmat4 transform = shift * rotate;
    AABB border_in_voxel = get_shift (transform, (*mesh).size);
    struct {ivec3 min; ivec3 max;} border;
    border.min = ivec3 (floor (border_in_voxel.min)) - ivec3 (0); // no -ivec3(1) cause IT STRETCHES MODELS;
    border.max = ivec3 ( ceil (border_in_voxel.max)) + ivec3 (0); // no +ivec3(1) cause IT STRETCHES MODELS;
    //todo: clamp here not in shader
    ivec3 map_area = (border.max - border.min);
    struct {mat4 trans; ivec4 shift;} itrans_shift = {mat4 (inverse (transform)), ivec4 (border.min, 0)};
    vkCmdPushConstants (commandBuffer, mapPipe.lineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof (itrans_shift), &itrans_shift);
    // ivec3 adjusted_size = ivec3(vec3(mesh.size) * vec3(2.0));

    // i have no fucking idea why it was  *3+3. Some sort of 3d-cube-diag fix?
    // vkCmdDispatch (commandBuffer, (map_area.x * 3 + 3) / 4, (map_area.y * 3 + 3) / 4, (map_area.z * 3 + 3) / 4);
    
    vkCmdDispatch (commandBuffer, (map_area.x + 3) / 4, (map_area.y + 3) / 4, (map_area.z + 3) / 4);
}

void LumInternal::LumInternalRenderer::end_map() {
    VkCommandBuffer& commandBuffer = computeCommandBuffers.current();
    lumal.cmdExplicitTransLayoutBarrier (commandBuffer, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_GENERAL,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
        &originBlockPalette.current());
    PLACE_TIMESTAMP_OUTSIDE(commandBuffer);
}

void LumInternal::LumInternalRenderer::end_compute() {
    VkCommandBuffer& commandBuffer = computeCommandBuffers.current();
}

void LumInternal::LumInternalRenderer::start_lightmap() {
    VkCommandBuffer& commandBuffer = lightmapCommandBuffers.current();
    PLACE_TIMESTAMP_OUTSIDE(commandBuffer);
    struct unicopy {mat4 trans;} unicopy = {lightTransform};
    vkCmdUpdateBuffer (commandBuffer, lightUniform.current().buffer, 0, sizeof (unicopy), &unicopy);
    lumal.cmdPipelineBarrier (commandBuffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT,
        lightUniform.current());
    lumal.cmdBeginRenderPass(commandBuffer, &lightmapRpass);
}

void LumInternal::LumInternalRenderer::lightmap_start_blocks() {
    VkCommandBuffer& commandBuffer = lightmapCommandBuffers.current();
    lumal.cmdBindPipe(commandBuffer, &lightmapBlocksPipe);
    /**/ vkCmdBindDescriptorSets (commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, lightmapBlocksPipe.lineLayout, 0, 1, &lightmapBlocksPipe.sets.current(), 0, 0);
}

void LumInternal::LumInternalRenderer::lightmap_start_models() {
    VkCommandBuffer& commandBuffer = lightmapCommandBuffers.current();
    lumal.cmdBindPipe(commandBuffer, &lightmapModelsPipe);
    /**/ vkCmdBindDescriptorSets (commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, lightmapModelsPipe.lineLayout, 0, 1, &lightmapModelsPipe.sets.current(), 0, 0);
}

void LumInternal::LumInternalRenderer::end_lightmap() {
    VkCommandBuffer& commandBuffer = lightmapCommandBuffers.current();
    PLACE_TIMESTAMP_OUTSIDE(commandBuffer);
    lumal.cmdEndRenderPass (commandBuffer, &lightmapRpass);
}

void LumInternal::LumInternalRenderer::start_raygen() {
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
        vec2 (lumal.swapChainExtent.width, lumal.swapChainExtent.height),
        lumal.iFrame
    };
    vkCmdUpdateBuffer (commandBuffer, uniform.current().buffer, 0, sizeof (unicopy), &unicopy);
    lumal.cmdPipelineBarrier (commandBuffer,
        VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
        VK_ACCESS_TRANSFER_WRITE_BIT, VK_ACCESS_MEMORY_READ_BIT,
        uniform.current());
    lumal.cmdBeginRenderPass(commandBuffer, &gbufferRpass);
}

void LumInternal::LumInternalRenderer::raygen_start_blocks() {
    VkCommandBuffer& commandBuffer = graphicsCommandBuffers.current();
    PLACE_TIMESTAMP_OUTSIDE(commandBuffer);
    lumal.cmdBindPipe(commandBuffer, &raygenBlocksPipe);
    /**/ vkCmdBindDescriptorSets (commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, raygenBlocksPipe.lineLayout, 0, 1, &raygenBlocksPipe.sets.current(), 0, 0);
}

static VkBuffer old_buff = NULL;
static bool is_face_visible (vec3 normal, vec3 camera_dir) {
    return (dot (normal, camera_dir) < 0.0f);
}

#define CHECK_N_DRAW_BLOCK(__norm, __dir) \
if(is_face_visible(i8vec3(__norm), camera.cameraDir)) {\
    draw_block_face(__norm, (*block_mesh).triangles.__dir, block_id);\
}
void LumInternal::LumInternalRenderer::draw_block_face (i8vec3 normal, IndexedVertices& buff, int block_id) {
    VkCommandBuffer& commandBuffer = graphicsCommandBuffers.current();
    // assert (buff.indexes.data());
    assert (block_id);
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
    vkCmdDrawIndexed (commandBuffer, buff.icount, 1, buff.offset, 0, 0);
}

void LumInternal::LumInternalRenderer::raygen_block (InternalMeshModel* block_mesh, int block_id, ivec3 shift) {
    VkCommandBuffer& commandBuffer = graphicsCommandBuffers.current();
    VkBuffer vertexBuffers[] = {(*block_mesh).triangles.vertexes.buffer};
    VkDeviceSize offsets[] = {0};
    DEBUG_LOG((*block_mesh).triangles.vertexes.buffer)

    lumal.cmdBindVertexBuffers (commandBuffer, 0, 1, vertexBuffers, offsets);
    lumal.cmdBindIndexBuffer (commandBuffer, (*block_mesh).triangles.indices.buffer, 0, VK_INDEX_TYPE_UINT16);
    ;

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

void LumInternal::LumInternalRenderer::raygen_start_models() {
    VkCommandBuffer& commandBuffer = graphicsCommandBuffers.current();
    vkCmdNextSubpass (commandBuffer, VK_SUBPASS_CONTENTS_INLINE);
    lumal.cmdBindPipe(commandBuffer, &raygenModelsPipe);
    /**/ vkCmdBindDescriptorSets (commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, raygenModelsPipe.lineLayout, 0, 1, &raygenModelsPipe.sets.current(), 0, 0);
}

#define CHECK_N_DRAW_MODEL(__norm, __dir) \
if(is_face_visible(model_trans->rot*__norm, camera.cameraDir)) {\
    draw_model_face(__norm, (*model_mesh).triangles.__dir);\
}
void LumInternal::LumInternalRenderer::draw_model_face (vec3 normal, IndexedVertices& buff) {
    VkCommandBuffer& commandBuffer = graphicsCommandBuffers.current();
    // assert (buff.indexes.data());
    struct {vec4 fnorm;} norms = {vec4 (normal, 0)};
    vkCmdPushConstants (commandBuffer, raygenModelsPipe.lineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT,
        32, sizeof (norms), &norms);
    vkCmdDrawIndexed (commandBuffer, buff.icount, 1, buff.offset, 0, 0);
}

void LumInternal::LumInternalRenderer::raygen_model (InternalMeshModel* model_mesh, MeshTransform* model_trans) {
    VkCommandBuffer& commandBuffer = graphicsCommandBuffers.current();
    VkBuffer vertexBuffers[] = { (*model_mesh).triangles.vertexes.buffer};
    VkDeviceSize offsets[] = {0};

    lumal.cmdBindVertexBuffers (commandBuffer, 0, 1, vertexBuffers, offsets);
    lumal.cmdBindIndexBuffer (commandBuffer, (*model_mesh).triangles.indices.buffer, 0, VK_INDEX_TYPE_UINT16);
    /*
        vec4 rot;
        vec4 shift;
        vec4 fnormal; //not encoded
    */
    struct {quat rot; vec4 shift;} rotshift = {model_trans->rot, vec4 (model_trans->shift, 0)};
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
void LumInternal::LumInternalRenderer::lightmap_block_face (i8vec3 normal, IndexedVertices& buff, int block_id) {
    VkCommandBuffer& lightmap_commandBuffer = lightmapCommandBuffers.current();
    vkCmdDrawIndexed (lightmap_commandBuffer, buff.icount, 1, buff.offset, 0, 0);
}

void LumInternal::LumInternalRenderer::lightmap_block (InternalMeshModel* block_mesh, int block_id, ivec3 shift) {
    VkCommandBuffer& lightmap_commandBuffer = lightmapCommandBuffers.current();
    VkBuffer vertexBuffers[] = {(*block_mesh).triangles.vertexes.buffer};
    VkDeviceSize offsets[] = {0};
    // DEBUG_LOG((*block_mesh).triangles.vertexes.buffer)

    lumal.cmdBindVertexBuffers (lightmap_commandBuffer, 0, 1, vertexBuffers, offsets);
    lumal.cmdBindIndexBuffer (lightmap_commandBuffer, (*block_mesh).triangles.indices.buffer, 0, VK_INDEX_TYPE_UINT16);
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
if(is_face_visible(model_trans->rot*(__norm), lightDir)){\
    lightmap_model_face(__norm, (*model_mesh).triangles.__dir);\
}
void LumInternal::LumInternalRenderer::lightmap_model_face (vec3 normal, IndexedVertices& buff) {
    VkCommandBuffer& lightmap_commandBuffer = lightmapCommandBuffers.current();
    vkCmdDrawIndexed (lightmap_commandBuffer, buff.icount, 1, buff.offset, 0, 0);
}

void LumInternal::LumInternalRenderer::lightmap_model (InternalMeshModel* model_mesh, MeshTransform* model_trans) {
    VkCommandBuffer& lightmap_commandBuffer = lightmapCommandBuffers.current();
    VkBuffer vertexBuffers[] = {(*model_mesh).triangles.vertexes.buffer};
    VkDeviceSize offsets[] = {0};

    lumal.cmdBindVertexBuffers (lightmap_commandBuffer, 0, 1, vertexBuffers, offsets);
    lumal.cmdBindIndexBuffer (lightmap_commandBuffer, (*model_mesh).triangles.indices.buffer, 0, VK_INDEX_TYPE_UINT16);
    /*
        vec4 rot;
        vec4 shift;
        vec4 fnormal; //not encoded
    */
    struct {quat rot; vec4 shift;} rotshift = {model_trans->rot, vec4 (model_trans->shift, 0)};
    vkCmdPushConstants (lightmap_commandBuffer, lightmapModelsPipe.lineLayout, VK_SHADER_STAGE_VERTEX_BIT,
        0, sizeof (rotshift), &rotshift);
    CHECK_N_LIGHTMAP_MODEL (vec3 (+1, 0, 0), Pzz);
    CHECK_N_LIGHTMAP_MODEL (vec3 (-1, 0, 0), Nzz);
    CHECK_N_LIGHTMAP_MODEL (vec3 (0, +1, 0), zPz);
    CHECK_N_LIGHTMAP_MODEL (vec3 (0, -1, 0), zNz);
    CHECK_N_LIGHTMAP_MODEL (vec3 (0, 0, +1), zzP);
    CHECK_N_LIGHTMAP_MODEL (vec3 (0, 0, -1), zzN);
}

void LumInternal::LumInternalRenderer::update_particles() {
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
    vmaFlushAllocation (lumal.VMAllocator, gpuParticles.current().alloc, 0, sizeof (Particle)*capped_particle_count);
}

void LumInternal::LumInternalRenderer::raygen_map_particles() {
    VkCommandBuffer& commandBuffer = graphicsCommandBuffers.current();
    //go to next no matter what
    vkCmdNextSubpass (commandBuffer, VK_SUBPASS_CONTENTS_INLINE);
    PLACE_TIMESTAMP_OUTSIDE(commandBuffer);
    if (!particles.empty()) { //just for safity
        lumal.cmdBindPipe(commandBuffer, &raygenParticlesPipe);
        /**/ vkCmdBindDescriptorSets (commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, raygenParticlesPipe.lineLayout, 0, 1, &raygenParticlesPipe.sets.current(), 0, 0);
        VkDeviceSize offsets[] = {0};
        vkCmdBindVertexBuffers (commandBuffer, 0, 1, &gpuParticles.current().buffer, offsets);
        vkCmdDraw (commandBuffer, particles.size(), 1, 0, 0);
    }
    PLACE_TIMESTAMP_OUTSIDE(commandBuffer);
}

void LumInternal::LumInternalRenderer::raygen_start_grass() {
    VkCommandBuffer& commandBuffer = graphicsCommandBuffers.current();
    PLACE_TIMESTAMP_OUTSIDE(commandBuffer);
    vkCmdNextSubpass (commandBuffer, VK_SUBPASS_CONTENTS_INLINE);
    // render.cmdBindPipe(commandBuffer, &raygenGrassPipe);
    // /**/ vkCmdBindDescriptorSets (commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, raygenGrassPipe.lineLayout, 0, 1, &raygenGrassPipe.sets.current(), 0, 0);
}

//DOES NOT run in renderpass. Placed here cause spatially makes sense
void LumInternal::LumInternalRenderer::updade_grass (vec2 windDirection) {
    VkCommandBuffer& commandBuffer = computeCommandBuffers.current();
    lumal.cmdBindPipe(commandBuffer, &updateGrassPipe);
    struct {vec2 wd, cp; float dt;} pushconstant = {windDirection, {}, float (lumal.iFrame)};
    vkCmdPushConstants (commandBuffer, updateGrassPipe.lineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof (pushconstant), &pushconstant);
    /**/ vkCmdBindDescriptorSets (commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, updateGrassPipe.lineLayout, 0, 1, &updateGrassPipe.sets[0], 0, 0);
    lumal.cmdPipelineBarrier (commandBuffer,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_WRITE_BIT,
        grassState.current());
    lumal.cmdPipelineBarrier (commandBuffer,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
        grassState.current());
    PLACE_TIMESTAMP_OUTSIDE(commandBuffer);
    vkCmdDispatch (commandBuffer, (world_size.x * 2 + 7) / 8, (world_size.y * 2 + 7) / 8, 1); //2x8 2x8 1x1
    PLACE_TIMESTAMP_OUTSIDE(commandBuffer);
}

void LumInternal::LumInternalRenderer::updade_water() {
    VkCommandBuffer& commandBuffer = computeCommandBuffers.current();
    lumal.cmdBindPipe(commandBuffer, &updateWaterPipe);
    struct {vec2 wd; float dt;} pushconstant = {{}, float (lumal.iFrame)};
    vkCmdPushConstants (commandBuffer, updateWaterPipe.lineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof (pushconstant), &pushconstant);
    vkCmdBindDescriptorSets (commandBuffer, VK_PIPELINE_BIND_POINT_COMPUTE, updateWaterPipe.lineLayout, 0, 1, &updateWaterPipe.sets[0], 0, 0);
    lumal.cmdPipelineBarrier (commandBuffer,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
        VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_WRITE_BIT,
        waterState.current());
    lumal.cmdPipelineBarrier (commandBuffer,
        VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
        VK_ACCESS_SHADER_WRITE_BIT, VK_ACCESS_SHADER_READ_BIT,
        waterState.current());
    PLACE_TIMESTAMP_OUTSIDE(commandBuffer);
    vkCmdDispatch (commandBuffer, (world_size.x * 2 + 7) / 8, (world_size.y * 2 + 7) / 8, 1); //2x8 2x8 1x1
    PLACE_TIMESTAMP_OUTSIDE(commandBuffer);
}

//blade is hardcoded but it does not really increase perfomance
//done this way for simplicity, can easilly be replaced with any vertex buffer
void LumInternal::LumInternalRenderer::raygen_map_grass (InternalMeshFoliage* grass, ivec3 pos) {
    VkCommandBuffer& commandBuffer = graphicsCommandBuffers.current();
    int size = 10;
    // int size = grass.dencity;
    bool x_flip = camera.cameraDir.x < 0;
    bool y_flip = camera.cameraDir.y < 0;

    // it is somewhat cached
    lumal.cmdBindPipe(commandBuffer, &grass->pipe);

    struct {vec4 _shift; int _size, _time; int xf, yf;} raygen_pushconstant = {vec4(pos,0), size, lumal.iFrame, x_flip, y_flip};
    vkCmdPushConstants (commandBuffer, grass->pipe.lineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof (raygen_pushconstant), &raygen_pushconstant);
    const int verts_per_blade = grass->vertices; //for triangle strip
    const int blade_per_instance = 1; //for triangle strip
    vkCmdDraw (commandBuffer,
               verts_per_blade* blade_per_instance,
               (size * size + (blade_per_instance - 1)) / blade_per_instance,
               0, 0);
}

void LumInternal::LumInternalRenderer::raygen_start_water() {
    VkCommandBuffer& commandBuffer = graphicsCommandBuffers.current();
    PLACE_TIMESTAMP_OUTSIDE(commandBuffer);
    PLACE_TIMESTAMP_OUTSIDE(commandBuffer);
    vkCmdNextSubpass (commandBuffer, VK_SUBPASS_CONTENTS_INLINE);
    lumal.cmdBindPipe(commandBuffer, &raygenWaterPipe);
    /**/ vkCmdBindDescriptorSets (commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, raygenWaterPipe.lineLayout, 0, 1, &raygenWaterPipe.sets.current(), 0, 0);
}

void LumInternal::LumInternalRenderer::raygen_map_water (InternalMeshLiquid water, ivec3 pos) {
    VkCommandBuffer& commandBuffer = graphicsCommandBuffers.current();
    int qualitySize = 32;
    struct {vec4 _shift; int _size, _time;} raygen_pushconstant = {vec4(pos,0), qualitySize, lumal.iFrame};
    vkCmdPushConstants (commandBuffer, raygenWaterPipe.lineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof (raygen_pushconstant), &raygen_pushconstant);
    const int verts_per_water_tape = qualitySize * 2 + 2;
    const int tapes_per_block = qualitySize;
    vkCmdDraw (commandBuffer,
               verts_per_water_tape,
               tapes_per_block,
               0, 0);
}

void LumInternal::LumInternalRenderer::end_raygen() {
    VkCommandBuffer& commandBuffer = graphicsCommandBuffers.current();
    lumal.cmdEndRenderPass (commandBuffer, &gbufferRpass);
}

void LumInternal::LumInternalRenderer::start_2nd_spass() {
    VkCommandBuffer& commandBuffer = graphicsCommandBuffers.current();
    vector<AoLut> ao_lut = generateLUT (8, 16.0 / 1000.0,
                        dvec2 (lumal.swapChainExtent.width, lumal.swapChainExtent.height),
                        dvec3 (camera.horizline* camera.viewSize.x / 2.0), dvec3 (camera.vertiline* camera.viewSize.y / 2.0));
    vkCmdUpdateBuffer (commandBuffer, aoLutUniform.current().buffer, 0, sizeof (AoLut)*ao_lut.size(), ao_lut.data());
    lumal.cmdPipelineBarrier (commandBuffer,
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
    //     renderPassInfo.clearValueCount = Lumal::clearColors.size();
    //     renderPassInfo.pLumal::ClearValues = Lumal::clearColors.data();
    // vkCmdBeginRenderPass (commandBuffer, &renderPassInfo, VK_SUBPASS_CONTENTS_INLINE);
    lumal.cmdBeginRenderPass(commandBuffer, &shadeRpass);

    // cmdSetViewport(commandBuffer, render.swapChainExtent);
}

void LumInternal::LumInternalRenderer::diffuse() {
    VkCommandBuffer& commandBuffer = graphicsCommandBuffers.current();
    PLACE_TIMESTAMP_OUTSIDE(commandBuffer);
    lumal.cmdBindPipe(commandBuffer, &diffusePipe);
    /**/ vkCmdBindDescriptorSets (commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, diffusePipe.lineLayout, 0, 1, &diffusePipe.sets.current(), 0, 0);
    struct rtpc {vec4 v1, v2; mat4 lp;} pushconstant = {vec4 (camera.cameraPos, intBitsToFloat (lumal.iFrame)), vec4 (camera.cameraDir, 0), lightTransform};
    vkCmdPushConstants (commandBuffer, diffusePipe.lineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof (pushconstant), &pushconstant);
    PLACE_TIMESTAMP_OUTSIDE(commandBuffer);
    vkCmdDraw (commandBuffer, 3, 1, 0, 0);
    PLACE_TIMESTAMP_OUTSIDE(commandBuffer);
}

void LumInternal::LumInternalRenderer::ambient_occlusion() {
    VkCommandBuffer& commandBuffer = graphicsCommandBuffers.current();
    vkCmdNextSubpass (commandBuffer, VK_SUBPASS_CONTENTS_INLINE);
    lumal.cmdBindPipe(commandBuffer, &aoPipe);
    /**/ vkCmdBindDescriptorSets (commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, aoPipe.lineLayout, 0, 1, &aoPipe.sets.current(), 0, 0);
    PLACE_TIMESTAMP_OUTSIDE(commandBuffer);
    vkCmdDraw (commandBuffer, 3, 1, 0, 0);
    PLACE_TIMESTAMP_OUTSIDE(commandBuffer);
}

void LumInternal::LumInternalRenderer::glossy_raygen() {
    VkCommandBuffer& commandBuffer = graphicsCommandBuffers.current();
    vkCmdNextSubpass (commandBuffer, VK_SUBPASS_CONTENTS_INLINE);
    lumal.cmdBindPipe(commandBuffer, &fillStencilGlossyPipe);
    /**/ vkCmdBindDescriptorSets (commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, fillStencilGlossyPipe.lineLayout, 0, 1, &fillStencilGlossyPipe.sets.current(), 0, 0);
    PLACE_TIMESTAMP_OUTSIDE(commandBuffer);
    vkCmdDraw (commandBuffer, 3, 1, 0, 0);
    PLACE_TIMESTAMP_OUTSIDE(commandBuffer);
}

void LumInternal::LumInternalRenderer::raygen_start_smoke() {
    VkCommandBuffer& commandBuffer = graphicsCommandBuffers.current();
    vkCmdNextSubpass (commandBuffer, VK_SUBPASS_CONTENTS_INLINE);
    lumal.cmdBindPipe(commandBuffer, &fillStencilSmokePipe);
    /**/ vkCmdBindDescriptorSets (commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, fillStencilSmokePipe.lineLayout, 0, 1, &fillStencilSmokePipe.sets.current(), 0, 0);
}

void LumInternal::LumInternalRenderer::raygen_map_smoke(InternalMeshVolumetric smoke, ivec3 pos) {
    VkCommandBuffer& commandBuffer = graphicsCommandBuffers.current();
    struct rtpc {vec4 centerSize;} pushconstant = {vec4 (vec3 (pos) * 16.f, 32)};
    specialRadianceUpdates.push_back (ivec4 (11, 11, 1, 0));
    vkCmdPushConstants (commandBuffer, fillStencilSmokePipe.lineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof (pushconstant), &pushconstant);
    PLACE_TIMESTAMP_OUTSIDE(commandBuffer);
    vkCmdDraw (commandBuffer, 36, 1, 0, 0);
    PLACE_TIMESTAMP_OUTSIDE(commandBuffer);
}

void LumInternal::LumInternalRenderer::smoke() {
    VkCommandBuffer& commandBuffer = graphicsCommandBuffers.current();
    vkCmdNextSubpass (commandBuffer, VK_SUBPASS_CONTENTS_INLINE);
    lumal.cmdBindPipe(commandBuffer, &smokePipe);
    /**/ vkCmdBindDescriptorSets (commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, smokePipe.lineLayout, 0, 1, &smokePipe.sets.current(), 0, 0);
    PLACE_TIMESTAMP_OUTSIDE(commandBuffer);
    vkCmdDraw (commandBuffer, 3, 1, 0, 0);
    PLACE_TIMESTAMP_OUTSIDE(commandBuffer);
}

void LumInternal::LumInternalRenderer::glossy() {
    VkCommandBuffer& commandBuffer = graphicsCommandBuffers.current();
    vkCmdNextSubpass (commandBuffer, VK_SUBPASS_CONTENTS_INLINE);
    lumal.cmdBindPipe(commandBuffer, &glossyPipe);
    /**/ vkCmdBindDescriptorSets (commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, glossyPipe.lineLayout, 0, 1, &glossyPipe.sets.current(), 0, 0);
    struct rtpc {vec4 v1, v2;} pushconstant = {vec4 (camera.cameraPos, 0), vec4 (camera.cameraDir, 0)};
    vkCmdPushConstants (commandBuffer, glossyPipe.lineLayout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof (pushconstant), &pushconstant);
    PLACE_TIMESTAMP_OUTSIDE(commandBuffer);
    vkCmdDraw (commandBuffer, 3, 1, 0, 0);
    PLACE_TIMESTAMP_OUTSIDE(commandBuffer);
}

void LumInternal::LumInternalRenderer::tonemap() {
    VkCommandBuffer& commandBuffer = graphicsCommandBuffers.current();
    vkCmdNextSubpass (commandBuffer, VK_SUBPASS_CONTENTS_INLINE);
    lumal.cmdBindPipe(commandBuffer, &tonemapPipe);
    /**/ vkCmdBindDescriptorSets (commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, tonemapPipe.lineLayout, 0, 1, &tonemapPipe.sets.current(), 0, 0);
    PLACE_TIMESTAMP_OUTSIDE(commandBuffer);
    vkCmdDraw (commandBuffer, 3, 1, 0, 0);
    PLACE_TIMESTAMP_OUTSIDE(commandBuffer);
}

void LumInternal::LumInternalRenderer::end_2nd_spass() {
    VkCommandBuffer& commandBuffer = graphicsCommandBuffers.current();
    // render.cmdEndRenderPass(commandBuffer);
}

void LumInternal::LumInternalRenderer::start_ui() {
    VkCommandBuffer& commandBuffer = graphicsCommandBuffers.current();
    PLACE_TIMESTAMP_OUTSIDE(commandBuffer);
    vkCmdNextSubpass (commandBuffer, VK_SUBPASS_CONTENTS_INLINE);
    //no dset
    vkCmdBindPipeline (commandBuffer, VK_PIPELINE_BIND_POINT_GRAPHICS, overlayPipe.line);
    VkViewport 
        viewport = {};
        viewport.x = 0.0f;
        viewport.y = 0.0f;
        viewport.width = (float) (lumal.swapChainExtent.width );
        viewport.height = (float) (lumal.swapChainExtent.height);
        viewport.minDepth = 0.0f;
        viewport.maxDepth = 1.0f;
    vkCmdSetViewport (commandBuffer, 0, 1, &viewport);
        ui_render_interface->last_scissors.offset = {0, 0};
        ui_render_interface->last_scissors.extent = lumal.swapChainExtent;
    vkCmdSetScissor (commandBuffer, 0, 1, &ui_render_interface->last_scissors);
}

void LumInternal::LumInternalRenderer::end_ui() {
    VkCommandBuffer& commandBuffer = graphicsCommandBuffers.current();
    lumal.cmdEndRenderPass (commandBuffer, &shadeRpass); //end of blur2present rpass
    PLACE_TIMESTAMP_OUTSIDE(commandBuffer);
}

void LumInternal::LumInternalRenderer::end_frame() {
TRACE();
    lumal.end_frame({
        // "Special" cmb used by UI copies & layout transitions HAS to be first
        // Otherwise image state is LAYOUT_UNDEFINED
        copyCommandBuffers.current(),
        computeCommandBuffers.current(),
        lightmapCommandBuffers.current(),
        graphicsCommandBuffers.current(),
    });

TRACE();
    copyCommandBuffers.move(); //runtime copies for ui. Also does first frame resources
TRACE();
    computeCommandBuffers.move();
TRACE();
    lightmapCommandBuffers.move();
TRACE();
    graphicsCommandBuffers.move();
TRACE();
    lightmap.move();
TRACE();
    highresFrame.move();
TRACE();
    highresDepthStencil.move();
TRACE();
    highresMatNorm.move();
TRACE();
    
    stencilViewForDS.move();
TRACE();
    farDepth.move(); //represents how much should smoke traversal for
TRACE();
    nearDepth.move(); //represents how much should smoke traversal for
TRACE();
    // maskFrame.move(); //where lowres renders to. Blends with highres afterwards
TRACE();
    stagingWorld.move();
TRACE();
    world.move(); //can i really use just one?
TRACE();
    radianceCache.move();
TRACE();
    originBlockPalette.move();
TRACE();
    // distancePalette.move();
TRACE();
    // bitPalette.move(); //bitmask of originBlockPalette
TRACE();
    materialPalette.move();
TRACE();
    lightUniform.move();
TRACE();
    uniform.move();
TRACE();
    aoLutUniform.move();
TRACE();
    gpuRadianceUpdates.move();
TRACE();
    stagingRadianceUpdates.move();
TRACE();
    gpuParticles.move(); //multiple because cpu-related work
TRACE();
    perlinNoise2d.move(); //full-world grass shift (~direction) texture sampled in grass
TRACE();
    grassState.move(); //full-world grass shift (~direction) texture sampled in grass
TRACE();
    waterState.move(); //~same but water
TRACE();
    perlinNoise3d.move(); //4 channels of different tileable noise for volumetrics
TRACE();

    double timestampPeriod = lumal.physicalDeviceProperties.limits.timestampPeriod;
    timeTakenByRadiance = float (lumal.timestamps[1] - lumal.timestamps[0]) * timestampPeriod / 1000000.0f;

    // #ifdef MEASURE_PERFOMANCE
    // #endif
    // printl(timeTakenByRadiance)
TRACE();
}

ivec2 get_block_xy (int N) {
    int x = N % LumInternal::BLOCK_PALETTE_SIZE_X;
    int y = N / LumInternal::BLOCK_PALETTE_SIZE_X;
    assert (y <= LumInternal::BLOCK_PALETTE_SIZE_Y);
    return ivec2 (x, y);
}