#pragma once
#ifndef LUM_C_BINDINGS_H
#define LUM_C_BINDINGS_H

/*
C99 API for Lum
it does get updates, but less often (on more stable versions)
also less strictly typed
*/

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// yypedefs for opaque pointers representing classes in Lum C++ API
typedef void* LumRenderer;
typedef void* LumMeshModel;
typedef int16_t LumMeshBlock; // to avoid pointer
typedef void* LumMeshFoliage;
typedef void* LumMeshLiquid;
typedef void* LumMeshVolumetric;
typedef void* LumMeshUi;

typedef struct {
    int world_size[3];
    int static_block_palette_size;
    int maxParticleCount;

    int timestampCount;
    int fif;
    char vsync;
    char fullscreen;
    char debug;
    char profile;
} LumSettings;
const LumSettings LUM_DEFAULT_SETTINGS = {
    {48, 48, 16},
    15,
    8128,
    1024,
    2,
    false,
    false,
    false,
    false,
};


typedef struct LumMeshTransform {
    float rot[4];   // rotation quaternion
    float shift[3]; // float because not necessarily snapped to grid
} LumMeshTransform;  // to avoid pointer

// Function prototypes
LumRenderer lum_create_instance(const LumSettings* settings);
void lum_destroy_instance(LumRenderer instance);

void lum_demo_load_scene(LumRenderer instance, const char* path);

void lum_init(LumRenderer instance, const LumSettings* settings);
void lum_load_world(LumRenderer instance, const LumMeshBlock* world_data);
void lum_load_world_file(LumRenderer instance, const char* file);

void lum_set_world_block(LumRenderer instance, int x, int y, int z, LumMeshBlock block);
LumMeshBlock lum_get_world_block(const LumRenderer instance, int x, int y, int z);

void lum_load_block(LumRenderer instance, LumMeshBlock block, void* block_data);
void lum_load_block_file(LumRenderer instance, LumMeshBlock block, const char* file);

void lum_upload_block_palette_to_gpu(LumRenderer instance);
void lum_upload_material_palette_to_gpu(LumRenderer instance);

LumMeshModel lum_load_mesh_file(LumRenderer instance, const char* file, bool try_extract_palette);
LumMeshModel lum_load_mesh_data(LumRenderer instance, void* mesh_data, int x_size, int y_size, int z_size);
void lum_free_mesh(LumRenderer instance, LumMeshModel model);

LumMeshFoliage lum_load_foliage(LumRenderer instance, const char* vertex_shader_file, int vertices_per_foliage, int density);
LumMeshVolumetric lum_load_volumetric(LumRenderer instance, float max_density, float density_deviation, const uint8_t color[3]);
LumMeshLiquid lum_load_liquid(LumRenderer instance, int main_mat, int foam_mat);

void lum_start_frame(LumRenderer instance);
void lum_draw_world(LumRenderer instance);
void lum_draw_particles(LumRenderer instance);
void lum_draw_model(LumRenderer instance, const LumMeshModel mesh, const LumMeshTransform* trans);
void lum_draw_foliage_block(LumRenderer instance, const LumMeshFoliage foliage, const int pos[3]);
void lum_draw_liquid_block(LumRenderer instance, const LumMeshLiquid liquid, const int pos[3]);
void lum_draw_volumetric_block(LumRenderer instance, const LumMeshVolumetric volumetric, const int pos[3]);

void lum_prepare_frame(LumRenderer instance);
void lum_submit_frame(LumRenderer instance);

void lum_cleanup(LumRenderer instance);
// GLFWwindow*
void* lum_get_glfw_ptr(const LumRenderer instance);
bool lum_get_should_close(const LumRenderer instance);
bool lum_set_should_close(const LumRenderer instance, bool should_close);
void lum_wait_idle(LumRenderer instance);

bool lum_is_profiling_enabled(const LumRenderer instance);
int lum_get_current_timestamp_count(const LumRenderer instance);
void lum_get_timestamp_name(const LumRenderer instance, int index, char* out_name, size_t name_length);
double lum_get_timestamp(const LumRenderer instance, int index);
double lum_get_timestamp_difference(const LumRenderer instance, int index_start, int index_end);

#ifdef __cplusplus
}
#endif

#endif // LUM_C_BINDINGS_H