#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <Volk/volk.h>
#include <vulkan/vulkan.hpp>
#include <vulkan/vk_enum_string_helper.h>
#include "window.h"
#include <malloc.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
#include <limits.h>

#define KNRM  "\x1B[0m"
#define KRED  "\x1B[31m"
#define KGRN  "\x1B[32m"
#define KYEL  "\x1B[33m"
#define KBLU  "\x1B[34m"
#define KMAG  "\x1B[35m"
#define KCYN  "\x1B[36m"
#define KWHT  "\x1B[37m"

#define each(item, array, length) \
(typeof(*(array)) *p = (array), (item) = *p; p < &((array)[length]); p++, (item) = *p)

#define VK_CHECK(func)                                                                        \
do {                                                                                          \
    VkResult result = func;                                                                   \
    if (result != VK_SUCCESS) {                                                               \
        fprintf(stderr, "LINE %d: Vulkan call failed: %s\n", __LINE__, string_VkResult(result));\
        exit(1);                                                                              \
    }                                                                                         \
} while (0)

#define _CheckTypes(a,b) _Static_assert(_Generic(a, typeof (b):1, default: 0), "Mismatched types")
#define Clamp(x,min,max) \
    ({ \
        typeof (x) _x = (x); \
        typeof (min) _min = (min); \
        typeof (max) _max = (max); \
        _CheckTypes(_x,_min); \
        _CheckTypes(_x,_max); \
        (_x > _max ? _max : (_x < _min ? _min : _x)); \
    })

#define print printf("L:%d F:%s\n", __LINE__, __FUNCTION__);

typedef struct FamilyIndex {
    bool exists;
    uint32_t index;
} FamilyIndex;

typedef struct QueueFamilyIndices {
    FamilyIndex graphical;
    FamilyIndex present;
} QueueFamilyIndices;

typedef struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    uint32_t formatCount;
    VkSurfaceFormatKHR* formats;
    uint32_t presentModesCount;
    VkPresentModeKHR* presentModes;
} SwapChainSupportDetails;   

typedef struct RenderContext{
    Window window;
    VkInstance instance;
    VkPhysicalDevice physicalDevice;
    // QueueFamilyIndices familyIndices;
    VkDevice device;
    VkSurfaceKHR surface;

    VkQueue graphicsQueue;
    VkQueue presentQueue;

    VkSwapchainKHR swapchain;
    uint32_t swapChainImagesCount;
    VkImage* swapChainImages;
} RenderContext;

RenderContext init_renderer();
void render_cleanup(RenderContext render_context);