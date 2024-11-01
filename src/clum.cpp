#include "clum.h"
#include "lum.hpp"
#include <cstring>

#ifdef __cplusplus
extern "C" {
#endif
// this file has to be compiled by C++ compiler

// Define opaque structs as C-compatible types pointing to C++ instances
struct LumRenderer {
    Lum::Renderer* instance;
};

//commented structures are ones that do not need pointer wrapper

// struct LumMeshModel {
//     Lum::MeshModel model;
// };

// struct LumMeshBlock {
//     Lum::MeshBlock block;
// };

struct LumMeshFoliage {
    Lum::MeshFoliage foliage;
};

// struct LumMeshLiquid {
//     Lum::MeshLiquid liquid;
// };

// struct LumMeshVolumetric {
//     Lum::MeshVolumetric volumetric;
// };

// LumInstance management
LumInstance* lum_create_instance(const LumSettings* settings) {
    LumInstance* instance = new LumInstance;
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
    instance->instance = new Lum::Renderer(_settings);
    return instance;
}

void lum_destroy_instance(LumInstance* instance) {
    delete instance->instance;
    delete instance;
}

void lum_demo_load_scene(LumInstance* instance, const char* path){
    instance->instance->render.load_scene(path);
}

// Initialization and world management
void lum_init(LumInstance* instance, const LumSettings* settings) {
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
    instance->instance->init(_settings);
}

void lum_load_world(LumInstance* instance, const LumMeshBlock* world_data) {
    instance->instance->loadWorld(reinterpret_cast<const Lum::MeshBlock*>(world_data));
}

void lum_load_world_file(LumInstance* instance, const char* file) {
    instance->instance->loadWorld(std::string(file));
}

void lum_set_world_block(LumInstance* instance, int x, int y, int z, LumMeshBlock block) {
    instance->instance->setWorldBlock({x, y, z}, block);
}

LumMeshBlock lum_get_world_block(const LumInstance* instance, int x, int y, int z) {
    LumMeshBlock block = instance->instance->getWorldBlock({x, y, z});
    return block;
}

// Block loading and palette management
void lum_load_block(LumInstance* instance, LumMeshBlock block, void* block_data) {
    instance->instance->loadBlock(block, reinterpret_cast<LumInternal::BlockWithMesh*>(block_data));
}

void lum_load_block_file(LumInstance* instance, LumMeshBlock block, const char* file) {
    // damn char* to string to char*
    instance->instance->loadBlock(block, std::string(file));
}

void lum_upload_block_palette_to_gpu(LumInstance* instance) {
    instance->instance->uploadBlockPaletteToGPU();
}

void lum_upload_material_palette_to_gpu(LumInstance* instance) {
    instance->instance->uploadMaterialPaletteToGPU();
}

// Mesh loading and management
LumMeshModel lum_load_mesh_file(LumInstance* instance, const char* file, bool try_extract_palette) {
    LumMeshModel model = instance->instance->loadMesh(std::string(file), try_extract_palette);
    return model;
}

LumMeshModel lum_load_mesh_data(LumInstance* instance, void* mesh_data, int x_size, int y_size, int z_size) {
    LumMeshModel model = instance->instance->loadMesh(reinterpret_cast<LumInternal::Voxel*>(mesh_data), x_size, y_size, z_size);
    return model;
}

void lum_free_mesh(LumInstance* instance, LumMeshModel model) {
    if (model) {
        instance->instance->freeMesh((LumInternal::InternalMeshModel*)(model));
    }
}

// Foliage, Volumetric, and Liquid loading
LumMeshFoliage* lum_load_foliage(LumInstance* instance, const char* vertex_shader_file, int vertices_per_foliage, int density) {
    LumMeshFoliage* foliage = new LumMeshFoliage;
    foliage->foliage = instance->instance->loadFoliage(std::string(vertex_shader_file), vertices_per_foliage, density);
    return foliage;
}

LumMeshVolumetric lum_load_volumetric(LumInstance* instance, float max_density, float density_deviation, const uint8_t color[3]) {
    LumMeshVolumetric volumetric;
    auto _v = instance->instance->loadVolumetric(max_density, density_deviation, {color[0], color[1], color[2]});
    volumetric.color[0] = _v.color.x;
    volumetric.color[1] = _v.color.y; 
    volumetric.color[2] = _v.color.z;
    volumetric.max_dencity = _v.max_dencity;
    volumetric.variation = _v.variation;
    return volumetric;
}

LumMeshLiquid lum_load_liquid(LumInstance* instance, int main_mat, int foam_mat) {
    LumMeshLiquid liquid;
    auto _l = instance->instance->loadLiquid(main_mat, foam_mat);
    liquid.foam = _l.foam;
    liquid.main = _l.main;
    return liquid;
}

void lum_start_frame(LumInstance* instance) {
    instance->instance->startFrame();
}

void lum_draw_world(LumInstance* instance) {
    instance->instance->drawWorld();
}

void lum_draw_particles(LumInstance* instance) {
    instance->instance->drawParticles();
}

void lum_draw_model(LumInstance* instance, const LumMeshModel mesh, const LumMeshTransform* trans) {
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
    instance->instance->drawModel((LumInternal::InternalMeshModel*) mesh, _trans);
}

void lum_draw_foliage_block(LumInstance* instance, const LumMeshFoliage* foliage, const int pos[3]) {
    instance->instance->drawFoliageBlock(foliage->foliage, {pos[0], pos[1], pos[2]});
}

void lum_draw_liquid_block(LumInstance* instance, const LumMeshLiquid liquid, const int pos[3]) {
    instance->instance->drawLiquidBlock({liquid.main, liquid.foam}, {pos[0], pos[1], pos[2]});
}

void lum_draw_volumetric_block(LumInstance* instance, const LumMeshVolumetric volumetric, const int pos[3]) {
    instance->instance->drawVolumetricBlock({
        volumetric.max_dencity, 
        volumetric.variation, 
            {
            volumetric.color[0],
            volumetric.color[1],
            volumetric.color[2],
            }
        }, 
        {pos[0], pos[1], pos[2]});
}

void lum_prepare_frame(LumInstance* instance) {
    instance->instance->prepareFrame();
}

void lum_submit_frame(LumInstance* instance) {
    instance->instance->submitFrame();
}

void lum_cleanup(LumInstance* instance) {
    instance->instance->cleanup();
}

GLFWwindow* lum_get_glfw_ptr(const LumInstance* instance) {
    return instance->instance->getGLFWptr();
}

bool lum_get_should_close(const LumInstance* instance) {
    return instance->instance->getShouldClose();
}
bool lum_set_should_close(const LumInstance* instance, bool should_close) {
    return instance->instance->setShouldClose(should_close);
}

void lum_wait_idle(LumInstance* instance) {
    instance->instance->waitIdle();
}

bool lum_is_profiling_enabled(const LumInstance* instance) {
    return instance->instance->render.render.settings.profile;
}

int lum_get_current_timestamp_count(const LumInstance* instance) {
    return instance->instance->render.render.currentTimestamp;
}

void lum_get_timestamp_name(const LumInstance* instance, int index, char* out_name, size_t name_length) {
    const char* name = instance->instance->render.render.timestampNames[index];
    strncpy(out_name, name, name_length - 1); // Ensure null termination
    out_name[name_length - 1] = '\0';         // Null-terminate in case of overflow
}

double lum_get_timestamp(const LumInstance* instance, int index) {
    return static_cast<double>(instance->instance->render.render.average_ftimestamps[index]);
}

double lum_get_timestamp_difference(const LumInstance* instance, int index_start, int index_end) {
    const auto& timestamps = instance->instance->render.render.average_ftimestamps;
    return static_cast<double>(timestamps[index_end] - timestamps[index_start]);
}

#ifdef __cplusplus
} // extern "C"
#endif