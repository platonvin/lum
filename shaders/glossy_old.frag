#version 450 core

/*
this is basically cone tracing shader (lowres)
ssr for mirrors but counting more than one "hit pixels" for rough reflections
*/

precision highp int;
precision highp float;
#define    varp highp

// #extension GL_KHR_shader_subgroup_arithmetic : enable

layout(push_constant) uniform constants{
    varp vec4 camera_pos;
    varp vec4 camera_direction;
} PushConstants;

layout(set = 0, binding = 0, rgba8_snorm) uniform image2D     matNorm;
layout(set = 0, binding = 1) uniform sampler2D depthBuffer;
// layout(input_attachment_index = 0, set = 0, binding = 0) uniform subpassInput matNorm;
// layout(input_attachment_index = 1, set = 0, binding = 1) uniform subpassInput depthBuffer;

layout(set = 0, binding = 2, r16i       ) uniform iimage3D  blocks;
layout(set = 0, binding = 3, r8ui       ) uniform uimage3D  blockPalette;
layout(set = 0, binding = 4, r32f       ) uniform image2D   voxelPalette;
layout(set = 0, binding = 5, rgb10_a2     ) uniform image3D   radianceCache;
layout(set = 0, binding = 6, r8ui       ) uniform uimage3D distancePalette;
// layout(set = 0, binding = 6             ) uniform sampler3D distancePalette;
layout(set = 0, binding = 7, r8ui       ) uniform uimage3D bitPalette;

layout(constant_id = 1) const int MAX_DEPTH = 1;
layout(constant_id = 2) const int NUM_SAMPLES = 1; //for 4090 owners i guess
layout(constant_id = 3) const int BLOCK_PALETTE_SIZE_X = 64;
layout(constant_id = 4) const int size_height = (0);

layout(location = 0) in vec2 non_clip_pos;
layout(location = 0) out vec4 frame_color;

varp vec3 globalLightDir;
varp vec3 cameraRayDirPlane;
varp vec3 horizline;
varp vec3 vertiline;

const varp float PI = 3.1415926535;
const varp float FAR_DISTANCE = 100000.0;

const varp ivec3 world_size = ivec3(48,48,16);

const varp float view_width  = 1920.0 / 10.0; //in block_diags
const varp float view_height = 1080.0 / 10.0; //in blocks

varp  ivec2 size;
highp ivec2 pix;

struct Material{
    varp vec3 color;
    varp float emmitance;
    varp vec3 diffuse_light;
    varp float roughness;
    // varp float transparancy;
};

varp vec3 get_origin_from_depth(varp float depth, varp vec2 uv_pos){
    const varp vec2 view_size = vec2(view_width, view_height);
    const varp vec2 clip_pos_scaled = (2.0*view_size)*(uv_pos)-view_size;
    
    varp vec3 origin = PushConstants.camera_pos.xyz + 
        (horizline*clip_pos_scaled.x) + 
        (vertiline*clip_pos_scaled.y) +
        (PushConstants.camera_direction.xyz*depth);
    return origin;
}

varp int GetBlock(in varp  ivec3 block_pos){
    varp int block;
    block = int((imageLoad(blocks, block_pos).r));
    return block;
}

varp ivec3 voxel_in_palette(varp ivec3 relative_voxel_pos, varp int block_id) {
    varp int block_x = block_id % BLOCK_PALETTE_SIZE_X;
    varp int block_y = block_id / BLOCK_PALETTE_SIZE_X;

    return relative_voxel_pos + ivec3(16*block_x, 16*block_y, 0);
}

ivec3 voxel_in_bit_palette(ivec3 relative_voxel_pos, int block_id) {
    int block_x = block_id % BLOCK_PALETTE_SIZE_X;
    int block_y = block_id / BLOCK_PALETTE_SIZE_X;

    return relative_voxel_pos + ivec3(0+2*block_x, 0+16*block_y,0);
}

