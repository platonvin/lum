#pragma once
#include "GLFW/glfw3.h"
#ifndef LUM_C_BINDINGS_H
#define LUM_C_BINDINGS_H

/*
C99 API for Lum
it does get updates, but less often (on more stable versions)
*/

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

// yypedefs for opaque pointers representing classes in Lum C++ API
typedef struct LumRenderer LumInstance; // pointer is fine
typedef void*  LumMeshModel; // pointer avoided (i mean extra pointer)
typedef int16_t LumMeshBlock; // to avoid pointer
typedef struct LumMeshFoliage LumMeshFoliage; // pointer not avoided
typedef struct {uint8_t main, foam;} LumMeshLiquid; // to avoid pointer
typedef struct {
    float max_dencity;
    float variation;
    uint8_t color[3];
} LumMeshVolumetric; // to avoid pointer
typedef struct LumMeshUi LumMeshUi;
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
LumInstance* lum_create_instance(const LumSettings* settings);
void lum_destroy_instance(LumInstance* instance);

void lum_demo_load_scene(LumInstance* instance, const char* path);

void lum_init(LumInstance* instance, const LumSettings* settings);
void lum_load_world(LumInstance* instance, const LumMeshBlock* world_data);
void lum_load_world_file(LumInstance* instance, const char* file);

void lum_set_world_block(LumInstance* instance, int x, int y, int z, LumMeshBlock block);
LumMeshBlock lum_get_world_block(const LumInstance* instance, int x, int y, int z);

void lum_load_block(LumInstance* instance, LumMeshBlock block, void* block_data);
void lum_load_block_file(LumInstance* instance, LumMeshBlock block, const char* file);

void lum_upload_block_palette_to_gpu(LumInstance* instance);
void lum_upload_material_palette_to_gpu(LumInstance* instance);

LumMeshModel lum_load_mesh_file(LumInstance* instance, const char* file, bool try_extract_palette);
LumMeshModel lum_load_mesh_data(LumInstance* instance, void* mesh_data, int x_size, int y_size, int z_size);
void lum_free_mesh(LumInstance* instance, LumMeshModel model);

LumMeshFoliage* lum_load_foliage(LumInstance* instance, const char* vertex_shader_file, int vertices_per_foliage, int density);
LumMeshVolumetric lum_load_volumetric(LumInstance* instance, float max_density, float density_deviation, const uint8_t color[3]);
LumMeshLiquid lum_load_liquid(LumInstance* instance, int main_mat, int foam_mat);

void lum_start_frame(LumInstance* instance);
void lum_draw_world(LumInstance* instance);
void lum_draw_particles(LumInstance* instance);
void lum_draw_model(LumInstance* instance, const LumMeshModel mesh, const LumMeshTransform* trans);
void lum_draw_foliage_block(LumInstance* instance, const LumMeshFoliage* foliage, const int pos[3]);
void lum_draw_liquid_block(LumInstance* instance, const LumMeshLiquid liquid, const int pos[3]);
void lum_draw_volumetric_block(LumInstance* instance, const LumMeshVolumetric volumetric, const int pos[3]);

void lum_prepare_frame(LumInstance* instance);
void lum_submit_frame(LumInstance* instance);

void lum_cleanup(LumInstance* instance);
GLFWwindow* lum_get_glfw_ptr(const LumInstance* instance);
bool lum_get_should_close(const LumInstance* instance);
bool lum_set_should_close(const LumInstance* instance, bool should_close);
void lum_wait_idle(LumInstance* instance);

bool lum_is_profiling_enabled(const LumInstance* instance);
int lum_get_current_timestamp_count(const LumInstance* instance);
void lum_get_timestamp_name(const LumInstance* instance, int index, char* out_name, size_t name_length);
double lum_get_timestamp(const LumInstance* instance, int index);
double lum_get_timestamp_difference(const LumInstance* instance, int index_start, int index_end);

#ifdef __cplusplus
}
#endif

#endif // LUM_C_BINDINGS_H