#include "renderer/src/internal_render.hpp"
#include "renderer.hpp" //opaque
#include "opaque_renderer_members.hpp"

#include "defines/macros.hpp"


// am i getting canceled?
#define let auto&

// just a shortcut for later
// #define transparent (*((Lum::OpaqueRendererMembers*)(&this->promised_opaque_members)))
#ifdef LUM_USE_CTRACK
    #include <ctrack.hpp>
    #define PROFILE_TIMESTAMP CTRACK;
#else
    #define PROFILE_TIMESTAMP 
#endif


Lum::Renderer::Renderer() noexcept : Renderer(1024, 1024, 64, 64, 64) {}
Lum::Renderer::Renderer(
        size_t block_palette_size, 
        size_t mesh_storage_size, 
        size_t foliage_storage_size, 
        size_t volumetric_storage_size, 
        size_t liquid_storage_size) noexcept 
        : curr_time(std::chrono::steady_clock::now()), 
        block_que(),
        mesh_que(),
        foliage_que(),
        liquid_que(),
        volumetric_que()
    {
        // this calls constructors on memory that is visible to Renderer as just bytes, but in fact represents Lum::OpaqueRendererMembers
        opaque_members = new Lum::OpaqueRendererMembers(
            block_palette_size, 
            mesh_storage_size,
            foliage_storage_size,
            volumetric_storage_size,
            liquid_storage_size
        );
    }
Lum::Renderer::~Renderer() noexcept 
    // {cleanup();} // i do not like it being that impicit
    {
        delete opaque_members;
    }

void Lum::Renderer::init(Lum::Settings lum_settings) noexcept {
    // sizeof(psAccessor);

    LumInternal::LumSettings 
        internal_settings = {}; 
        //copying from external
        internal_settings.world_size                = lum_settings.world_size;
        internal_settings.static_block_palette_size = lum_settings.static_block_palette_size;
        internal_settings.maxParticleCount          = lum_settings.maxParticleCount;
        internal_settings.timestampCount            = lum_settings.timestampCount;
        internal_settings.fif                       = lum_settings.fif;
        internal_settings.vsync                     = lum_settings.vsync;
        internal_settings.fullscreen                = lum_settings.fullscreen;
        internal_settings.debug                     = lum_settings.debug;
        internal_settings.profile                   = lum_settings.profile;
    
        //this has to be managed better but works for now
        // internal_settings.debug = false; //Validation Layers. Use them while developing or be tricked into thinking that your code is correct
        // internal_settings.timestampCount = 48;
        internal_settings.profile = true; //monitors perfomance via timestamps. You can place one with PLACE_TIMESTAMP() macro
        // currently fif has bug in it, do not change for now 
        internal_settings.fif = LumInternal::MAX_FRAMES_IN_FLIGHT; // Frames In Flight. If 1, then record cmdbuff and submit it. If multiple, cpu will (might) be ahead of gpu by FIF-1, which makes GPU wait less

        internal_settings.deviceFeatures.vertexPipelineStoresAndAtomics = VK_TRUE;
        internal_settings.deviceFeatures.fragmentStoresAndAtomics = VK_TRUE;
        internal_settings.deviceFeatures.geometryShader = VK_TRUE;
        internal_settings.deviceFeatures.independentBlend = VK_TRUE;
        internal_settings.deviceFeatures.shaderInt16 = VK_TRUE;
        internal_settings.deviceFeatures11.storagePushConstant16 = VK_TRUE;
        internal_settings.deviceFeatures12.storagePushConstant8 = VK_TRUE;
        internal_settings.deviceFeatures12.shaderInt8 = VK_TRUE;
        internal_settings.deviceFeatures12.storageBuffer8BitAccess = VK_TRUE;
        // for assumeEXT, currently disabled and will be brought back with new shader system
        // VkPhysicalDeviceShaderExpectAssumeFeaturesKHR 
        //     expect;
        //     expect.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_EXPECT_ASSUME_FEATURES_KHR;
        //     expect.shaderExpectAssume = VK_TRUE;
        // settings.addExtraFeaturesPnext(&expect);
        
        internal_settings.deviceExtensions.push_back(VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME); //simplifies renderer for negative cost lol
        internal_settings.deviceExtensions.push_back(VK_KHR_SHADER_FLOAT16_INT8_EXTENSION_NAME); //pco / ubo +perfomance
        internal_settings.deviceExtensions.push_back(VK_KHR_16BIT_STORAGE_EXTENSION_NAME); //just explicit control
        internal_settings.deviceExtensions.push_back(VK_KHR_8BIT_STORAGE_EXTENSION_NAME); //just explicit control
        internal_settings.deviceExtensions.push_back(VK_EXT_SCALAR_BLOCK_LAYOUT_EXTENSION_NAME); // aka zero-pain buffers
        // settings.deviceExtensions.push_back(VK_KHR_SHADER_EXPECT_ASSUME_EXTENSION_NAME); // GLSL's assumeEXT(statement)

    // settings = lum_settings;
    opaque_members->settings = lum_settings;
    opaque_members->render.init(internal_settings);
        vkDeviceWaitIdle(opaque_members->render.lumal.device);

    should_close = false; // just in case, idk
}

