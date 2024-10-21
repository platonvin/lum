#version 450 core

/*
this is basically ray tracing shader
*/
precision highp int;
precision highp float;


#include "common/ext.glsl"
#include "common/ubo.glsl"

layout(set = 0, binding = 1) uniform usampler2D matNorm;
layout(set = 0, binding = 2) uniform sampler2D depthBuffer;
layout(set = 0, binding = 3) uniform isampler3D blocks;
layout(set = 0, binding = 4) uniform usampler3D blockPalette;
layout(set = 0, binding = 5) uniform sampler2D  voxelPalette;
layout(set = 0, binding = 6) uniform sampler3D radianceCache;

// layout(location = 0) in vec2 clip_pos;
layout(location = 0) out vec4 frame_color;

// layout(constant_id = 1) const int MAX_DEPTH = 1;
// layout(constant_id = 2) const int NUM_SAMPLES = 1; //for 4090 owners i guess
layout(constant_id = 0) const int BLOCK_PALETTE_SIZE_X = 64;

const float PI = 3.1415926535;
const ivec3 world_size = ivec3(48,48,16);

 ivec2 size;
ivec2 pix;

struct Material{
    vec3 color;
    float emmitance;
    // vec3 diffuse_light;
    float roughness;
    // float transparancy;
};

vec3 get_origin_from_depth(float depth, vec2 clip_pos){
    vec3 origin = ubo.campos.xyz +
        (ubo.horizline_scaled.xyz*clip_pos.x) + 
        (ubo.vertiline_scaled.xyz*clip_pos.y) +
        (ubo.camdir.xyz*depth);
    return origin;
}

int GetBlock(in ivec3 block_pos){
    int block;
    // block = int((imageLoad(blocks, block_pos).r));
    // block = int((textureLod(blocks, (block_pos+0.5)/vec3(world_size), 0).r));
    block = int((texelFetch(blocks, (block_pos), 0).r));
    return block;
}

ivec3 voxel_in_palette(ivec3 relative_voxel_pos, int block_id) {
    int block_x = block_id % BLOCK_PALETTE_SIZE_X;
    int block_y = block_id / BLOCK_PALETTE_SIZE_X;

    return relative_voxel_pos + ivec3(16*block_x, 16*block_y, 0);
}

ivec3 voxel_in_bit_palette(ivec3 relative_voxel_pos, int block_id) {
    int block_x = block_id % BLOCK_PALETTE_SIZE_X;
    int block_y = block_id / BLOCK_PALETTE_SIZE_X;

    return relative_voxel_pos + ivec3(0+2*block_x, 0+16*block_y,0);
}

ivec2 GetVoxel(in vec3 pos){    
    int voxel;
    ivec3 ipos = ivec3(pos);
    ivec3 iblock_pos = ipos / 16;
    ivec3 relative_voxel_pos = ipos % 16;
    int block_id = GetBlock(iblock_pos);
    ivec3 voxel_pos = voxel_in_palette(relative_voxel_pos, block_id);
    voxel = int(texelFetch(blockPalette, (voxel_pos), 0).r);

    return ivec2(voxel, block_id);
}

// bool checkVoxel(in ivec3 pos){    
//     ivec3 block_pos = pos / 16;
//     int block_id = GetBlock(block_pos);

//     ivec3 relative_voxel_pos = pos % 16;
//     ivec3 bit_pos = relative_voxel_pos;
//           bit_pos.x /= 8;
//     int bit_num = relative_voxel_pos.x%8;

//     ivec3 voxel_pos = voxel_in_bit_palette(bit_pos, block_id);
//     uint voxel = (imageLoad(bitPalette, voxel_pos).x);

//     bool has_voxel = ((voxel & (1 << bit_num))!=0);
    
//     return has_voxel;
// }

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

vec3 sample_radiance(vec3 position, vec3 normal){
    vec3 block_pos = (position+normal*16.0) / 16.0;
    vec3 sampled_light = textureLod(radianceCache, (block_pos+0.5) / vec3(world_size), 0).rgb;

    return sampled_light;
}

bool initTvals(out vec3 tMax, out vec3 tDelta, out ivec3 blockPos, in vec3 rayOrigin, in vec3 rayDirection){
    vec3 effective_origin = rayOrigin;

    vec3 block_corner1 = (floor(effective_origin) - effective_origin)/rayDirection; //now corners are relative vectors
    vec3 block_corner2 = (floor(effective_origin) - effective_origin)/rayDirection  + 1.0/rayDirection;
    tMax.x = max(block_corner1.x, block_corner2.x); //1 of theese will be negative so max is just to get positive
    tMax.y = max(block_corner1.y, block_corner2.y);
    tMax.z = max(block_corner1.z, block_corner2.z);

    tDelta = 1.0 / abs(rayDirection); //how many dir vectors needeed to move 1.0 across each axys
    blockPos = ivec3(effective_origin); //round origin to block pos

    return true;
}

