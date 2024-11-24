#include <renderer/api/renderer.hpp>
// #include <glm/glm.hpp>
#include <iostream>
int main(){
    std::cout << "before init\n";
    Lum::Settings settings = {};
        settings.fullscreen = false;
        settings.vsync = false;
        settings.world_size = glm::ivec3(48, 48, 16);
        settings.static_block_palette_size = 15;
        settings.maxParticleCount = 8128;
    Lum::Renderer lum = Lum::Renderer(15, 4096, 64, 64, 64);
    lum.init(settings);
        // single frame
        lum.prepareFrame();
        lum.submitFrame();
    lum.cleanup();
    std::cout << "after init\n";
}