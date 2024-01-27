#include <stdio.h>

#include <Volk/volk.h>
#include <GLFW/glfw3.h>
#include <renderer/render.hpp>
#include <renderer/window.hpp>


int main() {
    // init();
    Renderer render = {};
    render.init();

    while(!glfwWindowShouldClose(render.window.pointer) and glfwGetKey(render.window.pointer, GLFW_KEY_ESCAPE) != GLFW_PRESS){
        glfwPollEvents();
        render.draw_Frame();
        vkDeviceWaitIdle(render.device);
    }

    render.cleanup();

    return 0;
}