bool CastRay_fast(in vec3 origin, in vec3 direction, 
        out float fraction, out vec3 normal, out Material material,
        out bool left_bounds){
    bool block_hit = false;

    vec3 voxel_pos = vec3(0);
    // vec3 orig = vec3(0);
    bvec3 currentStepDiretion = bvec3(false);

    // int max_steps = 256;
    float max_dist = 16.0*8.0;

    int current_voxel = GetVoxel(origin).x;
    int current_block = GetVoxel(origin).y;

    vec3 one_div_dir = 1.0 / direction;    
    bvec3 bprecomputed_corner = greaterThan(direction, vec3(0));
    vec3 precomputed_corner = vec3(bprecomputed_corner) * 16.0;

    int iterations = 0;
    fraction = 0;
    // [[loop]]
    while (true) {
        fraction += 0.5;

        vec3 pos = origin + direction*fraction;
        // voxel_pos = ivec3(pos);

            current_voxel = GetVoxel(pos).x;
            current_block = GetVoxel(pos).y;
        
            if(current_block == 0){
                vec3 box_precomp = vec3((ivec3(pos) / 16)*16) + precomputed_corner;
                vec3 temp = -pos*one_div_dir;
                vec3 block_corner = box_precomp*one_div_dir + temp;
                
                float _f = min(block_corner.x, min(block_corner.y, block_corner.z));
                float f = max(_f,0.01); //safity
                // float f = clamp(_f,0,16.0*sqrt(3.0)); //safity

                fraction += f;
            }
        
        if (current_voxel != 0){
            block_hit = true;
            break;
        }

        if (any(lessThan(pos,vec3(0))) || any(greaterThanEqual(pos, world_size*vec3(16)))) {
            block_hit = false;
            left_bounds = true;
            break;
        }
        // if (iterations++ >= max_steps) {
        if (fraction >= max_dist) {
            block_hit = false;
            left_bounds = true;
            break;
        }
    }

    vec3 before_hit = origin + direction*(fraction-1.5);

//todo:
    if(current_voxel !=0)
    {
        ivec3 steps = ivec3(greaterThan(direction, vec3(0)));
        steps = 2 * steps + (-1);

        vec3 tMax, tDelta;
        ivec3 voxel_pos;
        
        vec3 block_corner1 = (floor(before_hit) - before_hit)/direction; //now corners are relative vectors
        vec3 block_corner2 = (floor(before_hit) - before_hit)/direction  + 1.0/direction;
        tMax.x = max(block_corner1.x, block_corner2.x); //1 of theese will be negative so max is just to get positive
        tMax.y = max(block_corner1.y, block_corner2.y);
        tMax.z = max(block_corner1.z, block_corner2.z);

        tDelta = 1.0 / abs(direction); //how many dir vectors needeed to move 1.0 across each axys
        voxel_pos = ivec3(before_hit); //round origin to block pos

        vec3 fcurrentStepDiretion = vec3(0);
        int current_voxel_id = GetVoxel(voxel_pos).x;
        int iterations = 0;
        [[unroll]] //why
        while ((iterations++ <= 4) && (current_voxel_id == 0)) {
            // initTvals(tMax, tDelta, voxel_pos, rayOrigin+dot(tMax - tDelta, fcurrentStepDiretion)*rayDirection, rayDirection); //does not intersect with scene
            // fstep_Diretion =  vec3(0);
            fcurrentStepDiretion.x = float(int((tMax.x <= tMax.y) && (tMax.x <= tMax.z)));
            fcurrentStepDiretion.y = float(int((tMax.x >= tMax.y) && (tMax.y <= tMax.z)));
            fcurrentStepDiretion.z = float(int((tMax.z <= tMax.x) && (tMax.z <= tMax.y)));

            voxel_pos += steps * ivec3(fcurrentStepDiretion);
            tMax += tDelta * fcurrentStepDiretion;

            current_voxel_id = GetVoxel(voxel_pos).x;
        }

        normal = -(vec3(steps) * fcurrentStepDiretion);
        vec3 tFinal = tMax - tDelta;
        fraction = dot(tFinal, fcurrentStepDiretion);

        material = GetMat(current_voxel_id);
    }

    return (current_voxel !=0);
}

