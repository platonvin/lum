#include "window.h"

Window create_window()
{Window window = {0};
    assert(glfwInit());

    glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);

    const GLFWvidmode* mode = glfwGetVideoMode(glfwGetPrimaryMonitor());
    window.width  = mode->width;
    window.height = mode->height;
    // window.width  = mode->width / 2;
    // window.height = mode->height / 2;

    // window.pointer = glfwCreateWindow(window.width, window.height, "renderer_gl", glfwGetPrimaryMonitor(), 0);
    window.pointer = glfwCreateWindow(window.width, window.height, "renderer_vk", 0, 0);

    assert(window.pointer != NULL);


    glfwMakeContextCurrent(window.pointer);

    // glfwSetInputMode(window.pointer, GLFW_STICKY_KEYS, 1);
    // glfwSetInputMode(window.pointer, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    return window;
}