void Lum::Renderer::loadWorld(const LumInternal::BlockID_t* world_data) noexcept {
    assert(world_data);
    // TODO
    opaque_members->render.load_scene("assets/scene");
}
void Lum::Renderer::loadWorld(const std::string& file) noexcept {
    // TODO
    opaque_members->render.load_scene(file.c_str());
}

void Lum::Renderer::setWorldBlock(const ivec3& pos, MeshBlock block) noexcept {
    // yep, CPU-side structure
    // it is copied to gpu every frame for internal reasons
    // in some sence, this is a cache
    opaque_members->render.origin_world(pos) = block;
}
void Lum::Renderer::setWorldBlock(int x, int y, int z, MeshBlock block) noexcept {
    opaque_members->render.origin_world(x,y,z) = block;
}

LumInternal::BlockID_t Lum::Renderer::getWorldBlock(const ivec3& pos) const noexcept {
    LumInternal::BlockID_t block = opaque_members->render.origin_world(pos);
    return block;
}
LumInternal::BlockID_t Lum::Renderer::getWorldBlock(int x, int y, int z) const noexcept {
    LumInternal::BlockID_t block = opaque_members->render.origin_world(x,y,z);
    return block;  
}

void Lum::Renderer::loadBlock(MeshBlock block, LumInternal::BlockVoxels* block_data) noexcept {
    memcpy(&opaque_members->block_palette[block], block_data, sizeof(LumInternal::BlockVoxels));
    // opaque_members->render.updateMaterialPalette(mat_palette);
}
void Lum::Renderer::loadBlock(MeshBlock block, const std::string& file) noexcept {
    opaque_members->render.load_block(&opaque_members->block_palette[block], file.c_str());
}

void Lum::Renderer::uploadBlockPaletteToGPU() noexcept {
    opaque_members->render.updateBlockPalette(opaque_members->block_palette.data());
}

void Lum::Renderer::uploadMaterialPaletteToGPU() noexcept {
    opaque_members->render.updateMaterialPalette(opaque_members->mat_palette.data());
}
void Lum::Renderer::loadPalette(const std::string& file) noexcept{
    opaque_members->render.extract_palette(file.c_str(), opaque_members->mat_palette.data());
}
Lum::MeshModel Lum::Renderer::loadModel(const std::string& file, bool try_extract_palette) noexcept {
    LumInternal::InternalMeshModel* new_mesh = opaque_members->mesh_models_storage.allocate();
    opaque_members->render.load_mesh(new_mesh, file.c_str(), true, /*extract palette if no found = */ try_extract_palette, opaque_members->mat_palette.data());
    return new_mesh;
}
Lum::MeshModel Lum::Renderer::loadModel(LumInternal::Voxel* mesh_data, int x_size, int y_size, int z_size) noexcept {
    LumInternal::InternalMeshModel* new_mesh = opaque_members->mesh_models_storage.allocate();
    opaque_members->render.load_mesh(new_mesh, mesh_data, x_size,y_size,z_size, true);
    return new_mesh;
}
void Lum::Renderer::freeMesh(MeshModel model) noexcept {
    LumInternal::InternalMeshModel* mesh_ptr = (LumInternal::InternalMeshModel*) model.ptr;
    opaque_members->render.free_mesh(mesh_ptr);
}

