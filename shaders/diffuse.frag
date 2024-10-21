#version 450 core

/*
shader to diffuse-color lowres frame
ambient + "radiant" diffuse + lightmaps
*/

precision highp int;
precision highp float;
#include "common/ext.glsl"
#include "common/ubo.glsl"

layout(input_attachment_index = 0, set = 0, binding = 1) uniform usubpassInput matNorm;
layout(input_attachment_index = 1, set = 0, binding = 2) uniform  subpassInput depthBuffer;
layout(set = 0, binding = 3 ) uniform sampler2D voxelPalette;
layout(set = 0, binding = 4 ) uniform sampler3D radianceCache;
layout(set = 0, binding = 5 ) uniform sampler2DShadow lightmap;

layout(location = 0) out vec4 frame_color;

#define RAYS_PER_PROBE (32)
const ivec3 world_size = ivec3(48,48,16);

vec3 sample_probe(ivec3 probe_ipos, vec3 direction){
    ivec3 probe_ipos_clamped = clamp(probe_ipos, ivec3(0), world_size);
    ivec3 subprobe_pos;
          subprobe_pos.x  = probe_ipos_clamped.x; //same as local_pos actually but its optimized away and worth it for modularity
          subprobe_pos.yz = probe_ipos_clamped.yz; //reuses but its optimized away
    // vec3 light = imageLoad(radianceCache, (subprobe_pos)).xyz;
    vec3 light = texelFetch(radianceCache, (subprobe_pos), 0).rgb;
    return clamp(light, 0, 2);
}
float square(float a){return a*a;}

vec3 sample_radiance(vec3 position, vec3 normal){
    vec3 sampled_light;

    float total_weight =      0 ;
     vec3 total_colour = vec3(0);

    ivec3 zero_probe_ipos = clamp(ivec3(floor(position - 8.0))/16, ivec3(0), world_size);
     vec3 zero_probe_pos = vec3(zero_probe_ipos)*16.0 + 8.0;

    vec3 alpha = clamp((position - zero_probe_pos) / 16.0, 0,1);
    // alpha = vec3(1);

    for (int i=0; i<8; i++){
        //to make it little more readable
        ivec3 offset = ivec3(i, i >> 1, i >> 2) & ivec3(1);

        float probe_weight =     (1);
         vec3 probe_colour = vec3(0);

        vec3 probe_pos = zero_probe_pos + vec3(offset)*16.0;

        vec3  probeToPoint = probe_pos - position;
        vec3 direction_to_probe = normalize(probeToPoint);

        vec3 trilinear = mix(1.0-alpha, alpha, vec3(offset));
        probe_weight = trilinear.x * trilinear.y * trilinear.z;

        
        /*
        actually, not using directional weight **might** increase quality 
        by adding extra shadows in corners made of solid blocks
        but im still going to use it

        0.1 clamp to prevent weird cases where occasionally every single one would be 0 - in such cases, it will lead to trilinear
        */
        float direction_weight = clamp(dot(direction_to_probe, normal), 0.1,1);
        // float direction_weight = square(max(0.0001, (dot(direction_to_probe, normal) + 1.0) * 0.5)) + 0.2;
        // float direction_weight = float(dot(direction_to_probe, normal) > 0);

        probe_weight *= direction_weight;
        
        // const float crushThreshold = 0.2;
        // if (probe_weight < crushThreshold) {
        //     probe_weight *= probe_weight * probe_weight * (1.0 / square(crushThreshold)); 
        // }

        probe_colour = sample_probe(zero_probe_ipos + offset, direction_to_probe);
        // probe_colour = vec3(zero_probe_ipos + offset) / vec3(world_size);

        probe_weight  = max(1e-7, probe_weight);
        total_weight += probe_weight;
        total_colour += probe_weight * probe_colour;
    }

    return total_colour / total_weight;
}

vec3 sample_radiance(vec3 position){
    vec3 block_pos = position / 16.0;
    vec3 sampled_light = textureLod(radianceCache, (((block_pos / vec3(world_size)))), 0).rgb;
    // vec3 sampled_light = texture(radianceCache, (block_pos / vec3(world_size))).rgb;
    return sampled_light;
}

struct Material{
    vec3 color;
    float emmitance;
    float roughness;
    float transparancy;
};
vec3 load_norm(){
    // vec3 norm = (imageLoad(matNorm, pixel).gba);
    vec3 norm = (((subpassLoad(matNorm).gba/255.0)*2.0 - 1.0));
    return norm;
}
int load_mat(){
    int mat = int((subpassLoad(matNorm).x));
    return mat;
}
highp float load_depth(){
    // highp vec2 uv = (vec2(pixel)+0.5)/vec2(size);
    highp float depth_encoded = (subpassLoad(depthBuffer).x);
    return (depth_encoded)*1000.0;
}
Material GetMat(in int voxel){
    Material mat;

    mat.color.r      = texelFetch(voxelPalette, ivec2(0,voxel), 0).r;
    mat.color.g      = texelFetch(voxelPalette, ivec2(1,voxel), 0).r;
    mat.color.b      = texelFetch(voxelPalette, ivec2(2,voxel), 0).r;
    // mat.transparancy = 1.0 - texelFetch(voxelPalette, ivec2(3,voxel), 0).r;
    mat.emmitance    =       texelFetch(voxelPalette, ivec2(4,voxel), 0).r;
    mat.roughness    =       texelFetch(voxelPalette, ivec2(5,voxel), 0).r;
    return mat;
}