bool CastRay_precise(in vec3 rayOrigin, in vec3 rayDirection, 
        out float fraction, out vec3 normal, out Material material,
        out bool left_bounds){
    bool block_hit = false;

    ivec3 steps;
    steps = ivec3(greaterThan(rayDirection, vec3(0)));
    steps = 2 * steps + (-1);

    vec3 fsteps = vec3(steps);

    vec3 tMax = vec3(0);
    vec3 tDelta = vec3(0);
    ivec3 voxel_pos = ivec3(0);
    float block_fraction = 0.0;
    vec3 orig = vec3(0);
    bvec3 currentStepDiretion = bvec3(false);

    initTvals(tMax, tDelta, voxel_pos, rayOrigin, rayDirection); //does not intersect with scene

    int max_steps = 128;
    int current_voxel = GetVoxel(voxel_pos).x;

    int iterations = 0;
    while (true) {
        bool xLy = tMax.x <= tMax.y;
        bool zLx = tMax.z <= tMax.x;
        bool yLz = tMax.y <= tMax.z;

        currentStepDiretion.x = ((( xLy) && (!zLx)));
        currentStepDiretion.y = ((( yLz) && (!xLy)));
        currentStepDiretion.z = ((( zLx) && (!yLz)));

        tMax += tDelta * vec3(currentStepDiretion);


        voxel_pos += steps * ivec3(currentStepDiretion);

        current_voxel = GetVoxel(voxel_pos).x;
        
        if (current_voxel != 0){
            block_hit = true;
            break;
        }
        if (any(lessThan(voxel_pos,ivec3(0))) || any(greaterThanEqual(voxel_pos, world_size*ivec3(16)))) {
            block_hit = false;
            left_bounds = true;
            break;
        }
        if (iterations++ >= max_steps) {
            block_hit = false;
            left_bounds = true;
            break;
        }
    }

    // tMax = i_tMax * tDelta;

    normal = -(fsteps * vec3(currentStepDiretion));
    vec3 tFinal = tMax - tDelta;
    block_fraction = dot(tFinal, vec3(currentStepDiretion));

    material = GetMat(current_voxel);

    fraction = block_fraction;

    return (block_hit);
}

void ProcessHit_simple(inout vec3 origin, inout vec3 direction, 
                in float fraction, in vec3 normal, in Material material, 
                inout vec3 accumulated_light, inout vec3 accumulated_reflection){

            origin = origin + (fraction * direction);
            // origin += normal * 0.001; //TODO

            vec3 diffuse_light = vec3(0);
            
            accumulated_reflection *= material.color;
            accumulated_light += accumulated_reflection * (material.emmitance + diffuse_light);

            direction = (reflect(direction, normal));
}

void ProcessHit(inout vec3 origin, inout vec3 direction, 
                in float fraction, in vec3 normal, in Material material, 
                inout vec3 accumulated_light, inout vec3 accumulated_reflection){

            origin = origin + (fraction * direction);
            // origin += normal * 0.001; //TODO

            vec3 diffuse_light = sample_radiance(origin, normal);
            // vec3 diffuse_light = vec3(0);
            
            accumulated_reflection *= material.color; 
            accumulated_light += accumulated_reflection * (material.emmitance + diffuse_light);

            direction = (reflect(direction, normal));
}

vec3 trace_glossy_ray(in vec3 rayOrigin, in vec3 rayDirection, in vec3 accumulated_light, in vec3 accumulated_reflection){
    float fraction = 0.0;
    vec3 normal = vec3(0);

    vec3 origin = rayOrigin;
    vec3 direction = rayDirection;

    vec3 light = accumulated_light;
    vec3 reflection = accumulated_reflection;
    // vec3 reflection = vec3(1);

    Material material;
    bool left_bounds = false;
    bool hit = CastRay_fast(origin, direction, fraction, normal, material, left_bounds);
    if(hit) {
        ProcessHit(origin, direction, 
            fraction, normal, material, 
            light, reflection);
    } else {
        //TODO sample skybox (lol ancient skyboxes)
        
        float global_light_participance = -dot(direction, ubo.globalLightDir.xyz);
        if (global_light_participance > 0.9) {

            light += (vec3(0.9,0.9,0.6)*0.5) * reflection * (global_light_participance-0.9)*10.0;
        } 
        // else {
            //sky blue
            light += (vec3(0.53,0.81,0.92)*0.1) * reflection;
        // }
    }
    return light;
}


vec3 rotateAxis(vec3 p, vec3 axis, float angle) {
    return mix(dot(axis, p) * axis, p, cos(angle)) + cross(axis, p) * sin(angle);
}


