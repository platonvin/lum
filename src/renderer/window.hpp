#pragma once

#include <GLFW/glfw3.h>
// #include <assert.h>

typedef struct Window{
    GLFWwindow* pointer;
    int width;
    int height;
} Window;