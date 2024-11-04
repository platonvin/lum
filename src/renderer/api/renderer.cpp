#include "renderer.hpp"
#include "defines/macros.hpp"

#define let auto&

void Lum::Renderer::init(LumInternal::LumSettings lum_settings) noexcept {
    // LumSettings lum_settings(settings); 
        lum_settings.debug = false; //Validation Layers. Use them while developing or be tricked into thinking that your code is correct
        lum_settings.timestampCount = 48;
        lum_settings.profile = true; //monitors perfomance via timestamps. You can place one with PLACE_TIMESTAMP() macro
        // currently fif has bug in it, do not change for now 
        lum_settings.fif = LumInternal::MAX_FRAMES_IN_FLIGHT; // Frames In Flight. If 1, then record cmdbuff and submit it. If multiple, cpu will (might) be ahead of gpu by FIF-1, which makes GPU wait less

        lum_settings.deviceFeatures.vertexPipelineStoresAndAtomics = VK_TRUE;
        lum_settings.deviceFeatures.fragmentStoresAndAtomics = VK_TRUE;
        lum_settings.deviceFeatures.geometryShader = VK_TRUE;
        lum_settings.deviceFeatures.independentBlend = VK_TRUE;
        lum_settings.deviceFeatures.shaderInt16 = VK_TRUE;
        lum_settings.deviceFeatures11.storagePushConstant16 = VK_TRUE;
        lum_settings.deviceFeatures12.storagePushConstant8 = VK_TRUE;
        lum_settings.deviceFeatures12.shaderInt8 = VK_TRUE;
        lum_settings.deviceFeatures12.storageBuffer8BitAccess = VK_TRUE;
        // for assumeEXT, currently disabled and will be brought back with new shader system
        // VkPhysicalDeviceShaderExpectAssumeFeaturesKHR 
        //     expect;
        //     expect.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_EXPECT_ASSUME_FEATURES_KHR;
        //     expect.shaderExpectAssume = VK_TRUE;
        // settings.addExtraFeaturesPnext(&expect);
        
        lum_settings.deviceExtensions.push_back(VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME); //simplifies renderer for negative cost lol
        lum_settings.deviceExtensions.push_back(VK_KHR_SHADER_FLOAT16_INT8_EXTENSION_NAME); //pco / ubo +perfomance
        lum_settings.deviceExtensions.push_back(VK_KHR_16BIT_STORAGE_EXTENSION_NAME); //just explicit control
        lum_settings.deviceExtensions.push_back(VK_KHR_8BIT_STORAGE_EXTENSION_NAME); //just explicit control
        lum_settings.deviceExtensions.push_back(VK_EXT_SCALAR_BLOCK_LAYOUT_EXTENSION_NAME); // aka zero-pain buffers
        // settings.deviceExtensions.push_back(VK_KHR_SHADER_EXPECT_ASSUME_EXTENSION_NAME); // GLSL's assumeEXT(statement)

    settings = lum_settings;
    render.init(lum_settings);
        vkDeviceWaitIdle(render.render.device);

    should_close = false; // just in case, idk
}

void Lum::Renderer::loadWorld(const LumInternal::BlockID_t* world_data) noexcept {
    assert(world_data);
    // TODO
    render.load_scene("assets/scene");
}
void Lum::Renderer::loadWorld(const std::string& file) noexcept {
    // TODO
    render.load_scene(file.c_str());
}

void Lum::Renderer::setWorldBlock(const ivec3& pos, MeshBlock block) noexcept {
    // yep, CPU-side structure
    // it is copied to gpu every frame for internal reasons
    // in some sence, this is a cache
    render.origin_world(pos) = block;
}
void Lum::Renderer::setWorldBlock(int x, int y, int z, MeshBlock block) noexcept {
    render.origin_world(x,y,z) = block;
}

LumInternal::BlockID_t Lum::Renderer::getWorldBlock(const ivec3& pos) const noexcept {
    LumInternal::BlockID_t block = render.origin_world(pos);
    return block;
}
LumInternal::BlockID_t Lum::Renderer::getWorldBlock(int x, int y, int z) const noexcept {
    LumInternal::BlockID_t block = render.origin_world(x,y,z);
    return block;  
}

void Lum::Renderer::loadBlock(MeshBlock block, LumInternal::BlockWithMesh* block_data) noexcept {
    memcpy(&block_palette[block], block_data, sizeof(LumInternal::BlockWithMesh));
    // render.updateMaterialPalette(mat_palette);
}
void Lum::Renderer::loadBlock(MeshBlock block, const std::string& file) noexcept {
    render.load_block(&block_palette[block], file.c_str());
}