varp int GetVoxel(in varp ivec3 pos){    
    varp int voxel;
    varp ivec3 block_pos = pos / 16;
    varp ivec3 relative_voxel_pos = pos % 16;
    varp int block_id = GetBlock(block_pos);
    varp ivec3 voxel_pos = voxel_in_palette(relative_voxel_pos, block_id);
    voxel = int(imageLoad(blockPalette, voxel_pos).r);

    return voxel;
}

bool checkVoxel(in varp ivec3 pos){    
    varp ivec3 block_pos = pos / 16;
    varp int block_id = GetBlock(block_pos);

    varp ivec3 relative_voxel_pos = pos % 16;
    ivec3 bit_pos = relative_voxel_pos;
          bit_pos.x /= 8;
    int bit_num = relative_voxel_pos.x%8;

    varp ivec3 voxel_pos = voxel_in_bit_palette(bit_pos, block_id);
    uint voxel = (imageLoad(bitPalette, voxel_pos).x);

    bool has_voxel = ((voxel & (1 << bit_num))!=0);
    
    return has_voxel;
}

void SetVoxel(in varp ivec3 pos, in varp uint voxel){
    varp ivec3 block_pos = pos / 16;
    varp ivec3 relative_voxel_pos = pos % 16;

    varp int block_id = GetBlock(block_pos);
    varp ivec3 voxel_pos = voxel_in_palette(relative_voxel_pos, block_id);
    
    imageStore(blockPalette, voxel_pos, uvec4(voxel));
}

Material GetMat(in varp int voxel){
    Material mat;

    mat.color.r      = imageLoad(voxelPalette, ivec2(0,voxel)).r;
    mat.color.g      = imageLoad(voxelPalette, ivec2(1,voxel)).r;
    mat.color.b      = imageLoad(voxelPalette, ivec2(2,voxel)).r;
    // mat.transparancy = 1.0 - imageLoad(voxelPalette, ivec2(3,voxel)).r;
    mat.emmitance    =       imageLoad(voxelPalette, ivec2(4,voxel)).r;
    mat.roughness    =       imageLoad(voxelPalette, ivec2(5,voxel)).r;
    // mat.roughness = 1.0;
    // mat.transparancy = 0;

    // mat.smoothness = 0.5;
    // mat.smoothness = 0;
    // if(voxel < 30) 
    // mat.color.rgb = vec3(0.9);
    // mat.color.rgb = clamp(mat.color.rgb,0.2,1);
    // mat.emmitance = .0;
return mat;
}

