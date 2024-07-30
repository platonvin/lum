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
