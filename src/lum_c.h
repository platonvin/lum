#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

typedef struct LumWrapper LumWrapper;
typedef struct lum_ivec3 {
    int x,y,z;
} lum_ivec3;
typedef struct LumSpecificSettings {
    lum_ivec3 world_size;// lum_ivec3{48, 48, 16};
    int static_block_palette_size;// 15;
    int maxParticleCount;// 8128;
} LumSpecificSettings;
const LumSpecificSettings LUM_DEFAULT_SETTINGS = {
    {48, 48, 16},
    15,
    8128,
};

// Lum initialization and cleanup
LumWrapper* lum_create(LumSpecificSettings settings);
void lum_destroy(LumWrapper* lum);
void lum_init(LumWrapper* lum);
void lum_cleanup(LumWrapper* lum);

// World loading and block management
void lum_load_world(LumWrapper* lum, const int32_t* world_data);
void lum_set_world_block(LumWrapper* lum, int x, int y, int z, uint32_t block);
uint32_t lum_get_world_block(const LumWrapper* lum, int x, int y, int z);

// Mesh loading and management
void* lum_load_mesh(LumWrapper* lum, const char* file, bool try_extract_palette);
void lum_free_mesh(LumWrapper* lum, void* mesh);

// Frame rendering and submission
void lum_start_frame(LumWrapper* lum);
void lum_draw_world(LumWrapper* lum);
void lum_submit_frame(LumWrapper* lum);

// Window management
bool lum_should_close(const LumWrapper* lum);
void lum_wait_idle(LumWrapper* lum);

#ifdef __cplusplus
}
#endif