void Lum::Renderer::uploadBlockPaletteToGPU() noexcept {
    render.updateBlockPalette(block_palette.data());
}

void Lum::Renderer::uploadMaterialPaletteToGPU() noexcept {
    render.updateMaterialPalette(mat_palette.data());
}
void Lum::Renderer::loadPalette(const std::string& file) noexcept{
    render.extract_palette(file.c_str(), mat_palette.data());
}
Lum::MeshModel Lum::Renderer::loadModel(const std::string& file, bool try_extract_palette) noexcept {
    LumInternal::InternalMeshModel* new_mesh = mesh_models_storage.allocate();
    render.load_mesh(new_mesh, file.c_str(), true, /*extract palette if no found = */ try_extract_palette, mat_palette.data());
    return new_mesh;
}
Lum::MeshModel Lum::Renderer::loadModel(LumInternal::Voxel* mesh_data, int x_size, int y_size, int z_size) noexcept {
    LumInternal::InternalMeshModel* new_mesh = mesh_models_storage.allocate();
    render.load_mesh(new_mesh, mesh_data, x_size,y_size,z_size, true);
    return new_mesh;
}
void Lum::Renderer::freeMesh(MeshModel model) noexcept {
    LumInternal::InternalMeshModel* mesh_ptr = model;
    render.free_mesh(mesh_ptr);
}

