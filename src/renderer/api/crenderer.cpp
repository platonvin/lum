#include "crenderer.h"
#include "renderer.hpp"
#include "opaque_renderer_members.hpp"
#include "defines/macros.hpp"
#include <cstring>
// this file has to be compiled by C++ compiler

#ifdef __cplusplus
extern "C" {
#endif

LumRenderer lum_create_instance(const LumSettings* settings) {
    LumRenderer instance = nullptr;
    // allocate because opaque types thing
    instance = new Lum::Renderer();
    return instance;
}

void lum_destroy_instance(LumRenderer instance) {
    delete ((Lum::Renderer*)instance);
}

void lum_demo_load_scene(LumRenderer instance, const char* path){
    ((Lum::Renderer*)instance)->opaque_members->render.load_scene(path);
}

// Initialization and world management
void lum_init(LumRenderer instance, const LumSettings* settings) {
    Lum::Settings _settings = {};
    _settings.world_size.x = settings->world_size[0];
    _settings.world_size.y = settings->world_size[1];
    _settings.world_size.z = settings->world_size[2];
    _settings.static_block_palette_size = settings->static_block_palette_size;
    _settings.maxParticleCount = settings->maxParticleCount;
    _settings.timestampCount = settings->timestampCount;
    _settings.fif = settings->fif;
    _settings.vsync = settings->vsync;
    _settings.fullscreen = settings->fullscreen;
    _settings.debug = settings->debug;
    _settings.profile = settings->profile;
    ((Lum::Renderer*)instance)->init(_settings);
}

void lum_load_world(LumRenderer instance, const LumMeshBlock* world_data) {
    ((Lum::Renderer*)instance)->loadWorld(reinterpret_cast<const Lum::MeshBlock*>(world_data));
}

void lum_load_world_file(LumRenderer instance, const char* file) {
    ((Lum::Renderer*)instance)->loadWorld(std::string(file));
}

void lum_set_world_block(LumRenderer instance, int x, int y, int z, LumMeshBlock block) {
    ((Lum::Renderer*)instance)->setWorldBlock({x, y, z}, block);
}

LumMeshBlock lum_get_world_block(const LumRenderer instance, int x, int y, int z) {
    LumMeshBlock block = ((Lum::Renderer*)instance)->getWorldBlock({x, y, z});
    return block;
}

// Block loading and palette management
void lum_load_block(LumRenderer instance, LumMeshBlock block, void* block_data) {
    ((Lum::Renderer*)instance)->loadBlock(block, reinterpret_cast<LumInternal::BlockVoxels*>(block_data));
}

void lum_load_block_file(LumRenderer instance, LumMeshBlock block, const char* file) {
    // damn char* to string to char*
    ((Lum::Renderer*)instance)->loadBlock(block, std::string(file));
}

void lum_upload_block_palette_to_gpu(LumRenderer instance) {
    ((Lum::Renderer*)instance)->uploadBlockPaletteToGPU();
}

void lum_upload_material_palette_to_gpu(LumRenderer instance) {
    ((Lum::Renderer*)instance)->uploadMaterialPaletteToGPU();
}

// Mesh loading and management
LumMeshModel lum_load_mesh_file(LumRenderer instance, const char* file, bool try_extract_palette) {
    LumMeshModel model = ((Lum::Renderer*)instance)->loadModel(std::string(file), try_extract_palette);
    assert(model);
    return model;
}

LumMeshModel lum_load_mesh_data(LumRenderer instance, void* mesh_data, int x_size, int y_size, int z_size) {
    LumMeshModel model = ((Lum::Renderer*)instance)->loadModel(reinterpret_cast<LumInternal::Voxel*>(mesh_data), x_size, y_size, z_size);
    assert(model);
    return model;
}

void lum_free_mesh(LumRenderer instance, LumMeshModel model) {
    if (model) {
        ((Lum::Renderer*)instance)->freeMesh((LumInternal::InternalMeshModel*)(model));
    }
}

// Foliage, Volumetric, and Liquid loading
LumMeshFoliage lum_load_foliage(LumRenderer instance, const char* vertex_shader_file, int vertices_per_foliage, int density) {
    Lum::MeshFoliage new_foliage = ((Lum::Renderer*)instance)->loadFoliage(std::string(vertex_shader_file), vertices_per_foliage, density);
    assert(new_foliage);
    return new_foliage;
}

LumMeshVolumetric lum_load_volumetric(LumRenderer instance, float max_density, float density_deviation, const uint8_t color[3]) {
    // LumMeshVolumetric volumetric;
    Lum::MeshVolumetric new_v = ((Lum::Renderer*)instance)->loadVolumetric(max_density, density_deviation, {color[0], color[1], color[2]});
    assert(new_v);
    //propper conversion at least once
    return ((LumMeshVolumetric)((void*)(((Lum::MeshVolumetric*)new_v))));
}

LumMeshLiquid lum_load_liquid(LumRenderer instance, int main_mat, int foam_mat) {
    // LumMeshLiquid liquid;
    Lum::MeshLiquid new_l = ((Lum::Renderer*)instance)->loadLiquid(main_mat, foam_mat);
    assert(new_l);
    return new_l;
}

void lum_start_frame(LumRenderer instance) {
    ((Lum::Renderer*)instance)->startFrame();
}

void lum_draw_world(LumRenderer instance) {
    ((Lum::Renderer*)instance)->drawWorld();
}

void lum_draw_particles(LumRenderer instance) {
    ((Lum::Renderer*)instance)->drawParticles();
}

void lum_draw_model(LumRenderer instance, const LumMeshModel mesh, const LumMeshTransform* trans) {
    Lum::MeshTransform _trans = {};
    _trans.rot = {
        trans->rot[0],
        trans->rot[1],
        trans->rot[2],
        trans->rot[3],
    };
    _trans.shift[0] = trans->shift[0];
    _trans.shift[1] = trans->shift[1];
    _trans.shift[2] = trans->shift[2];
    ((Lum::Renderer*)instance)->drawModel((LumInternal::InternalMeshModel*) mesh, _trans);
}

void lum_draw_foliage_block(LumRenderer instance, const LumMeshFoliage foliage, const int pos[3]) {
    ((Lum::Renderer*)instance)->drawFoliageBlock((LumInternal::InternalMeshFoliage*)foliage, {pos[0], pos[1], pos[2]});
}

void lum_draw_liquid_block(LumRenderer instance, const LumMeshLiquid liquid, const int pos[3]) {
    ((Lum::Renderer*)instance)->drawLiquidBlock((LumInternal::InternalMeshLiquid*)liquid, {pos[0], pos[1], pos[2]});
}

void lum_draw_volumetric_block(LumRenderer instance, const LumMeshVolumetric volumetric, const int pos[3]) {
    ((Lum::Renderer*)instance)->drawVolumetricBlock(
        ((Lum::MeshVolumetric)  ((LumInternal::InternalMeshVolumetric*) (((void*)volumetric)))), 
        {pos[0], pos[1], pos[2]});
}

void lum_prepare_frame(LumRenderer instance) {
    ((Lum::Renderer*)instance)->prepareFrame();
}

void lum_submit_frame(LumRenderer instance) {
    ((Lum::Renderer*)instance)->submitFrame();
}

void lum_cleanup(LumRenderer instance) {
    ((Lum::Renderer*)instance)->cleanup();
    delete ((Lum::Renderer*)instance);
}

void* lum_get_glfw_ptr(const LumRenderer instance) {
    return (void*)((Lum::Renderer*)instance)->getGLFWptr();
}

bool lum_get_should_close(const LumRenderer instance) {
    return ((Lum::Renderer*)instance)->getShouldClose();
}
bool lum_set_should_close(const LumRenderer instance, bool should_close) {
    return ((Lum::Renderer*)instance)->setShouldClose(should_close);
}

void lum_wait_idle(LumRenderer instance) {
    ((Lum::Renderer*)instance)->waitIdle();
}

bool lum_is_profiling_enabled(const LumRenderer instance) {
    return ((Lum::Renderer*)instance)->opaque_members->render.lumal.settings.profile;
}

int lum_get_current_timestamp_count(const LumRenderer instance) {
    return ((Lum::Renderer*)instance)->opaque_members->render.lumal.currentTimestamp;
}

void lum_get_timestamp_name(const LumRenderer instance, int index, char* out_name, size_t name_length) {
    const char* name = ((Lum::Renderer*)instance)->opaque_members->render.lumal.timestampNames[index];
    strncpy(out_name, name, name_length - 1); // Ensure null termination
    out_name[name_length - 1] = '\0';         // Null-terminate in case of overflow
}

double lum_get_timestamp(const LumRenderer instance, int index) {
    return static_cast<double>(((Lum::Renderer*)instance)->opaque_members->render.lumal.average_ftimestamps[index]);
}

double lum_get_timestamp_difference(const LumRenderer instance, int index_start, int index_end) {
    const auto& timestamps = ((Lum::Renderer*)instance)->opaque_members->render.lumal.average_ftimestamps;
    return static_cast<double>(timestamps[index_end] - timestamps[index_start]);
}

#ifdef __cplusplus
} // extern "C"
#endif