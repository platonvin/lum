#version 450

//simple ssao shader

layout(location = 0) in vec2 non_clip_pos;
layout(location = 0) out vec4 frame_color;

#extension GL_EXT_control_flow_attributes : enable

layout(push_constant) uniform restrict readonly constants{
    vec4 campos;
    vec4 camdir;
} pco;
vec3 globalLightDir;
vec3 cameraRayDirPlane;
vec3 horizline;
vec3 vertiline;
const float PI = 3.1415926535;
const ivec3 world_size = ivec3(48,48,16);
const float view_width  = 1920.0 / 10.0; //in block_diags
const float view_height = 1080.0 / 10.0; //in blocks

layout(input_attachment_index = 0, set = 0, binding = 0) uniform usubpassInput matNorm;
// layout(input_attachment_index = 1, set = 0, binding = 1) uniform subpassInput inFrame;

layout(set = 0, binding = 1) uniform sampler2D depthBuffer;

vec3 load_norm(){
    vec3 norm = (((subpassLoad(matNorm).gba)/255.0)*2.0 - 1.0);
    return norm;
}
int load_mat(){
    int mat = int(subpassLoad(matNorm).x);
    return mat;
}

float load_depth(vec2 pixel){
    vec2 uv = (vec2(pixel)+0.0)/vec2(textureSize(depthBuffer,0));
    float depth_encoded = (texture(depthBuffer, uv).x);
    return (depth_encoded)*1000.0;
}

vec3 get_shift_from_depth(float depth_diff, vec2 clip_shift){
    const vec2 view_size = vec2(view_width, view_height);
    const vec2 clip_shift_scaled = clip_shift*view_size;
    
    vec3 origin = 
        (horizline*clip_shift_scaled.x) + 
        (vertiline*clip_shift_scaled.y) +
        (pco.camdir.xyz*depth_diff);
    return origin;
}

void main() {
    cameraRayDirPlane = normalize(vec3(pco.camdir.xy, 0));
    horizline = normalize(cross(cameraRayDirPlane, vec3(0,0,1)));
    vertiline = normalize(cross(pco.camdir.xyz, horizline));

    ivec2 size = textureSize(depthBuffer,0);
    
    // vec3 norm = load_norm();
    // vec2 initial_pix = gl_FragCoord.xy;
    // float initial_depth = load_depth(initial_pix);
    // const int sample_count = 64;
    // const float max_radius = 5.0;
    // int shadowed_count = 00;

    // sample in  worldspace

    // vec3 virtual_x = normalize(cross(norm, vec3(1)));
    // vec3 virtual_y = normalize(cross(norm, virtual_x));
    
    // float angle = 00;
    // float radius = 00;
    
    // // [[unroll]]
    // for(int i=00; i<sample_count; i++){
    //     vec2 virtual_shift = radius*vec2(sin(angle), cos(angle));
    //     vec3 worldspace_shift = virtual_shift.x*virtual_x + virtual_shift.y*virtual_y;
    //     worldspace_shift += norm*radius;
    //     // vec2 screenspace_shift = 
        
    //     float depth_proj = dot(worldspace_shift, pco.camdir.xyz);
    //     vec2 screen_poj;
    //     screen_poj.x = ((dot(worldspace_shift, horizline)/(view_width *2.0)))*float(size.x);
    //     screen_poj.y = ((dot(worldspace_shift, vertiline)/(view_height*2.0)))*float(size.y);
    //     float current_depth = initial_depth + depth_proj;
    //     vec2 current_pix = initial_pix + screen_poj;

    //     float test_depth = load_depth(current_pix);
    //     if (
    //         (test_depth < (current_depth-0.1)) &&
    //         ((current_depth-test_depth) < 8.0)
    //     ) shadowed_count++;

    //     angle += 0.69420;
    //     radius += max_radius / float(sample_count);
    // }

    // sample in screenspace
    

    vec3 norm = load_norm();
    vec2 initial_pix = gl_FragCoord.xy;
    float initial_depth = load_depth(initial_pix);
    const int sample_count = 10;
    const float max_radius = 6.0;
    float angle = 00;
    float angle_bias = sin(radians(0));
    float radius = 00;

    float total_ao = 00;
    float total_weight = 00;
    
    [[unroll]]
    for(int i=01; i<=sample_count; i++){
        angle += 0.69420;
        radius = ((float(i)/float(sample_count)))*max_radius;

        vec2 screen_shift = radius*vec2(sin(angle), cos(angle));
        // vec2 screenspace_shift = 
        
        float current_depth = load_depth(initial_pix + screen_shift);

        float depth_shift = current_depth - initial_depth;
        vec2 pix_shift = screen_shift;
        vec2 clip_shift = (pix_shift / vec2(size))*2.0;

        vec3 relative_pos = get_shift_from_depth(depth_shift, clip_shift);        
        vec3 direction = normalize(relative_pos);

        // float dist = length(screen_shift);
        // if((depth_shift) < 8.0) {
            float ao = clamp(dot(direction, norm)-angle_bias ,0,1);// * float((depth_shift) < 8.0);
            float normalized_radius = (radius/float(max_radius));
            // float weight = clamp(1.0 - (normalized_radius*normalized_radius), 0,1);
            float weight = clamp(1.0 - (normalized_radius)*(normalized_radius), 0,1);
            total_ao += ao*weight;
            total_weight += weight;
            // total_ao = max(total_ao, dot(direction, norm));
        // }        
        // angle += PI / 3.333;
    }

    // float obfuscation = float(shadowed_count) / float(sample_count); 
    float obfuscation = total_ao / total_weight; 
    // float obfuscation = total_ao; 
    // obfuscation = (sqrt(obfuscation));
    // obfuscation = obfuscation*obfuscation;
    obfuscation = clamp((obfuscation), 0.0, 0.7);
    frame_color = vec4(vec3(0.0), obfuscation);
    // frame_color = vec4(vec3(0.0), 0);
    // frame_color = vec4(vec3(1-obfuscation), 1);
} 