Lum::MeshFoliage Lum::Renderer::loadFoliage(const std::string& vertex_shader_file, int vertices_per_foliage, int density) noexcept {
    LumInternal::InternalMeshFoliage* 
        new_foliage = mesh_foliage_storage.allocate();
        // this is not creating pipe(line). Only queuing shader file for later pipe(line)
        // i just hope C++ is not fucked up and lifetime of string is whole main
        new_foliage->vertex_shader_file = vertex_shader_file;
        // LOG(new_foliage->vertex_shader_file)
        new_foliage->vertices = vertices_per_foliage;
        new_foliage->dencity = density;
    // registering in internal renderer
    render.registered_foliage.push_back(new_foliage);
    return new_foliage;
}
Lum::MeshVolumetric Lum::Renderer::loadVolumetric(float max_density, float density_deviation, const u8vec3& color) noexcept {
    LumInternal::InternalMeshVolumetric* 
        new_volumetric = mesh_volumetric_storage.allocate();
        new_volumetric->max_dencity = max_density;
        new_volumetric->variation = density_deviation;
        new_volumetric->color = color;
    return new_volumetric;
}
Lum::MeshLiquid Lum::Renderer::loadLiquid(LumInternal::MatID_t main_mat, LumInternal::MatID_t foam_mat) noexcept {
    LumInternal::InternalMeshLiquid* 
        new_liquid = mesh_liquid_storage.allocate();
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

void Lum::Renderer::drawWorld() noexcept {
    // block_que.clear(); done in startFrame
    // cam_dist could be moved into update()
    for(int xx=0; xx<settings.world_size.x; xx++){
    for(int yy=0; yy<settings.world_size.y; yy++){
    for(int zz=0; zz<settings.world_size.z; zz++){
        MeshModel* block_mesh = NULL;
        
        int block_id = render.origin_world(xx,yy,zz);
        if(block_id != 0){
            struct BlockRenderRequest /*goes*/ brr = {}; //
            brr.pos = ivec3(xx*16,yy*16, zz*16);
            brr.block = block_id;

            // vec3 clip_coords = (render.camera.cameraTransform * vec4(brr.pos,1));
            //     clip_coords.z = -clip_coords.z;

            // brr.cam_dist = clip_coords.z;

            if(is_block_visible(render.camera.cameraTransform, dvec3(brr.pos))){
                block_que.push_back(brr);
            }
        }
        assert(block_id <= render.static_block_palette_size && "there is block in the world that is outside of the palette");
    }}}
}

void Lum::Renderer::drawParticles() noexcept {
    // nothing here. Particles are not worth sorting
}

void Lum::Renderer::drawModel(const MeshModel& mesh, const MeshTransform& trans) noexcept {
    assert(mesh != nullptr);
    ModelRenderRequest 
        mrr = {};
        mrr.mesh = mesh;
        mrr.trans = trans;
    mesh_que.push_back(mrr);
}

void Lum::Renderer::drawFoliageBlock(const MeshFoliage& foliage, const ivec3& pos) noexcept {
    assert(foliage != nullptr);
    FoliageRenderRequest 
        frr = {};
        frr.foliage = foliage;
        frr.pos = pos;
    foliage_que.push_back(frr);
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
    should_close |= glfwWindowShouldClose(render.render.window.pointer);
    updateTime();
    render.deltaTime = delta_time;

    // separate for models just cause not worth template
    auto cam = render.camera.cameraTransform;
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
// ATRACE()
    render.start_frame();
// ATRACE()

        // render.start_compute();
            render.start_blockify();    
            for (let m : mesh_que){
                render.blockify_mesh(m.mesh, &m.trans);
            }
// ATRACE()

            render.end_blockify();
            render.update_radiance();
            // render.recalculate_df(); // currently unused. per-voxel Distance Field 
            // render.recalculate_bit(); // currently unused. Bitpacking, like (block==Air) ? 0 : 1 
            render.updade_grass({});
            render.updade_water();
            render.exec_copies();
                render.start_map();
                for (let m : mesh_que){
                    render.map_mesh(m.mesh, &m.trans);
                }
                render.end_map();
// ATRACE()
            render.end_compute();
// ATRACE()
                // render.raytrace();
                render.start_lightmap();
// ATRACE()
                //yeah its wrong
                render.lightmap_start_blocks();
// ATRACE()
                    for(let b : block_que){
                        render.lightmap_block(&block_palette[b.block].mesh, b.block, b.pos);
                    }
// ATRACE()
                render.lightmap_start_models();
// ATRACE()
                    for (let m : mesh_que){
                        render.lightmap_model(m.mesh, &m.trans);
                    }
// ATRACE()
                render.end_lightmap();
// ATRACE()

                render.start_raygen();  
// ATRACE()
                // printl(block_que.size());
                render.raygen_start_blocks();
// ATRACE()
                    for(let b : block_que){
                        // DEBUG_LOG(b.block)
                        // DEBUG_LOG(&block_palette[b.block].mesh)
                        render.raygen_block(&block_palette[b.block].mesh, b.block, b.pos);
                    }  
// ATRACE()
                    
                render.raygen_start_models();
// ATRACE()
                    for (let m : mesh_que){
                        render.raygen_model(m.mesh, &m.trans);
                    }
// ATRACE()
                render.update_particles();
// ATRACE()
                render.raygen_map_particles();      
// ATRACE()
                render.raygen_start_grass();
// ATRACE()
                    for(let f : foliage_que){
                        render.raygen_map_grass(f.foliage, f.pos);
                    }
// ATRACE()

                render.raygen_start_water();
// ATRACE()
                    for(let l : liquid_que){
                        render.raygen_map_water(*(l.liquid), l.pos);
                    }
// ATRACE()
                render.end_raygen();
// ATRACE()
                render.start_2nd_spass();
// ATRACE()
                render.diffuse();
// ATRACE()
                render.ambient_occlusion(); 
// ATRACE()
                render.glossy_raygen();
// ATRACE()
                render.raygen_start_smoke();
// ATRACE()
                    for(let v : volumetric_que){
                        render.raygen_map_smoke(*v.volumetric, v.pos);
                    }
// ATRACE()
                render.glossy();
// ATRACE()
                render.smoke();
// ATRACE()
                render.tonemap();
// ATRACE()
            render.start_ui(); 
//                 ui.update();
// ATRACE()
//                 ui.draw();
// ATRACE()
        render.end_ui(); 
        render.end_2nd_spass();
// ATRACE()
    render.end_frame();
// ATRACE()
}

void Lum::Renderer::cleanup() noexcept {
    waitIdle();
// ATRACE();
    for(int i=1; i<render.static_block_palette_size; i++){
        // frees the mesh buffers and voxel images
        render.free_block(&block_palette[i]);
    }
    waitIdle();
// ATRACE();
    for(int i=0; i<mesh_models_storage.total_size(); i++){
        bool idx_is_free = mesh_models_storage.free_indices.contains(i);
        bool is_allocated = (not idx_is_free);
        if(is_allocated) {
            LumInternal::InternalMeshModel* imm_ptr = &mesh_models_storage.storage[i];
            // LOG(imm_ptr)
            render.free_mesh(imm_ptr);
            // no free cause not needed and will actually make referenced memory illegal
            // mesh_models_storage.free(imm_ptr);
        }
    }
    waitIdle();
// ATRACE();
    render.cleanup();
// ATRACE();
}

void Lum::Renderer::waitIdle() noexcept {
    render.render.deviceWaitIdle();
}

GLFWwindow* Lum::Renderer::getGLFWptr() const noexcept {
    return render.render.window.pointer;
}