vec3 sample_probe(ivec3 probe_ipos, vec3 direction){
    // int closest_id = clamp(int(inverseFibonacciPoint(direction).x), 0,RAYS_PER_PROBE-1);
    ivec3 probe_ipos_clamped = clamp(probe_ipos, ivec3(0), world_size);
    ivec3 subprobe_pos;
          subprobe_pos.x  = probe_ipos_clamped.x; //same as local_pos actually but its optimized away and worth it for modularity
          subprobe_pos.yz = probe_ipos_clamped.yz; //reuses but its optimized away
    // vec3 light = imageLoad(radianceCache, clamp(subprobe_pos, ivec3(0), world_size)).xyz;
    vec3 light = imageLoad(radianceCache, (subprobe_pos)).xyz;
    return clamp(light, 0, 2);

    // NearestPoint[4] nearestPoints = findNearestPoints(direction, RAYS_PER_PROBE);

    // // Calculate inverse distances and normalize weights
    // float totalWeight = 0.0;
    // float weights[4];
    // for (int i = 0; i < 4; i++) {
    //     weights[i] = 1.0 / nearestPoints[i].distance;
    //     totalWeight += weights[i];
    // }

    // for (int i = 0; i < 4; i++) {
    //     weights[i] /= totalWeight;
    // }

    // // Interpolate colors
    // vec3 interpolatedColor = vec3(0.0);
    // for (int i = 0; i < 4; i++) {
    //     interpolatedColor += weights[i] * get_color(int(nearestPoints[i].index), probe_ipos);
    // }

    // return interpolatedColor;
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
        //weird algo to make it little more readable and maybe low-instruction-cache-but-fast-loops gpu's friendly (if they will ever exist)
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


// Material GetMat(in varp int voxel, in vec3 pos){
//     Material mat = GetMat(voxel);

//     mat.diffuse_light = 
// return mat;
// }

bool initTvals(out varp vec3 tMax, out varp vec3 tDelta, out varp ivec3 blockPos, in varp vec3 rayOrigin, in varp vec3 rayDirection){
    varp vec3 effective_origin = rayOrigin;

    varp vec3 block_corner1 = (floor(effective_origin) - effective_origin)/rayDirection; //now corners are relative vectors
    varp vec3 block_corner2 = (floor(effective_origin) - effective_origin)/rayDirection  + 1.0/rayDirection;
    tMax.x = max(block_corner1.x, block_corner2.x); //1 of theese will be negative so max is just to get positive
    tMax.y = max(block_corner1.y, block_corner2.y);
    tMax.z = max(block_corner1.z, block_corner2.z);

    tDelta = 1.0 / abs(rayDirection); //how many dir vectors needeed to move 1.0 across each axys
    blockPos = ivec3(effective_origin); //round origin to block pos

    return true;
}
// vec2 intersectAABB(vec3 rayOrigin, vec3 rayDir, vec3 boxMin, vec3 boxMax) {
//     vec3 tMin = (boxMin - rayOrigin) / rayDir;
//     vec3 tMax = (boxMax - rayOrigin) / rayDir;
//     vec3 t1 = min(tMin, tMax);
//     vec3 t2 = max(tMin, tMax);
//     float tNear = max(max(t1.x, t1.y), t1.z);
//     float tFar = min(min(t2.x, t2.y), t2.z);
//     return vec2(tNear, tFar);
// }

bool CastRay_fast(in varp vec3 origin, in varp vec3 direction, 
        out varp float fraction, out varp vec3 normal, out Material material,
        out bool left_bounds){
    bool block_hit = false;

    varp ivec3 voxel_pos = ivec3(0);
    // vec3 orig = vec3(0);
    bvec3 currentStepDiretion = bvec3(false);

    // int max_steps = 128;
    float max_dist = 16.0*8.0;
    bool current_voxel = checkVoxel(voxel_pos);

    vec3 one_div_dir = 1.0 / direction;    
    vec3 precomputed_corner;
        precomputed_corner.x = (direction.x >0)? 16.0 : 0.0; 
        precomputed_corner.y = (direction.y >0)? 16.0 : 0.0; 
        precomputed_corner.z = (direction.z >0)? 16.0 : 0.0; 

    varp int iterations = 0;
    fraction = 0;
    while (true) {
        fraction += 0.5;

        vec3 pos = origin + direction*fraction;
        voxel_pos = ivec3(pos);

        // orig += dot(tDelta, vec3(currentStepDiretion)) * direction;
        // current_voxel = GetVoxel(voxel_pos);
            varp int voxel;
            varp ivec3 block_pos = voxel_pos / 16;
            varp ivec3 relative_voxel_pos = voxel_pos % 16;
            varp int block_id = GetBlock(block_pos);
            varp ivec3 _voxel_pos = voxel_in_palette(relative_voxel_pos, block_id);
            voxel = int(imageLoad(blockPalette, _voxel_pos).r);
            current_voxel = voxel!=0;
            // ivec3 bit_pos = relative_voxel_pos;
            //     bit_pos.x /= 8;
            // int bit_num = relative_voxel_pos.x%8;

            // varp ivec3 _voxel_pos = voxel_in_bit_palette(bit_pos, block_id);
            // uint _voxel = (imageLoad(bitPalette, _voxel_pos).x);
            // bool has_voxel = ((_voxel & (1 << bit_num))!=0);
            // current_voxel = has_voxel;
            // current_voxel = (checkVoxel(voxel_pos));

            // float distance_from_df = sqrt(float(int(imageLoad(distancePalette, _voxel_pos).r)));
            // fraction += distance_from_df;
            // if(distance_from_df == 0){
            //     fraction += 10;
            // }
            // float distance_from_df = 0;
            // float distance_from_df = texture(distancePalette, _voxel_pos).r));

            // if block_id is zero - teleport to border
            if(block_id == 0){
                // vec3 box_min = vec3(block_pos)*16.0;
                // vec3 box_max = box_min+16.0;
                vec3 box_precomp = vec3(block_pos)*16.0 + precomputed_corner;

                vec3 temp = -pos*one_div_dir;
                
                // varp vec3 block_corner1 = box_min*one_div_dir + temp; //now corners are relative vectors
                // varp vec3 block_corner2 = box_max*one_div_dir + temp;
                varp vec3 block_corner = box_precomp*one_div_dir + temp;
                
                vec3 _t;
                // _t.x = max(block_corner1.x, block_corner2.x); //1 of theese will be negative so max is just to get positive
                // _t.y = max(block_corner1.y, block_corner2.y);
                // _t.z = max(block_corner1.z, block_corner2.z);
                _t = block_corner; 

                float _f = min(_t.x, min(_t.y, _t.z));
                float f = max(_f,0.001); //safity

                fraction += f;
            }
        
        if (current_voxel){
            block_hit = true;
            break;
        }
        if (any(lessThan(voxel_pos,ivec3(0))) || any(greaterThanEqual(voxel_pos, world_size*ivec3(16)))) {
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

    vec3 before_hit = origin + direction*(fraction-1.0);

//todo:
    if(current_voxel){

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
        int current_voxel_id = GetVoxel(voxel_pos);
        int iterations = 0;
        while ((iterations++ <= 2) && (current_voxel_id == 0)) {
            // initTvals(tMax, tDelta, voxel_pos, rayOrigin+dot(tMax - tDelta, fcurrentStepDiretion)*rayDirection, rayDirection); //does not intersect with scene
            // fstep_Diretion =  vec3(0);
            fcurrentStepDiretion.x = float(int((tMax.x <= tMax.y) && (tMax.x <= tMax.z)));
            fcurrentStepDiretion.y = float(int((tMax.x >= tMax.y) && (tMax.y <= tMax.z)));
            fcurrentStepDiretion.z = float(int((tMax.z <= tMax.x) && (tMax.z <= tMax.y)));

            voxel_pos += steps * ivec3(fcurrentStepDiretion);
            tMax += tDelta * fcurrentStepDiretion;

            current_voxel_id = GetVoxel(voxel_pos);
        }

        normal = -(vec3(steps) * fcurrentStepDiretion);
        vec3 tFinal = tMax - tDelta;
        fraction = dot(tFinal, fcurrentStepDiretion);

        material = GetMat(current_voxel_id);
    }

    return (block_hit);
}

bool CastRay_precise(in varp vec3 rayOrigin, in varp vec3 rayDirection, 
        out varp float fraction, out varp vec3 normal, out Material material,
        out bool left_bounds){
    bool block_hit = false;

    varp ivec3 steps;
    steps = ivec3(greaterThan(rayDirection, vec3(0)));
    steps = 2 * steps + (-1);

    varp vec3 fsteps = vec3(steps);

    // varp ivec3 i_tMax = ivec3(0);
    varp vec3 tMax = vec3(0);
    varp vec3 tDelta = vec3(0);
    varp ivec3 voxel_pos = ivec3(0);
    varp float block_fraction = 0.0;
    vec3 orig = vec3(0);
    bvec3 currentStepDiretion = bvec3(false);

    initTvals(tMax, tDelta, voxel_pos, rayOrigin, rayDirection); //does not intersect with scene

    int max_steps = 128;
    varp int current_voxel = GetVoxel(voxel_pos);

    varp int iterations = 0;
    // float _fraction = 0;
    while (true) {
        bool xLy = tMax.x <= tMax.y;
        bool zLx = tMax.z <= tMax.x;
        bool yLz = tMax.y <= tMax.z;

        // ivec3 bools_xyz =    ivec3(xLy, yLz, zLx);
        // ivec3 bools_n_zxy = ~ivec3((zLx, xLy, yLz));

        //LOL no perfomance benefit currently but it was there last time i tested it TODO
        // currentStepDiretion = ivec3(bools_xyz & bools_n_zxy);
        currentStepDiretion.x = ((( xLy) && (!zLx)));
        currentStepDiretion.y = ((( yLz) && (!xLy)));
        currentStepDiretion.z = ((( zLx) && (!yLz)));

        tMax += tDelta * vec3(currentStepDiretion);

        // _fraction += dot(tDelta, vec3(currentStepDiretion));

        voxel_pos += steps * ivec3(currentStepDiretion);

        // orig += dot(tDelta, vec3(currentStepDiretion)) * rayDirection;
        // current_voxel = GetVoxel(voxel_pos);
            varp int voxel;
            varp ivec3 block_pos = voxel_pos / 16;
            varp ivec3 relative_voxel_pos = voxel_pos % 16;
            varp int block_id = GetBlock(block_pos);
            //if block_id is zero - teleport to border
            varp ivec3 _voxel_pos = voxel_in_palette(relative_voxel_pos, block_id);
            voxel = int(imageLoad(blockPalette, _voxel_pos).r);
            current_voxel = voxel;

            // if(block_id == 0){
            //         varp vec3 tFinal = tMax - tDelta;
            //         // block_fraction += dot(tFinal, vec3(currentStepDiretion));
            //     vec3 orig = rayOrigin+_fraction*rayDirection;

            //     vec3 box_min = vec3((voxel_pos/16)*16);
            //     vec3 box_max = vec3((voxel_pos/16)*16+16);
            //     // vec2 skip;

            //         varp vec3 block_corner1 = (box_min - orig)/rayDirection; //now corners are relative vectors
            //         varp vec3 block_corner2 = (box_max - orig)/rayDirection;
            //         vec3 _t;
            //         _t.x = max(block_corner1.x, block_corner2.x); //1 of theese will be negative so max is just to get positive
            //         _t.y = max(block_corner1.y, block_corner2.y);
            //         _t.z = max(block_corner1.z, block_corner2.z);

            //         _fraction += min(_t.x, min(_t.y, _t.z))-0.0001;
                    
            //         // tMax += _t;
            //         initTvals(tMax, tDelta, voxel_pos, rayOrigin+_fraction*rayDirection, rayDirection); //does not intersect with scene
            //         // {
            //         //     varp vec3 block_corner1 = (floor(effective_origin) - effective_origin)/rayDirection; //now corners are relative vectors
            //         //     varp vec3 block_corner2 = (floor(effective_origin) - effective_origin)/rayDirection  + 1.0/rayDirection;
            //         //     tMax.x = max(block_corner1.x, block_corner2.x); //1 of theese will be negative so max is just to get positive
            //         //     tMax.y = max(block_corner1.y, block_corner2.y);
            //         //     tMax.z = max(block_corner1.z, block_corner2.z);

            //         //     blockPos = ivec3(effective_origin); //round origin to block pos
            //         // }
            //         // initTvals()
            //     // block_fraction += skip.x;
            //         varp int voxel;
            //         varp ivec3 block_pos = voxel_pos / 16;
            //         varp ivec3 relative_voxel_pos = voxel_pos % 16;
            //         varp int block_id = GetBlock(block_pos);
            //         //if block_id is zero - teleport to border
            //         varp ivec3 _voxel_pos = voxel_in_palette(relative_voxel_pos, block_id);
            //         voxel = int(imageLoad(blockPalette, _voxel_pos).r);
            //         current_voxel = voxel;
            //     // tMax = vec3(0);
            // }
            

        
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
    varp vec3 tFinal = tMax - tDelta;
    block_fraction = dot(tFinal, vec3(currentStepDiretion));
    // block_fraction += dot(tFinal, vec3(currentStepDiretion));

    material = GetMat(current_voxel);
    // vec3 diffuse_light = sample_radiance(rayOrigin + rayDirection*block_fraction, normal);

    fraction = block_fraction;
    // fraction = _fraction;

    return (block_hit);
}

void ProcessHit(inout varp vec3 origin, inout varp vec3 direction, 
                in varp float fraction, in varp vec3 normal, in Material material, 
                inout varp vec3 accumulated_light, inout varp vec3 accumulated_reflection){

            varp vec3 new_origin = origin + (fraction * direction);
            // varp vec3 new_origin = origin;

            varp vec3 new_direction;

            bool refracted = false;//TODO:
            if (refracted)
            {
                // vec3 idealRefraction = IdealRefract(direction, normal, nIN, nOUT);
                // new_direction = normalize(mix(-new_direction, idealRefraction, material.smoothness));
                // // newRayDirection = normalize(mix(idealRefraction, -newRayDirection, material.smoothness));
                // new_origin += normal * (dot(new_direction, normal) < 0.0 ? -0.001 : 0.001);
            }
            else
            {
                varp vec3 idealReflection = reflect(direction, normal);
                new_direction = idealReflection; //conetracing just works this way
                // new_direction = normalize(mix(idealReflection, new_direction, material.roughness)); //conetracing just works this way
                new_origin += normal * 0.001; //TODO

                vec3 diffuse_light = sample_radiance(new_origin, normal);
                material.diffuse_light = diffuse_light;
                
                accumulated_reflection *= material.color;

                accumulated_light += accumulated_reflection * (material.emmitance + material.diffuse_light*material.color);
                // accumulated_light += accumulated_reflection * ;
                // accumulated_light += vec3(0.8) * 0.1 * accumulated_reflection;
// 
                // angle += PI * material.roughness;
                // angle = clamp(angle, 0.0, PI/4.0);
                // angle = PI/2.0;

                // angle = 0.0;
                // new_direction = normalize(mix(idealReflection, new_direction, material.roughness)); //conetracing just works this way
            }

            // direction = reflect(direction,normal);
            direction = new_direction;
            origin = new_origin;

}
float hypot (vec2 z) {
  float t;
  float x = abs(z.x);
  float y = abs(z.y);
  t = min(x, y);
  x = max(x, y);
  t = t / x;
  return (z.x == 0.0 && z.y == 0.0) ? 0.0 : x * sqrt(1.0 + t * t);
}
// float area(float dist, float r1, float r2) {
//     float d = hypot(vec2(B.x - A.x, B.y - A.y));

//     if (d < A.r + B.r) {

//         Arr = A.r * A.r;
//         Brr = B.r * B.r;

//         if (d <= abs(B.r - A.r)) {
//         return PI * min(Arr, Brr)
//         }

//         tA = 2 * acos((Arr + d * d - Brr) / (2 * d * A.r))
//         tB = 2 * acos((Brr + d * d - Arr) / (2 * d * B.r))

//         return 0.5 * (Arr * (tA - sin(tA)) + Brr * (tB - sin(tB)))
//     }
//     return 0
// }

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
    bool hit;
    // for (int i = 0; (i < MAX_DEPTH); i++)
    // {
        // left_bounds = false;
        // hit = CastRay_precise(origin, direction, fraction, normal, material, left_bounds);
        // hit = CastRay_fast(rayOrigin, direction, fraction, normal, material, 9999.0, cone_diameter, cone_angle, left_bounds);
        // dir_before_hit = direction;
        // material.roughness = 0;
        // if(hit) light = vec3(1); 
        // if(!hit) break;
        // if(length(reflection) < 0.01 || length(light) > sqrt(3.0)) break;
    // }
        
    hit = CastRay_fast(origin, direction, fraction, normal, material, left_bounds);
    if(hit) {
        ProcessHit(origin, direction, 
            fraction, normal, material, 
            light, reflection);
    } else {
        varp float global_light_participance = -dot(direction, globalLightDir);
        if (global_light_participance > 0.95) {
        // if (global_light_participance > 0.95) {
            light += (vec3(0.9,0.9,0.6)*0.5) * reflection * global_light_participance;
        } 
        else {
            //sky blue
            light += (vec3(0.53,0.81,0.92)*0.1) * reflection;
        }
    }
    // if(any(greaterThan(reflection, vec3(1)))) return vec3(0);
    // varp float ndot = (dot(normal, globalLightDir));
    // varp float _global_light_participance = (ndot>0)? material.roughness * ndot * 0.5 : 0.0;
    // light += (vec3(.9,.9,.6)*3.0) * reflection * _global_light_participance / 2.0;
    return light;
}


vec3 rotateAxis(vec3 p, vec3 axis, float angle) {
    return mix(dot(axis, p) * axis, p, cos(angle)) + cross(axis, p) * sin(angle);
}


// varp vec3 load_norm(){
//     // varp vec3 norm = (imageLoad(matNorm, pixel).gba);
//     varp vec3 norm = (subpassLoad(matNorm).gba);
//     // subpass
//     return norm;
// }
varp vec3 load_norm(highp ivec2 pixel){
    varp vec3 norm = (imageLoad(matNorm, pixel).gba);
    return norm;
}
// varp int load_mat(){
//     varp int mat = int(round(subpassLoad(matNorm).x*127.0))+127;
//     // varp int mat = int(subpassLoad(matNorm).x);
//     return mat;
// }
varp int load_mat(highp ivec2 pixel){
    varp int mat = int(round(imageLoad(matNorm, pixel).x*127.0))+127;
    return mat;
}
highp float load_depth(vec2 pixel){
    highp vec2 uv = (vec2(pixel)+0.5)/vec2(size);
    // highp float depth_encoded = (subpassLoad(depthBuffer).x);
    highp float depth_encoded = (texture(depthBuffer, uv).x);
    return (1.0-depth_encoded)*1000.0;
}
bool ssr_intersects(in varp float test_depth, in varp vec2 pix, inout bool smooth_intersection){
    highp float depth = load_depth((pix));
    varp float diff = test_depth - depth;
    bool ssr = false;
    smooth_intersection = false;
    // if(diff >= 0.03)    {ssr = true;}
    if(diff > 0.2)    {ssr = true;}
    //ATTENTION causes division line on low values due to rounding errors
    // if(diff >= 0.0)    {ssr = true;}
    // if(diff <  0.5) {smooth_intersection = true;}
    if(diff <  1.0) {smooth_intersection = true;}
    // smooth_intersection = true;
    return ssr;
    // return false;
}
bool ssr_traceRay(in varp vec3 origin, in varp vec3 direction, in highp vec2 start_pix, out highp float fraction, out varp vec3 normal, out Material material){
    bool smooth_intersection = false;
    highp float fraction_add = .15;
    highp float fraction_mul = 1.0;
    
    fraction = 0.0;
    fraction = fraction_add;

    highp vec2 ssr_pix = vec2(0);
    highp float depth = 0;
    while(true){
        //TODO turn into cached step 
        highp vec3 new_origin = origin + direction*fraction;
        highp vec3 relative_pos = new_origin - PushConstants.camera_pos.xyz;
        depth = dot(relative_pos, PushConstants.camera_direction.xyz);
        ssr_pix.x = ((dot(relative_pos, horizline)/(view_width *2.0)) +0.5)*float(size.x);
        ssr_pix.y = ((dot(relative_pos, vertiline)/(view_height*2.0)) +0.5)*float(size.y);

        if (ssr_intersects(depth, ssr_pix, smooth_intersection)) {
            if(smooth_intersection){
                    normal = load_norm(ivec2(ssr_pix));
                    material = GetMat(load_mat(ivec2(ssr_pix)));
                return true;
            } else {
                fraction -= fraction_add;
                // fraction /= fraction_mul;
                return false;
            }
        }
        fraction += fraction_add;
        // fraction *= fraction_mul;
        if (fraction > 1.5) return false;
    }
}



varp float luminance(varp vec3 color){
    varp vec3 luminance_const = vec3(0.2126, 0.7152, 0.0722);
    return dot(color, luminance_const);
}
const varp float COLOR_ENCODE_VALUE = 5.0;
varp vec3 decode_color(varp vec3 encoded_color){
    return clamp(encoded_color,0,1)*vec3(COLOR_ENCODE_VALUE);
}
varp vec3 encode_color(varp vec3 color){
    return clamp(color/vec3(COLOR_ENCODE_VALUE), 0,1);
}

//TODO: balance
// layout(local_size_x = 8, local_size_y = 8) in;
void main(void){
    //lowres resolution. out_frame cause in_frame is sampler
    size = imageSize(matNorm);

    globalLightDir = normalize(vec3(0.5, 0.5, -0.9));

    // globalLightDir = normalize(vec3(1, 1, -1.6));
    // globalLightDir = rotateAxis(globalLightDir, vec3(0,0,1), PushConstants.timeSeed.x / 100.0);
    
    cameraRayDirPlane = normalize(vec3(PushConstants.camera_direction.xy, 0));
    horizline = normalize(cross(cameraRayDirPlane, vec3(0,0,1)));
    vertiline = normalize(cross(PushConstants.camera_direction.xyz, horizline));
    
    pix = ivec2(gl_FragCoord.xy);
    // if (pix.x >= size.x || pix.y >= size.y) {return;}
    // const varp vec2 pos = vec2(pix) / vec2(size.x - 1, size.y - 1);

          Material mat = GetMat(load_mat(pix));
    highp     vec3 direction = PushConstants.camera_direction.xyz;
    highp     vec3 origin = get_origin_from_depth(load_depth(pix), non_clip_pos);
    varp      vec3 normal = load_norm(pix);

    // vec3 direction = reflect(init_direction, normal); 

    // if(mat.roughness >.5){
    //     frame_color = vec4(vec3(0),0);
    //     return;
    // }

    vec3 accumulated_light = vec3(0);
    vec3 accumulated_reflection = vec3(1);

    // origin += normal*0.01;
    ProcessHit(origin, direction, 
            0, normal, mat, 
            accumulated_light, accumulated_reflection);

    Material ssr_mat;
    float ssr_fraction;
    vec3 ssr_normal;
    bool ssr_hit = ssr_traceRay(origin, direction, vec2(0), ssr_fraction, ssr_normal, ssr_mat);
    
    origin += ssr_fraction * direction;
    if(ssr_hit){
        ProcessHit(origin, direction, 
            0, normal, mat, 
            accumulated_light, accumulated_reflection);
    }
    ssr_hit = ssr_traceRay(origin, direction, vec2(0), ssr_fraction, ssr_normal, ssr_mat);
    origin += ssr_fraction * direction;
    if(ssr_hit){
        ProcessHit(origin, direction, 
            0, normal, mat, 
            accumulated_light, accumulated_reflection);
    }

    vec3 traced_color = trace_glossy_ray(origin, direction, accumulated_light, accumulated_reflection);
    
    // vec3 old_color = imageLoad(out_frame, pix).xyz;

    // vec3 new_color = traced_color;

    // imageStore(out_frame, pix, vec4((new_color), 1));

    //than blurs to imitate real roughness
    frame_color = vec4(traced_color, 1.0-mat.roughness);
    // frame_color = vec4(traced_color, 1.0);

    // frame_color = vec4(.5,.6,.7, 1);
}