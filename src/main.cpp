#include <stdio.h>

#include <Volk/volk.h>
#include <GLFW/glfw3.h>
#include <renderer/render.hpp>
#include <renderer/window.hpp>

// #include <slmath/sources/

VisualWorld world = {};
Renderer render = {};
int main() {
    // init();
    world.init();
    // cout << world.unitedBlocks.size().x <<" "<< world.unitedBlocks.size().y <<" "<< world.unitedBlocks.size().z <<"\n";
    render.init();

    while(!glfwWindowShouldClose(render.window.pointer) and glfwGetKey(render.window.pointer, GLFW_KEY_ESCAPE) != GLFW_PRESS){
        glfwPollEvents();
        render.draw_Frame();
        vkDeviceWaitIdle(render.device);
    }

    render.cleanup();

    return 0;
}