Lum::MeshFoliage Lum::Renderer::loadFoliage(const std::string& vertex_shader_file, int vertices_per_foliage, int density) noexcept {
    LumInternal::InternalMeshFoliage* 
        new_foliage = opaque_members->mesh_foliage_storage.allocate();
        // this is not creating pipe(line). Only queuing shader file for later pipe(line)
        // i just hope C++ is not fucked up and lifetime of string is whole main
        new_foliage->vertex_shader_file = vertex_shader_file;
        // LOG(new_foliage->vertex_shader_file)
        new_foliage->vertices = vertices_per_foliage;
        new_foliage->dencity = density;
    // registering in internal renderer
    opaque_members->render.registered_foliage.push_back(new_foliage);
    return new_foliage;
}
Lum::MeshVolumetric Lum::Renderer::loadVolumetric(float max_density, float density_deviation, const u8vec3& color) noexcept {
    LumInternal::InternalMeshVolumetric* 
        new_volumetric = opaque_members->mesh_volumetric_storage.allocate();
        new_volumetric->max_dencity = max_density;
        new_volumetric->variation = density_deviation;
        new_volumetric->color = color;
    return new_volumetric;
}
Lum::MeshLiquid Lum::Renderer::loadLiquid(LumInternal::MatID_t main_mat, LumInternal::MatID_t foam_mat) noexcept {
    LumInternal::InternalMeshLiquid* 
        new_liquid = opaque_members->mesh_liquid_storage.allocate();
        new_liquid->main = main_mat;
        new_liquid->foam = foam_mat;
    return new_liquid;
}

static bool is_block_visible(dmat4 trans, dvec3 pos){
    for(int xx = 0; xx < 2; xx++){
    for(int yy = 0; yy < 2; yy++){
    for(int zz = 0; zz < 2; zz++){
        double x = double(xx) * 16.0;
        double y = double(yy) * 16.0;
        double z = double(zz) * 16.0;

        dvec3 new_pos = glm::quat_identity<double, defaultp>() * pos;
        dvec4 corner = dvec4(new_pos + dvec3(x, y, z), 1.0);
        dvec4 clip = trans * corner;

        // Check if within NDC range
        if ((clip.x >= -1.0) && (clip.y >= -1.0) && (clip.z >= -1.0) && 
            (clip.x <= +1.0) && (clip.y <= +1.0) && (clip.z <= +1.0)) {
                return true;
        }
    }}}
    
    return false;
}

// version for screen-space visibility
static bool is_model_visible(const Lum::MeshModel& model, const Lum::MeshTransform& trans, const glm::mat4& viewProjMatrix) {
    // Calculate the model's bounding box after applying the transform
    glm::ivec3 modelSize = model.getSize();
    glm::vec3 minCorner = glm::vec3(0.0f); // Least corner before transform
    glm::vec3 maxCorner = glm::vec3(modelSize); // Max corner before transform

    // Transform the corners
    std::array<glm::vec3,8> transformedCorners = {};
    int index = 0;
    for (int x = 0; x <= 1; ++x) {
        for (int y = 0; y <= 1; ++y) {
            for (int z = 0; z <= 1; ++z) {
                glm::vec3 corner = glm::vec3(x, y, z) * maxCorner + minCorner;
                transformedCorners[index++] = trans.rot * corner + trans.shift;
            }
        }
    }

    // Check if any corner is within the screen bounds
    for (let corner : transformedCorners) {
        glm::vec4 clip = viewProjMatrix * glm::vec4(corner, 1.0f);

        // Perspective divide (to convert from clip space to NDC)
        // NOTE: i have no idea if it actually helps. TODO:
        if (clip.w != 0.0f) {
            clip /= clip.w;
        }

        // Check if the point lies within the NDC range
        // i guess i can use GLM for simd but its not bottleneck for now
        // TODO: asm view to imrpove every fun
        if ((clip.x >= -1.0f) && (clip.y >= -1.0f) && (clip.z >= -1.0f) &&
            (clip.x <= +1.0f) && (clip.y <= +1.0f) && (clip.z <= +1.0f)) {
            return true;
        }
    }
    return false;
}