// vec3 load_norm(){
//     vec3 norm = ((subpassLoad(matNorm).gba)/255.0)*2.0 - 1.0;
//     return norm;
// }
// int load_mat(){
//     int mat = int(round(subpassLoad(matNorm).x));
//     return mat;
// }
vec3 load_norm(ivec2 pixel){
    vec3 norm = (((texelFetch(matNorm, pixel, 0).gba)/255.0)*2.0 - 1.0);
    return norm;
}
int load_mat(ivec2 pixel){
    int mat = int((texelFetch(matNorm, pixel, 0).x));
    return mat;
}
float load_depth(vec2 pixel){
    vec2 uv = (vec2(pixel)+0.5)/vec2(size);
    // float depth_encoded = (subpassLoad(depthBuffer).x);
    float depth_encoded = (texture(depthBuffer, uv).x);
    return (depth_encoded)*1000.0;
}
bool ssr_intersects(in float test_depth, in vec2 pix, inout bool smooth_intersection, in float current_step){
    float depth = load_depth((pix));
    float diff = test_depth - depth;
    bool ssr = false;
    smooth_intersection = false;
    // if(diff >= 0.03)    {ssr = true;}
    if(diff > 0.1)    {ssr = true;}
    //ATTENTION causes division line on low values due to rounding errors
    // if(diff >= 0.0)    {ssr = true;}
    // if(diff <  0.5) {smooth_intersection = true;}
    if(diff < 0.2) {smooth_intersection = true;}
    // smooth_intersection = true;
    return ssr;
    // return false;
}
bool ssr_traceRay(in vec3 origin, in vec3 direction, inout vec2 pix, inout float depth, out float fraction, out vec3 normal, out Material material){
    bool smooth_intersection = false;
    float fraction_step = .15;
    
    fraction = fraction_step;

    vec3 horizline = normalize(ubo.horizline_scaled.xyz); 
    vec3 vertiline = normalize(ubo.vertiline_scaled.xyz);
    //pixels movement per 1.0 of direction vector movement
    vec2 dir_proj_to_screen = (vec2(
        dot(direction,horizline),
        dot(direction,vertiline)
    ));
    float dir_proj_to_camera_dir = -(dot(direction, ubo.camdir.xyz)); //idk why
    
    // [[unroll]]
    while(fraction < 1.0){
        pix   += +dir_proj_to_screen*fraction;
        depth += +dir_proj_to_camera_dir*fraction;

        if (ssr_intersects(depth, pix, smooth_intersection, fraction_step)) {
            if(smooth_intersection){
                    normal = load_norm(ivec2(pix));
                    material = GetMat(load_mat(ivec2(pix)));
                return true;
            } else {
                // fraction -= fraction_step;
                // fraction /= fraction_mul;
                // start_pix = ssr_pix;
                return false;
            }
        }
        fraction += fraction_step;
    }
    return false;
}

const float COLOR_ENCODE_VALUE = 8.0;
vec3 decode_color(vec3 encoded_color){
    return encoded_color*COLOR_ENCODE_VALUE;
}
vec3 encode_color(vec3 color){
    return color/COLOR_ENCODE_VALUE;
}

// layout(local_size_x = 8, local_size_y = 8) in;
void main(void){
    //lowres resolution. out_frame cause in_frame is sampler
    size = textureSize(matNorm, 0);

    pix = ivec2(gl_FragCoord.xy);

    Material   mat       = GetMat(load_mat(pix));
    vec3 direction = ubo.camdir.xyz;

    vec2 clip_pos = gl_FragCoord.xy / ubo.frame_size * 2.0 - 1.0;
    vec3 origin    = get_origin_from_depth(load_depth(pix), clip_pos);
          vec3 normal    = load_norm(pix);

    vec3 accumulated_light      = vec3(0);
    vec3 accumulated_reflection = vec3(1);

    //TODO move to blend so less radiance reads happen
    ProcessHit(origin, direction, //TODO maybe remove sample radiance
            0, normal, mat, 
            accumulated_light, accumulated_reflection);

    // origin += direction*0.5;
    Material ssr_mat;
    float ssr_fraction;
    vec3 ssr_normal;
    vec2 current_pixel = vec2(gl_FragCoord);
    float current_depth = load_depth(pix);
    
    // if(GetVoxel(ivec3(origin)).x != 0){
    //     bool ssr_hit = ssr_traceRay(origin, direction, current_pixel, current_depth, ssr_fraction, ssr_normal, ssr_mat);
    //     origin += ssr_fraction * direction;
    //     if(ssr_hit){
    //         ProcessHit(origin, direction, 
    //             0, normal, mat, 
    //             accumulated_light, accumulated_reflection);
    //     }
    // }
    // ssr_hit = ssr_traceRay(origin, direction, current_pixel, current_depth, ssr_fraction, ssr_normal, ssr_mat);
    // origin += ssr_fraction * direction;
    // if(ssr_hit){
    //     ProcessHit(origin, direction, 
    //         0, normal, mat, 
    //         accumulated_light, accumulated_reflection);
    // }

    vec3 traced_color = trace_glossy_ray(origin, direction, accumulated_light, accumulated_reflection);
    
    frame_color = vec4(encode_color(traced_color), 1.0-mat.roughness);
    // frame_color = vec4(1);
}