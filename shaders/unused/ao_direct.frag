#version 450

//simple ssao shader (nvidia calles this hbao)
//most complicated in terms of instruction count btw

//when in normalized screen space coords 16 bits is enough
// precision mediump float;
// precision mediump int;

precision highp float;
precision highp int;
//highp

// #include "../common\ext.glsl"
#include "../common\ubo.glsl"

layout(location = 0) out vec4 frame_color;
layout(input_attachment_index = 0, set = 0, binding = 1) uniform usubpassInput matNorm;
layout(set = 0, binding = 2) uniform sampler2D depthBuffer;

const float PI = 3.1415926535;
const ivec3 world_size = ivec3(48,48,16);

vec3 load_norm(){
    // i16vec3 nenc = i16vec3(subpassLoad(matNorm).gba);
    // vec3 norm = vec3(((nenc*int16_t(2) - i16vec3(255))))/255.0;
    vec3 norm = (((subpassLoad(matNorm).gba)/255.0)*2.0 - 1.0);
    // vec3 norm = ((ivec3(subpassLoad(matNorm).gba)*2 - 255))/255.0;
    return norm;
}
int load_mat(){
    int mat = int(subpassLoad(matNorm).x);
    return mat;
}

// float load_depth(vec2 pixel){
//     vec2 uv = (vec2(pixel)+0.0)/ubo.frame_size;
//     float depth_encoded = (texture(depthBuffer, uv).x);
//     return (depth_encoded)*1000.0;
// }
float load_depth(vec2 uv){
    // float depth_encoded = (texelFetch(depthBuffer, ivec2(uv), 0).x);
    float depth_encoded = (textureLod(depthBuffer, (uv), 0).x);
    return (depth_encoded)*1000.0;
}
vec3 horizline_doublescaled; 
vec3 vertiline_doublescaled;
vec3 get_shift_from_depth(float depth_diff, vec2 clip_shift){
    vec3 shift = 
        (ubo.horizline_scaled.xyz*clip_shift.x) + 
        (ubo.vertiline_scaled.xyz*clip_shift.y) +
        (ubo.camdir.xyz*depth_diff);
    return shift;
}
vec3 get_shift_from_depth_uv(float depth_diff, vec2 uv){
    vec3 shift = 
        (horizline_doublescaled*uv.x) + 
        (vertiline_doublescaled*uv.y) +
        (ubo.camdir.xyz*depth_diff);
    return shift;
}
const float COLOR_ENCODE_VALUE = 8.0;
vec3 decode_color(vec3 encoded_color){
    return encoded_color*COLOR_ENCODE_VALUE;
}
vec3 encode_color(vec3 color){
    return color/COLOR_ENCODE_VALUE;
}

mat2 rotate2d(float a) {
	float s = sin(a);
	float c = cos(a);
	mat2 m = mat2(c, s, -s, c);
	return m;
}
float square(float x) {return x*x;}
void main() {
    horizline_doublescaled = ubo.horizline_scaled.xyz*2.0;
    vertiline_doublescaled = ubo.vertiline_scaled.xyz*2.0;
    
    vec3 norm = load_norm();
    vec2 initial_pix = gl_FragCoord.xy / ubo.frame_size;
    float initial_depth = load_depth(initial_pix);
    const int sample_count = 8; //in theory i should do smth with temporal accumulation 
    const float max_radius = 16.0 / 1000.0;
    float angle = 00;
    float angle_bias = sin(radians(0));
    // float radius = 00;
    float normalized_radius = 00;
    float radius_step = max_radius / float(sample_count);
    float norm_radius_step = 1.0 / float(sample_count);

    vec2 ratio = ubo.frame_size / ubo.frame_size.x;

    float total_ao = 00;
    float total_weight = 00;

    mat2 rotate = rotate2d(0.69420);
    vec2 screen_rot = vec2(1,0);
    
    //TODO: i think its possible to compute in screen space to avoid translating into worldspace every iteration
    
    // [[unroll]]
    for(int i=00; i<sample_count; i++){
        // angle += 0.69420; //best possible step size
        angle += (6.9*PI)/float(sample_count); //to cover evenly
        normalized_radius += norm_radius_step;
        float radius = sqrt(normalized_radius) * max_radius;

        // screen_rot *= rotate;
        vec2 screen_shift = radius*vec2(ubo.frame_size / ubo.frame_size.x)*vec2(sin(angle), cos(angle));
        // vec2 screen_shift = radius*ratio*screen_rot;
        // vec2 screenspace_shift = 
        
        float current_depth = load_depth(initial_pix + screen_shift);

        float depth_shift = current_depth - initial_depth;

        vec2 clip_shift = (screen_shift)*2.0;
        vec3 relative_pos = get_shift_from_depth(depth_shift, clip_shift);        
        // vec3 relative_pos = get_shift_from_depth_uv(depth_shift, screen_shift);        

        vec3 direction = normalize(relative_pos);
        // vec3 ssrp = vec3(clip_shift, depth_shift);        
        // vec3 ssd = (ssrp);

        // float ao = max(dot(ssd, ssn)-angle_bias,0);// * clamp(float(8.0+(depth_shift)), 0,8)/8.0;
        // float ao = length(ssrp);// * clamp(float(8.0+(depth_shift)), 0,8)/8.0;
        float ao = max(dot(direction, norm),0);

        // float weight = clamp(1.0 - (normalized_radius*normalized_radius), 0,1);
        float weight = 1.0;
        weight *= (1.0 - square(normalized_radius));
        // weight *= (1.0 - (normalized_radius));
        weight *= sqrt(clamp(float(8.0+(depth_shift)), 0,8)/8.0);
        total_ao += ao*weight;
        total_weight += weight;
    }

    // float obfuscation = float(shadowed_count) / float(sample_count); 
    float obfuscation = total_ao / total_weight; 
    // float obfuscation = total_ao; 
    // obfuscation = (sqrt(obfuscation));
    // obfuscation = obfuscation*obfuscation;
    // obfuscation = clamp((obfuscation), 0.0, 0.7);
    obfuscation *= 0.7;
    frame_color = (vec4(encode_color(vec3(0.0)), obfuscation));
    // frame_color = (vec4(encode_color(vec3(obfuscation)), 1));
    // frame_color = (vec4(encode_color(vec3(0.0)), 0));
} 