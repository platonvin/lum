#pragma once

//compute lookup table for ambient occlusion shader
#include <glm/glm.hpp>
#include <vector>
#include <cmath>

using namespace glm;
using std::vector;
struct AoLut {
    vec3 world_shift;
    float weight_normalized; // ((1-r^2)/total_weight)*0.7
    vec2 screen_shift;
    vec2 padding;
};

vector<AoLut> generateLUT (int sample_count, double max_radius,
                           dvec2 frame_size, dvec3 horizline_scaled, dvec3 vertiline_scaled);