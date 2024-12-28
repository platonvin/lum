#pragma once

//compute lookup table for ambient occlusion shader
#include <glm/glm.hpp>
#include <vector>
#ifndef M_PI
    #define M_PI 3.14159265358979323846
#endif

using glm::u8, glm::u16, glm::u16, glm::u32;
using glm::i8, glm::i16, glm::i32;
using glm::f32, glm::f64;
using glm::defaultp;
using glm::quat;
using glm::ivec2,glm::ivec3,glm::ivec4;
using glm::i8vec2,glm::i8vec3,glm::i8vec4;
using glm::i16vec2,glm::i16vec3,glm::i16vec4;
using glm::uvec2,glm::uvec3,glm::uvec4;
using glm::u8vec2,glm::u8vec3,glm::u8vec4;
using glm::u16vec2,glm::u16vec3,glm::u16vec4;
using glm::vec,glm::vec2,glm::vec3,glm::vec4;
using glm::dvec2,glm::dvec3,glm::dvec4;
using glm::mat, glm::mat2, glm::mat3, glm::mat4;
using glm::dmat2, glm::dmat3, glm::dmat4;

using std::vector;
struct AoLut {
    vec3 world_shift;
    float weight_normalized; // ((1-r^2)/total_weight)*0.7
    vec2 screen_shift;
    vec2 padding;
};

vec3 get_world_shift_from_clip_shift (vec2 clip_shift, vec3 horizline_scaled, vec3 vertiline_scaled);

double calculate_total_weight (int sample_count, double max_radius,
                               dvec2 frame_size, dvec3 horizline_scaled, dvec3 vertiline_scaled);

// I still have no idea if this is valid C++23
template <size_t sample_count>
std::array<AoLut, sample_count> generateLUT (double max_radius,
                           dvec2 frame_size, dvec3 horizline_scaled, dvec3 vertiline_scaled);