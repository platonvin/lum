// lum_wrapper.cpp

#include "lum_c.h"
#include "lum_modern.hpp" // Include the original C++ header
#include <string>

struct LumWrapper {
    Lum* instance;
};

extern "C" {

LumWrapper* lum_create() {
    return new LumWrapper{new Lum()};
}

void lum_destroy(LumWrapper* lum) {
    delete lum->instance;
    delete lum;
}

void lum_init(LumWrapper* lum) {
    lum->instance->init();
}

void lum_cleanup(LumWrapper* lum) {
    lum->instance->cleanup();
}

void lum_load_world(LumWrapper* lum, const int32_t* world_data) {
    lum->instance->loadWorld(reinterpret_cast<const BlockID_t*>(world_data));
}

void lum_set_world_block(LumWrapper* lum, int x, int y, int z, uint32_t block) {
    lum->instance->setWorldBlock(x, y, z, static_cast<MeshBlock>(block));
}

uint32_t lum_get_world_block(const LumWrapper* lum, int x, int y, int z) {
    return lum->instance->getWorldBlock(x, y, z);
}

void* lum_load_mesh(LumWrapper* lum, const char* file, bool try_extract_palette) {
    std::string filename(file);
    return lum->instance->loadMesh(filename, try_extract_palette);
}

void lum_free_mesh(LumWrapper* lum, void* mesh) {
    lum->instance->freeMesh(static_cast<MeshModel&>(mesh));
}

void lum_start_frame(LumWrapper* lum) {
    lum->instance->startFrame();
}

void lum_draw_world(LumWrapper* lum) {
    lum->instance->drawWorld();
}

void lum_submit_frame(LumWrapper* lum) {
    lum->instance->submitFrame();
}

bool lum_should_close(const LumWrapper* lum) {
    return lum->instance->shouldClose();
}

void lum_wait_idle(LumWrapper* lum) {
    lum->instance->waitIdle();
}

} // extern "C"
