#include "lum.hpp"
#include "defines/macros.hpp"

#define let auto&

void Lum::init() {
    settings.debug = false; //Validation Layers. Use them while developing or be tricked into thinking that your code is correct
    settings.timestampCount = 48;
    settings.profile = true; //monitors perfomance via timestamps. You can place one with PLACE_TIMESTAMP() macro
    // currently fif has bug in it, do not change for now 
    settings.fif = MAX_FRAMES_IN_FLIGHT; // Frames In Flight. If 1, then record cmdbuff and submit it. If multiple, cpu will (might) be ahead of gpu by FIF-1, which makes GPU wait less

    settings.deviceFeatures.vertexPipelineStoresAndAtomics = VK_TRUE;
    settings.deviceFeatures.fragmentStoresAndAtomics = VK_TRUE;
    settings.deviceFeatures.geometryShader = VK_TRUE;
    settings.deviceFeatures.independentBlend = VK_TRUE;
    settings.deviceFeatures.shaderInt16 = VK_TRUE;
    settings.deviceFeatures11.storagePushConstant16 = VK_TRUE;
    settings.deviceFeatures12.storagePushConstant8 = VK_TRUE;
    settings.deviceFeatures12.shaderInt8 = VK_TRUE;
    settings.deviceFeatures12.storageBuffer8BitAccess = VK_TRUE;
    // for assumeEXT, currently disabled and will be brought back with new shader system
    // VkPhysicalDeviceShaderExpectAssumeFeaturesKHR 
    //     expect;
    //     expect.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_SHADER_EXPECT_ASSUME_FEATURES_KHR;
    //     expect.shaderExpectAssume = VK_TRUE;
    // settings.addExtraFeaturesPnext(&expect);
    
    settings.deviceExtensions.push_back(VK_KHR_PUSH_DESCRIPTOR_EXTENSION_NAME); //simplifies renderer for negative cost lol
    settings.deviceExtensions.push_back(VK_KHR_SHADER_FLOAT16_INT8_EXTENSION_NAME); //pco / ubo +perfomance
    settings.deviceExtensions.push_back(VK_KHR_16BIT_STORAGE_EXTENSION_NAME); //just explicit control
    settings.deviceExtensions.push_back(VK_KHR_8BIT_STORAGE_EXTENSION_NAME); //just explicit control
    settings.deviceExtensions.push_back(VK_EXT_SCALAR_BLOCK_LAYOUT_EXTENSION_NAME); // aka zero-pain buffers
    // settings.deviceExtensions.push_back(VK_KHR_SHADER_EXPECT_ASSUME_EXTENSION_NAME); // GLSL's assumeEXT(statement)

    render.init(settings);
        vkDeviceWaitIdle(render.render.device);
    render.load_scene("assets/scene");

    should_close = false; // just in case, idk
}

void Lum::loadWorld(BlockID_t* world_data) {
    assert(world_data);
    // TODO
    render.load_scene("assets/scene");
}
void Lum::loadWorld(const char* file) {
    // TODO
    render.load_scene(file);
}

void Lum::setWorldBlock(ivec3 pos, BlockID_t block) {
    // yep, CPU-side structure
    // it is copied to gpu every frame for internal reasons
    // in some sence, this is a cache
    render.origin_world(pos) = block;
}
void Lum::setWorldBlock(int x, int y, int z, MeshBlock block) {
    render.origin_world(x,y,z) = block;
}

BlockID_t Lum::getWorldBlock(ivec3 pos) {
    BlockID_t block = render.origin_world(pos);
    return block;
}
BlockID_t Lum::getWorldBlock(int x, int y, int z) {
    BlockID_t block = render.origin_world(x,y,z);
    return block;  
}

void Lum::loadBlock(BlockID_t block, BlockWithMesh* block_data) {
    memcpy(&block_palette[block], block_data, sizeof(BlockWithMesh));
    // render.updateMaterialPalette(mat_palette);
}

void Lum::loadBlock(BlockID_t block, const char* file) {
    render.load_block(&block_palette[block], file);
}

void Lum::uploadBlockPaletteToGPU() {
    render.updateBlockPalette(block_palette.data());
}

void Lum::uploadMaterialPaletteToGPU() {
    render.updateMaterialPalette(mat_palette);
}

MeshModel Lum::loadMesh(const char* file, bool try_extract_palette) {
    InternalMeshModel* new_mesh = mesh_models_storage.allocate();
    render.load_mesh(new_mesh, file, true, /*extract palette if no found = */ try_extract_palette, mat_palette);
    return new_mesh;
}
MeshModel Lum::loadMesh(Voxel* data, int x_size, int y_size, int z_size) {
    InternalMeshModel* new_mesh = mesh_models_storage.allocate();
    render.load_mesh(new_mesh, data, x_size,y_size,z_size, true);
    return new_mesh;
}
void Lum::freeMesh(MeshModel model) {
    InternalMeshModel* mesh_ptr = model;
    render.free_mesh(mesh_ptr);
}

MeshFoliage Lum::loadFoliage(const char* vertex_shader_file, int vertices_per_foliage, int dencity){
    MeshFoliage 
        foliage = {};
        foliage.vertices = vertices_per_foliage;
        foliage.dencity = dencity;
    return foliage;
}

