#version 450

//simple ssao shader

//required
precision highp float;
precision highp int;

layout(location = 0) in vec2 non_clip_pos;
layout(location = 0) out vec4 frame_color;

#extension GL_EXT_control_flow_attributes : enable

layout(binding = 0, set = 0) uniform restrict readonly UniformBufferObject {
    mat4 trans_w2s;
    vec4 campos;
    vec4 camdir;
    vec4 horizline_scaled;
    vec4 vertiline_scaled;
    vec4 globalLightDir;
    mat4 lightmap_proj; 
    vec2 frame_size;
    int timeseed;
} ubo;
layout(input_attachment_index = 0, set = 0, binding = 1) uniform usubpassInput matNorm;
layout(set = 0, binding = 2) uniform sampler2D depthBuffer;

const float PI = 3.1415926535;
const ivec3 world_size = ivec3(48,48,16);

vec3 load_norm(){
    vec3 norm = (((subpassLoad(matNorm).gba)/255.0)*2.0 - 1.0);
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
    float depth_encoded = (texelFetch(depthBuffer, ivec2(uv), 0).x);
    return (depth_encoded)*1000.0;
}

vec3 get_shift_from_depth(float depth_diff, vec2 clip_shift){
    vec3 shift = 
        (ubo.horizline_scaled.xyz*clip_shift.x) + 
        (ubo.vertiline_scaled.xyz*clip_shift.y) +
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

void main() {
    vec3 norm = load_norm();
    vec2 initial_pix = gl_FragCoord.xy;
    float initial_depth = load_depth(initial_pix);
    const int sample_count = 30; //in theory i can do smth with temporal accumulation 
    const float max_radius = 10.0;
    float angle = 00;
    float angle_bias = sin(radians(0));
    float radius = 00;

    float total_ao = 00;
    float total_weight = 00;
    
    [[unroll]]
    for(int i=01; i<=sample_count; i++){
        angle += 0.69420; //best possible step size
        radius = ((float(i)/float(sample_count)))*max_radius;

        vec2 screen_shift = radius*vec2(sin(angle), cos(angle));
        // vec2 screenspace_shift = 
        
        float current_depth = load_depth(initial_pix + screen_shift);

        float depth_shift = current_depth - initial_depth;
        vec2 pix_shift = screen_shift;
        vec2 clip_shift = (pix_shift / vec2(ubo.frame_size))*2.0;

        vec3 relative_pos = get_shift_from_depth(depth_shift, clip_shift);        
        vec3 direction = normalize(relative_pos);

        float ao = max(dot(direction, norm)-angle_bias,0);// * clamp(float(8.0+(depth_shift)), 0,8)/8.0;
        float normalized_radius = (radius/float(max_radius));
        // float weight = clamp(1.0 - (normalized_radius*normalized_radius), 0,1);
        float weight = clamp(1.0 - (normalized_radius)*(normalized_radius), 0,1);
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
    frame_color = (vec4(encode_color(vec3(obfuscation)), 1));
} 