void Lum::Renderer::drawWorld() noexcept {
    PROFILE_TIMESTAMP;
    // block_que.clear(); done in startFrame
    // cam_dist could be moved into update()
    // LOG(opaque_members->settings.world_size.x)
    // LOG(opaque_members->settings.world_size.y)
    // LOG(opaque_members->settings.world_size.z)
    for(int xx=0; xx < opaque_members->settings.world_size.x; xx++){
    for(int yy=0; yy < opaque_members->settings.world_size.y; yy++){
    for(int zz=0; zz < opaque_members->settings.world_size.z; zz++){
        MeshModel* block_mesh = NULL;
TRACE()
        int block_id = opaque_members->render.origin_world(xx,yy,zz);
TRACE()
        if(block_id != 0){
            struct BlockRenderRequest /*goes*/ brr = {}; //
            brr.pos = ivec3(xx*16,yy*16, zz*16);
            brr.block = block_id;

            // vec3 clip_coords = (render.camera.cameraTransform * vec4(brr.pos,1));
            //     clip_coords.z = -clip_coords.z   ;

            // brr.cam_dist = clip_coords.z;

TRACE()
            if(is_block_visible(opaque_members->render.camera.cameraTransform, dvec3(brr.pos))){
                block_que.push_back(brr);
TRACE()
            }
TRACE()
        }
TRACE()
        assert(block_id <= opaque_members->render.static_block_palette_size && "there is block in the world that is outside of the palette");
    }}}
}

void Lum::Renderer::drawParticles() noexcept {
    // nothing here. Particles are not worth sorting
}

// world visibility. Turns out, screen-space is better, but gotta keep this one
static bool isModelVisible(const Lum::MeshModel& model, const Lum::MeshTransform& trans, const glm::dvec3& worldSize) {
    PROFILE_TIMESTAMP;
    // Calculate the model's bounding box after applying the transform
    glm::ivec3 modelSize = model.getSize();
    glm::vec3 minCorner = glm::vec3(0.0f); // Least corner before transform
    glm::vec3 maxCorner = glm::vec3(modelSize); // Max corner before transform

    // Transform the corners
    glm::vec3 transformedCorners[8];
    int index = 0;
    for (int x = 0; x <= 1; ++x) {
        for (int y = 0; y <= 1; ++y) {
            for (int z = 0; z <= 1; ++z) {
                glm::vec3 corner = glm::vec3(x, y, z) * maxCorner + minCorner;
                transformedCorners[index++] = trans.rot * corner + trans.shift;
            }
        }
    }

    // Check if any corner is inside the world bounds
    for (const auto& corner : transformedCorners) {
        if (corner.x >= 0.0 && corner.y >= 0.0 && corner.z >= 0.0 &&
            corner.x <= worldSize.x && corner.y <= worldSize.y && corner.z <= worldSize.z) {
            return true;
        }
    }
    return false;
}

void Lum::Renderer::drawModel(const MeshModel& mesh, const MeshTransform& trans) noexcept {
    PROFILE_TIMESTAMP;
    assert(mesh.ptr != nullptr);
    
    // if (isModelVisible(mesh, trans, dvec3(getWorldSize())*16.0)) {
    if (is_model_visible(mesh, trans, opaque_members->render.camera.cameraTransform)) {
        ModelRenderRequest 
            mrr = {};
            mrr.mesh = mesh;
            mrr.trans = trans;
        mesh_que.push_back(mrr);
    }
}

void Lum::Renderer::drawFoliageBlock(const MeshFoliage& foliage, const ivec3& pos) noexcept {
    assert(foliage != nullptr);
    FoliageRenderRequest 
        frr = {};
        frr.foliage = foliage;
        frr.pos = pos;
TRACE()
    FoliageRenderRequest t = frr;
TRACE()
//     LOG(&foliage_que)
//     LOG(foliage_que.data())
    foliage_que.push_back(t);
TRACE()
}
void Lum::Renderer::drawLiquidBlock(const MeshLiquid& liquid, const ivec3& pos) noexcept {
    assert(liquid != nullptr);
    LiquidRenderRequest 
        lrr = {};
        lrr.liquid = liquid;
        lrr.pos = pos;
    liquid_que.push_back(lrr);
}
void Lum::Renderer::drawVolumetricBlock(const MeshVolumetric& volumetric, const ivec3& pos) noexcept {
    assert(volumetric != nullptr);
    VolumetricRenderRequest 
        vrr = {};
        vrr.volumetric = volumetric;
        vrr.pos = pos;
    volumetric_que.push_back(vrr);
}