MeshVolumetric Lum::loadVolumetric(float max_dencity, float dencity_deviation, u8vec3 color) {
    MeshVolumetric 
        volumetric = {};
        volumetric.max_dencity = max_dencity;
        volumetric.variation = dencity_deviation;
        volumetric.color = color;
    return volumetric;
}
MeshLiquid Lum::loadLiquid(MatID_t main_mat, MatID_t foam_mat) {
    MeshLiquid 
        liquid = {};
        liquid.main = main_mat;
        liquid.foam = foam_mat;
    return liquid;
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

void Lum::drawWorld() {
    // block_que.clear(); done in startFrame
    // cam_dist could be moved into update()
    for(int xx=0; xx<settings.world_size.x; xx++){
    for(int yy=0; yy<settings.world_size.y; yy++){
    for(int zz=0; zz<settings.world_size.z; zz++){
        MeshModel* block_mesh = NULL;
        
        int block_id = render.origin_world(xx,yy,zz);
        if(block_id != 0){
            struct block_render_request /*goes*/ brr = {}; //
            brr.pos = ivec3(xx*16,yy*16, zz*16);
            brr.block = block_id;

            vec3 clip_coords = (render.camera.cameraTransform * vec4(brr.pos,1));
                clip_coords.z = -clip_coords.z;

            brr.cam_dist = clip_coords.z;

            if(is_block_visible(render.camera.cameraTransform, dvec3(brr.pos))){
                block_que.push_back(brr);
            }
        }
        assert(block_id <= render.static_block_palette_size && "there is block in the world that is outside of the palette");
    }}}
}

void Lum::drawParticles() {
    // nothing here. Particles are not worth sorting
}

void Lum::drawMesh(MeshModel mesh, MeshTransform* trans) {
    assert (mesh && trans);
    model_render_request 
        mrr = {};
        mrr.mesh = mesh;
        mrr.trans = *trans;
    mesh_que.push_back(mrr);
}

void Lum::drawFoliageBlock(MeshFoliage mesh, ivec3 pos){
    foliage_render_request 
        frr = {};
        frr.foliage = mesh;
        frr.pos = pos;
    foliage_que.push_back(frr);
}
void Lum::drawLiquidBlock(MeshLiquid mesh, ivec3 pos){
    liquid_render_request 
        lrr = {};
        lrr.pos = pos;
    liquid_que.push_back(lrr);
}
void Lum::drawVolumetricBlock(MeshVolumetric mesh, ivec3 pos){
    volumetric_render_request 
        vrr = {};
        vrr.pos = pos;
    volumetric_que.push_back(vrr);
}

// clears vectors so you can fill them again
void Lum::startFrame() {
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

void Lum::prepareFrame(){
    glfwPollEvents();
    should_close |= glfwWindowShouldClose(render.render.window.pointer);
    prev_time = curr_time;
    curr_time = glfwGetTime();
    delt_time = curr_time-prev_time;
    render.deltaTime = delt_time;

    // separate for models just cause not worth template
    auto cam = render.camera.cameraTransform;
    auto sortMeshByCamDist = [](const struct model_render_request& lhs, const struct model_render_request& rhs) {
        return lhs.cam_dist > rhs.cam_dist;
    };
    for(let mrr: mesh_que){
        dvec3 clip_coords = (cam * dvec4(mrr.trans.shift,1));
            clip_coords.z = -clip_coords.z;
        mrr.cam_dist = clip_coords.z;
    } std::sort(mesh_que.begin(), mesh_que.end(), sortMeshByCamDist); 

    calculateAndSortByCamDist<block_render_request>(block_que, cam);
    calculateAndSortByCamDist<foliage_render_request>(foliage_que, cam);
    calculateAndSortByCamDist<liquid_render_request>(liquid_que, cam);
    calculateAndSortByCamDist<volumetric_render_request>(volumetric_que, cam);
}

void Lum::submitFrame() {
    render.start_frame();

        // render.start_compute();
            render.start_blockify();    
            for (let m : mesh_que){
                render.blockify_mesh(m.mesh, &m.trans);
            }

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
            render.end_compute();
                // render.raytrace();
                render.start_lightmap();
                //yeah its wrong
                render.lightmap_start_blocks();
                    for(let b : block_que){
                        render.lightmap_block(&block_palette[b.block].mesh, b.block, b.pos);
                    }
                render.lightmap_start_models();
                    for (let m : mesh_que){
                        render.lightmap_model(m.mesh, &m.trans);
                    }
                render.end_lightmap();

                render.start_raygen();  
                // printl(block_que.size());
                render.raygen_start_blocks();
                    for(let b : block_que){
                        // DEBUG_LOG(b.block)
                        // DEBUG_LOG(&block_palette[b.block].mesh)
                        render.raygen_block(&block_palette[b.block].mesh, b.block, b.pos);
                    }  
                    
                render.raygen_start_models();
                    for (let m : mesh_que){
                        render.raygen_model(m.mesh, &m.trans);
                    }
                render.update_particles();
                render.raygen_map_particles();      
                render.raygen_start_grass();
                    for(let f : foliage_que){
                        render.raygen_map_grass(f.foliage, f.pos);
                    }

                render.raygen_start_water();
                    for(let l : liquid_que){
                        render.raygen_map_water(l.liquid, l.pos);
                    }
                render.end_raygen();
                render.start_2nd_spass();
                render.diffuse();
                render.ambient_occlusion(); 
                render.glossy_raygen();
                render.raygen_start_smoke();
                    for(let v : volumetric_que){
                        render.raygen_map_smoke(v.volumetric, v.pos);
                    }
                render.glossy();
                render.smoke();
                render.tonemap();
            render.start_ui(); 
//                 ui.update();
// TRACE();
//                 ui.draw();
        render.end_ui(); 
        render.end_2nd_spass();
    render.end_frame();
}

void Lum::cleanup() {
ATRACE();
    for(int i=1; i<render.static_block_palette_size; i++){
        // frees the mesh buffers and voxel images
        render.free_block(&block_palette[i]);
    }
ATRACE();
    render.cleanup();
}

void Lum::waitIdle() {
    render.render.deviceWaitIdle();
}

GLFWwindow* Lum::getGLFWptr(){
    return render.render.window.pointer;
}