vec3 get_origin_from_depth(float depth, vec2 clip_pos){
    vec3 origin = ubo.campos.xyz +
        (ubo.horizline_scaled.xyz*clip_pos.x) + 
        (ubo.vertiline_scaled.xyz*clip_pos.y) +
        (ubo.camdir.xyz*depth);
    return origin;
}
vec3 get_origin_from_depth_interpolated(float depth, vec3 pos_interpol){
    vec3 origin = pos_interpol + (ubo.camdir.xyz*depth);
    return origin;
}
//not really nextafter but somewhat close
float next_after (float x, int s){
    int ix = floatBitsToInt(x);
    float fxp1 = intBitsToFloat(ix+s);
    return fxp1;}
float next_after (float x){ return next_after(x, 1);}
float prev_befor (float x, int s){
    int ix = floatBitsToInt(x);
    float fxp1 = intBitsToFloat(ix-s);
    return fxp1;}
float prev_befor (float x){ return prev_befor(x, 1);}

float sample_lightmap_with_shift(int xx, int yy, vec2 base_uv, float test_depth){
    vec2 pcfshift = vec2(1.0/1024.0);
    vec2 lighmap_shift = vec2(xx, yy) * pcfshift;

    float shadow = texture(lightmap, vec3(base_uv + lighmap_shift, test_depth)).r; //TODO PCF
    return shadow;
}
float sample_lightmap(vec3 world_pos, vec3 normal){
    // float b = (float((dot(normal, ubo.globalLightDir.xyz) < 0.0))*2.0 - 1.0);
    vec3 biased_pos = world_pos;
    float bias;
    if(dot(normal, ubo.globalLightDir.xyz) > 0.0){
        biased_pos -= normal*.9;
        bias = -0.002;
    } else {
        biased_pos += normal*.9;
        bias = +0.002;
    }

    vec3 light_clip = (ubo.lightmap_proj* vec4(biased_pos,1)).xyz; //move up
         light_clip.z = 1+light_clip.z;
    float world_depth = light_clip.z;
    
    // float bias = 0.0 * (float((dot(normal, ubo.globalLightDir.xyz) < 0.0))*2.0 - 1.0);

    vec2 light_uv = (light_clip.xy + 1.0)/2.0;
    // float bias = 0.0;

    const float PI = 3.15;
    const int sample_count = 1; //in theory i should do smth with temporal accumulation 
    const float max_radius = 2.0 / 1000.0;
    float angle = 00;
    float normalized_radius = 00;
    float norm_radius_step = 1.0 / float(sample_count);

    float total_light = 00;
    float total_weight = 00;
    vec2 ratio = vec2(1);

    vec2 pcfshift = vec2(1.0/1024.0);
    // [[unroll]]
    //this loop was not unrolled for whatever reason, so i did it manually
    // for(int xx=-1; xx<=+1; xx++){
    // for(int yy=-1; yy<=+1; yy++){
    //     // if((xx==00) || (yy==00)) continue;
    //     if(!((xx!=00) && (yy!=00))){
    //         vec2 lighmap_shift = vec2(xx, yy) * pcfshift;
    //         // float light_depth = texture(lightmap, vec3(light_uv + lighmap_shift, 0.0)).x; //TODO PCF

    //         float test = texture(lightmap, vec3(light_uv + lighmap_shift, world_depth)).r; //TODO PCF
    //         // float diff = abs(world_depth - light_depth);
    //         float weight = 1;
    //         total_light += test;
    //         total_weight += weight;
    //     }
    // }}

    total_light += sample_lightmap_with_shift(-1,0,light_uv,world_depth);
    total_light += sample_lightmap_with_shift(0,0,light_uv,world_depth);
    total_light += sample_lightmap_with_shift(1,0,light_uv,world_depth);
    total_light += sample_lightmap_with_shift(0,-1,light_uv,world_depth);
    total_light += sample_lightmap_with_shift(0,1,light_uv,world_depth);

    return ((total_light / 5.0)) * 0.15;
}

const float COLOR_ENCODE_VALUE = 8.0;
vec3 decode_color(vec3 encoded_color){
    return encoded_color*COLOR_ENCODE_VALUE;
}
vec3 encode_color(vec3 color){
    return color/COLOR_ENCODE_VALUE;
}

void main(void){
    vec3 final_color = vec3(0);

    const Material stored_mat = GetMat(load_mat());
    const vec3 stored_accumulated_reflection = vec3(1);
    const vec3 stored_accumulated_light = vec3(0);
    const vec3 direction = ubo.camdir.xyz;

    vec2 clip_pos = gl_FragCoord.xy / ubo.frame_size * 2.0 - 1.0;
    const vec3 origin = get_origin_from_depth(load_depth(), clip_pos);
    const      vec3 stored_normal = load_norm();

    vec3 incoming_light = sample_radiance(origin + stored_normal*6.0);
    float sunlight = sample_lightmap(origin, stored_normal);

    final_color = (2.0*incoming_light+stored_mat.emmitance + sunlight) * stored_mat.color;
    frame_color = vec4(encode_color(final_color),1);
}