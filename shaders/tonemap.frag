#version 450

//simple ssao shader

layout(location = 0)  in vec2 non_clip_pos;
layout(location = 0) out vec4 frame_color;

layout(input_attachment_index = 0, set = 0, binding = 0) uniform subpassInput rendered_frame;

const float COLOR_ENCODE_VALUE = 5.0;
vec3 decode_color(vec3 encoded_color){
    return encoded_color*COLOR_ENCODE_VALUE;
}
vec3 encode_color(vec3 color){
    return color/COLOR_ENCODE_VALUE;
}

float luminance(vec3 v){
    return dot(v, vec3(0.2126f, 0.7152f, 0.0722f));
}
vec3 change_luminance(vec3 c_in, float l_out){
    float l_in = luminance(c_in);
    return c_in * (l_out / l_in);
}
vec3 reinhard_extended(vec3 v, float max_white){
    vec3 numerator = v * (1.0f + (v / vec3(max_white * max_white)));
    return numerator / (1.0f + v);
}
vec3 reinhard_extended_luminance(vec3 v, float max_white_l){
    float l_old = luminance(v);
    float numerator = l_old * (1.0f + (l_old / (max_white_l * max_white_l)));
    float l_new = numerator / (1.0f + l_old);
    return change_luminance(v, l_new);
}
vec3 uncharted2_tonemap_partial(vec3 x){
    float A = 0.15f;
    float B = 0.50f;
    float C = 0.10f;
    float D = 0.20f;
    float E = 0.02f;
    float F = 0.30f;
    return ((x*(A*x+C*B)+D*E)/(x*(A*x+B)+D*F))-E/F;
}
vec3 uncharted2_filmic(vec3 v){
    float exposure_bias = 2.0f;
    vec3 curr = uncharted2_tonemap_partial(v * exposure_bias);

    vec3 W = vec3(11.2f);
    vec3 white_scale = vec3(1.0f) / uncharted2_tonemap_partial(W);
    return curr * white_scale;
}
vec3 aces_approx(vec3 v){
    v *= 0.6f;
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    return clamp((v*(a*v+b))/(v*(c*v+d)+e), 0.0f, 1.0f);
}
vec3 tonemap(vec3 color){
    // return reinhard_extended(color, 5.0);
    return reinhard_extended_luminance(color, 5.0);
    // return uncharted2_filmic(color);
    // return aces_approx(color);
}

vec3 adjust_brightness(vec3 color, float value) {
    return color + value;
}
vec3 adjust_contrast(vec3 color, float value) {
    // return 0.5 + value * (color - 0.5);
    return 0.5 + (1.0 + value) * (color - 0.5);
}
vec3 adjust_exposure(vec3 color, float value) {
    return (1.0 + value) * color;
}
vec3 adjust_saturation(vec3 color, float value) {
    float grayscale = luminance(color);
    return mix(vec3(grayscale), color, 1.0 + value);
}

void main() {
    // frame_color = vec4(vec3(0.5), 1);
    vec3 color = decode_color(subpassLoad(rendered_frame).xyz);

    color = adjust_saturation(color, .1);
    color = adjust_contrast(color, .1);
    color = adjust_exposure(color, 0.2);
    color = tonemap(color);

    // frame_color = vec4(vec3(non_clip_pos,0), 1);
    frame_color = vec4(color,1);
    // frame_color = vec4(vec3(color.x), .5);
} 