// clears vectors so you can fill them again
void Lum::Renderer::startFrame() noexcept {
    block_que.clear();
    mesh_que.clear();
    foliage_que.clear();
    liquid_que.clear();
    volumetric_que.clear();
}

// nevery knew std::sort is allowed to segfault on wrong __comp. Why everything has exceptions except for things that need it?
template <typename RenderRequestType, typename Container>
void calculateAndSortByCamDist(Container& render_queue, const glm::dmat4& cameraTransform) {
    // calculate cam_dist for each render request
    for (let rrequest : render_queue) {
        glm::dvec3 clip_coords = glm::dvec3(cameraTransform * glm::dvec4(rrequest.pos, 1.0));
        rrequest.cam_dist = -clip_coords.z; // set cam_dist to negative z cause i never get it right (depth sorting)
    }

    // sort render requests by cam_dist in descending order
    auto sortByCamDist = [](const RenderRequestType& lhs, const RenderRequestType& rhs) {
        return lhs.cam_dist > rhs.cam_dist;
    };

    std::sort(render_queue.begin(), render_queue.end(), sortByCamDist);
}

void Lum::Renderer::prepareFrame() noexcept{
    glfwPollEvents();
    should_close |= glfwWindowShouldClose(opaque_members->render.lumal.window.pointer);
    updateTime();
    opaque_members->render.deltaTime = delta_time;

    // separate for models just cause not worth template
    auto cam = opaque_members->render.camera.cameraTransform;
    auto sortMeshByCamDist = [](const struct ModelRenderRequest& lhs, const struct ModelRenderRequest& rhs) {
        return lhs.cam_dist > rhs.cam_dist;
    };
    for(let mrr: mesh_que){
        dvec3 clip_coords = (cam * dvec4(mrr.trans.shift,1));
            clip_coords.z = -clip_coords.z;
        mrr.cam_dist = clip_coords.z;
    } std::sort(mesh_que.begin(), mesh_que.end(), sortMeshByCamDist); 

    calculateAndSortByCamDist<BlockRenderRequest>(block_que, cam);
    calculateAndSortByCamDist<FoliageRenderRequest>(foliage_que, cam);
    calculateAndSortByCamDist<LiquidRenderRequest>(liquid_que, cam);
    calculateAndSortByCamDist<VolumetricRenderRequest>(volumetric_que, cam);
}

