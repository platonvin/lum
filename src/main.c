#include <stdio.h>

#include <GLFW/glfw3.h>
#include <Volk/volk.h>
#include <renderer/render.h>
#include <renderer/window.h>
#include <stdbool.h>

typedef struct {
    bool should_close;
    RenderContext rctx;
} game;

game g = {0};

void init() {
    g.rctx = init_renderer();
    // init_physics();
    // init_network();
}

void update() {
    glfwPollEvents();

    GLFWwindow *winptr = g.rctx.window.pointer;
    g.should_close = g.should_close || glfwWindowShouldClose(winptr);

    if (glfwGetKey(winptr, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        g.should_close = true;

    glfwSwapBuffers(winptr);
}

void cleanup() {
    render_cleanup(g.rctx);
}

int main() {
    init();

    while (!g.should_close) {
        update();
    }

    cleanup();

    return 0;
}
