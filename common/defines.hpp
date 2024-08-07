#pragma once

#include <iostream>

#define KEND  "\x1B[0m"
#define KRED  "\x1B[31m"
#define KGRN  "\x1B[32m"
#define KYEL  "\x1B[33m"
#define KBLU  "\x1B[34m"
#define KMAG  "\x1B[35m"
#define KCYN  "\x1B[36m"
#define KWHT  "\x1B[37m"

#define println printf(KGRN "%s:%d: Fun: %s\n" KEND, __FILE__, __LINE__, __FUNCTION__);
#define printl(x) std::cout << #x " "<< x << std::endl;

// #include <stdint.h>
// #include <stdfloat>

// typedef int8_t    i8;
// typedef int16_t   i16;
// typedef int32_t   i32;
// typedef int64_t   i64;
// typedef uint8_t   u8;
// typedef uint16_t  u16;
// typedef uint32_t  u32;
// typedef uint64_t  u64;

// typedef _Float16  f16;
// typedef _Float32  f32;
// typedef _Float64  f64;
// typedef _Float128 f128;

#define STRINGIZE(x) STRINGIZE2(x)
#define STRINGIZE2(x) #x
#define __LINE_STRING__ STRINGIZE(__LINE__)

#define crash(string) do {printf(KRED "l:%d %s\n" KEND, __LINE__, #string); exit(69);} while(0)

// #define NDEBUG
#ifdef NDEBUG
// #define VMA_NAME(allocation) 
#define VKNDEBUG
#define VK_CHECK(func) do{\
    VkResult result = func;\
    if (result != VK_SUCCESS) {\
        fprintf(stderr, "LINE :%d Vulkan call failed: %s\n", __LINE__, string_VkResult(result));\
        exit(result);\
    }} while (0)
#else
#define VK_CHECK(func) do{\
    VkResult result = (func);\
    if (result != VK_SUCCESS) {\
        fprintf(stderr, "LINE :%d Vulkan call failed: %s\n", __LINE__, string_VkResult(result));\
        exit(1);\
    }} while (0)
#define malloc(x)   ({void* res_ptr =(malloc)(x  ); assert(res_ptr!=NULL && __LINE_STRING__); res_ptr;})
#define calloc(x,y) ({void* res_ptr =(calloc)(x,y); assert(res_ptr!=NULL && __LINE_STRING__); res_ptr;})
#endif

#define PLACE_TIMESTAMP() do {\
    if(measureAll){\
        timestampNames[currentTimestamp] = __func__;\
        vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, queryPoolTimestamps[currentFrame], currentTimestamp++);\
    }\
} while(0)

#define PLACE_TIMESTAMP_ALWAYS() do {\
    timestampNames[currentTimestamp] = __func__;\
    vkCmdWriteTimestamp(commandBuffer, VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, queryPoolTimestamps[currentFrame], currentTimestamp++);\
} while(0)

// #define SET_ALLOC_NAMES
#ifdef SET_ALLOC_NAMES
#define vmaCreateImage(allocator, pImageCreateInfo, pAllocationCreateInfo, pImage, pAllocation, pAllocationInfo) {\
    VkResult res = (vmaCreateImage)(allocator, pImageCreateInfo, pAllocationCreateInfo, pImage, pAllocation, pAllocationInfo);\
    vmaSetAllocationName(allocator,* pAllocation, #pAllocation __LINE_STRING__);\
    res;\
    }
#define vmaCreateBuffer(allocator, pImageCreateInfo, pAllocationCreateInfo, pImage, pAllocation, pAllocationInfo) {\
    VkResult res = (vmaCreateBuffer)(allocator, pImageCreateInfo, pAllocationCreateInfo, pImage, pAllocation, pAllocationInfo);\
    vmaSetAllocationName(allocator,* pAllocation, #pAllocation " " __LINE_STRING__);\
    res;\
    }
#endif
