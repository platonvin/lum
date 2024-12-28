//compute lut for ambient occlusion shader
#include "ao_lut.hpp"
#include <array>

vec3 get_world_shift_from_clip_shift (vec2 clip_shift, vec3 horizline_scaled, vec3 vertiline_scaled) {
    vec3 shift =
        (horizline_scaled* clip_shift.x) +
        (vertiline_scaled* clip_shift.y);
    return shift;
}

double calculate_total_weight (int sample_count, double max_radius,
                               dvec2 frame_size, dvec3 horizline_scaled, dvec3 vertiline_scaled) {
    double normalized_radius = 0.0;
    double norm_radius_step = 1.0 / double (sample_count);
    double total_weight = 0.0;
    for (int i = 0; i < sample_count; ++i) {
        normalized_radius += norm_radius_step;
        double weight = (1.0 - normalized_radius* normalized_radius);
        total_weight += weight;
    }
    return total_weight;
}

template <size_t sample_count>
std::array<AoLut, sample_count> generateLUT (double max_radius,
                           dvec2 frame_size, dvec3 horizline_scaled, dvec3 vertiline_scaled) {
    std::array<AoLut, sample_count> lut = {};
    double angle = 0.0;
    double normalized_radius = 0.0;
    double norm_radius_step = 1.0 / double (sample_count);
    double radius_step = max_radius / double (sample_count);
    double total_weight = calculate_total_weight (sample_count, max_radius, frame_size, horizline_scaled, vertiline_scaled);
    normalized_radius = 0.0;
    for (int i = 0; i < sample_count; ++i) {
        angle += (6.9 * M_PI) / double (sample_count); // even coverage
        normalized_radius += norm_radius_step;
        double radius = sqrt (normalized_radius) * max_radius;
        dvec2 screen_shift = dvec2 (radius) * (frame_size / dvec2 (frame_size.x)) * dvec2 (sin (angle), cos (angle));
        dvec2 clip_shift = (screen_shift) * 2.0;
        dvec3 world_shift = horizline_scaled * clip_shift.x + vertiline_scaled * clip_shift.y;
        double weight = (1.0 - normalized_radius* normalized_radius);
        double weight_normalized = (weight / total_weight) * 0.7;
        lut[i] = {
            vec3 (world_shift),
            float (weight_normalized),
            vec2 (screen_shift),
            vec2 (0) //padding
        };
    }
    return lut;
}

template class std::array<AoLut, 8>;
template std::array<AoLut, 8> generateLUT<8>(double, dvec2, dvec3, dvec3);