void Lum::Renderer::submitFrame() noexcept {
TRACE()
    opaque_members->render.start_frame();
TRACE()

        // opaque_members->render.start_compute();
            opaque_members->render.start_blockify();    
            for (let m : mesh_que){
                opaque_members->render.blockify_mesh((LumInternal::InternalMeshModel*)m.mesh.ptr, &m.trans);
            }
TRACE()

            opaque_members->render.end_blockify();
TRACE()
            opaque_members->render.update_radiance();
TRACE()
            // opaque_members->render.recalculate_df(); // currently unused. per-voxel Distance Field 
            // opaque_members->render.recalculate_bit(); // currently unused. Bitpacking, like (block==Air) ? 0 : 1 
TRACE()
            opaque_members->render.updade_grass({});
TRACE()
            opaque_members->render.updade_water();
TRACE()
            opaque_members->render.exec_copies();
TRACE()
                opaque_members->render.start_map();
TRACE()
                for (let m : mesh_que){
                    opaque_members->render.map_mesh((LumInternal::InternalMeshModel*)m.mesh.ptr, &m.trans);
                }
TRACE()
                opaque_members->render.end_map();
TRACE()
            opaque_members->render.end_compute();
TRACE()
                // opaque_members->render.raytrace();
                opaque_members->render.start_lightmap();
TRACE()
                //yeah its wrong
                opaque_members->render.lightmap_start_blocks();
TRACE()
                    for(let b : block_que){
                        opaque_members->render.lightmap_block(&opaque_members->block_palette[b.block].mesh, b.block, b.pos);
                    }
TRACE()
                opaque_members->render.lightmap_start_models();
TRACE()
                    for (let m : mesh_que){
                        opaque_members->render.lightmap_model((LumInternal::InternalMeshModel*)m.mesh.ptr, &m.trans);
                    }
TRACE()
                opaque_members->render.end_lightmap();
TRACE()

                opaque_members->render.start_raygen();  
TRACE()
                // printl(block_que.size());
                opaque_members->render.raygen_start_blocks();
TRACE()
                    for(let b : block_que){
                        // DEBUG_LOG(b.block)
                        // DEBUG_LOG(&block_palette[b.block].mesh)
                        opaque_members->render.raygen_block(&opaque_members->block_palette[b.block].mesh, b.block, b.pos);
                    }  
TRACE()
                    
                opaque_members->render.raygen_start_models();
TRACE()
                    for (let m : mesh_que){
                        opaque_members->render.raygen_model((LumInternal::InternalMeshModel*)m.mesh.ptr, &m.trans);
                    }
TRACE()
                opaque_members->render.update_particles();
TRACE()
                opaque_members->render.raygen_map_particles();      
TRACE()
                opaque_members->render.raygen_start_grass();
TRACE()
                    for(let f : foliage_que){
                        opaque_members->render.raygen_map_grass((LumInternal::InternalMeshFoliage*)f.foliage, f.pos);
                    }
TRACE()

                opaque_members->render.raygen_start_water();
TRACE()
                    for(let l : liquid_que){
                        opaque_members->render.raygen_map_water(*((LumInternal::InternalMeshLiquid*)(l.liquid)), l.pos);
                    }
TRACE()
                opaque_members->render.end_raygen();
TRACE()
                opaque_members->render.start_2nd_spass();
TRACE()
                opaque_members->render.diffuse();
TRACE()
                opaque_members->render.ambient_occlusion(); 
TRACE()
                opaque_members->render.glossy_raygen();
TRACE()
                opaque_members->render.raygen_start_smoke();
TRACE()
                    for(let v : volumetric_que){
                        opaque_members->render.raygen_map_smoke(*((LumInternal::InternalMeshVolumetric*)(v.volumetric)), v.pos);
                    }
TRACE()
                opaque_members->render.glossy();
TRACE()
                opaque_members->render.smoke();
TRACE()
                opaque_members->render.tonemap();
TRACE()
            opaque_members->render.start_ui(); 
//                 ui.update();
TRACE()
//                 ui.draw();
TRACE()
        opaque_members->render.end_ui(); 
        opaque_members->render.end_2nd_spass();
TRACE()
    opaque_members->render.end_frame();
TRACE()
}

void Lum::Renderer::cleanup() noexcept {
    waitIdle();
TRACE();
    for(int i=1; i < opaque_members->render.static_block_palette_size; i++){
        // frees the mesh buffers and voxel images
        opaque_members->render.free_block(&opaque_members->block_palette[i]);
    }
    waitIdle();
TRACE();
    for(int i=0; i < opaque_members->mesh_models_storage.total_size(); i++){
        bool idx_is_free = opaque_members->mesh_models_storage.free_indices.contains(i);
        bool is_allocated = (not idx_is_free);
        if(is_allocated) {
            LumInternal::InternalMeshModel* imm_ptr = &opaque_members->mesh_models_storage.storage[i];
            // LOG(imm_ptr)
            opaque_members->render.free_mesh(imm_ptr);
            // no free cause not needed and will actually make referenced memory illegal
            // mesh_models_storage.free(imm_ptr);
        }
    }
    waitIdle();
TRACE();
    opaque_members->render.cleanup();
TRACE();
}

void Lum::Renderer::waitIdle() noexcept {
    opaque_members->render.lumal.deviceWaitIdle();
}

void* Lum::Renderer::getGLFWptr() const noexcept {
    return opaque_members->render.lumal.window.pointer;
}

void Lum::Renderer::updateTime() noexcept {
    auto now = std::chrono::steady_clock::now();
    delta_time = std::chrono::duration<double>(now - curr_time).count();
    curr_time = now;
}

Lum::Camera& Lum::Renderer::getCamera(){
    return opaque_members->render.camera;
}

glm::ivec3 Lum::Renderer::getWorldSize(){
    return opaque_members->render.world_size;
}

void Lum::Renderer::addParticle(Particle particle){
    opaque_members->render.particles.push_back(particle);
}

glm::ivec3 Lum::MeshModelWithGetters::getSize() const {
    LumInternal::InternalMeshModel* ptr = (LumInternal::InternalMeshModel*) this->ptr;
    return ptr->size;
}
Lum::MeshModelWithGetters::operator void* () {
    return ptr;
}