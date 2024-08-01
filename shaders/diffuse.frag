#version 450 core

/*
shader to diffuse-color lowres frame
ambient + "radiant" diffuse
*/

precision highp int;
precision highp float;
// #extension GL_KHR_shader_subgroup_arithmetic : enable

#define varp highp

layout(push_constant) uniform constants{
    varp vec3 camera_pos;
    varp  int timeSeed;
    varp  vec4 camera_direction;
} PushConstants;

layout(input_attachment_index = 0, set = 0, binding = 0) uniform subpassInput matNorm;
layout(input_attachment_index = 1, set = 0, binding = 1) uniform subpassInput depthBuffer;

layout(set = 0, binding = 2, r16i       ) uniform iimage3D  blocks;
layout(set = 0, binding = 3, r8ui       ) uniform uimage3D  blockPalette;
layout(set = 0, binding = 4, r32f       ) uniform image2D   voxelPalette;
layout(set = 0, binding = 5, rgba16     ) uniform image3D   radianceCache;

layout(location = 0) in vec2 non_clip_pos;
layout(location = 0) out vec4 frame_color;

#define RAYS_PER_PROBE (32)
ivec2 size;
const varp ivec3 world_size = ivec3(48,48,16);
const varp float view_width  = 1920.0 / 10.0; //in block_diags
const varp float view_height = 1080.0 / 10.0; //in blocks

vec3 cameraRayDirPlane;
vec3 horizline;
vec3 vertiline;

vec3 sample_probe(ivec3 probe_ipos, vec3 direction){
    ivec3 probe_ipos_clamped = clamp(probe_ipos, ivec3(0), world_size);
    ivec3 subprobe_pos;
          subprobe_pos.x  = probe_ipos_clamped.x; //same as local_pos actually but its optimized away and worth it for modularity
          subprobe_pos.yz = probe_ipos_clamped.yz; //reuses but its optimized away
    // vec3 light = imageLoad(radianceCache, clamp(subprobe_pos, ivec3(0), world_size)).xyz;
    vec3 light = imageLoad(radianceCache, (subprobe_pos)).xyz;
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

struct Material{
    varp vec3 color;
    varp float emmitance;
    varp float roughness;
    varp float transparancy;
};
varp vec3 load_norm(){
    // varp vec3 norm = (imageLoad(matNorm, pixel).gba);
    varp vec3 norm = (subpassLoad(matNorm).gba);
    // subpass
    return norm;
}
varp int load_mat(){
    varp int mat = int(round(subpassLoad(matNorm).x*127.0))+127;
    // varp int mat = int(subpassLoad(matNorm).x);
    return mat;
}
highp float load_depth(){
    // highp vec2 uv = (vec2(pixel)+0.5)/vec2(size);
    highp float depth_encoded = (subpassLoad(depthBuffer).x);
    return (1.0-depth_encoded)*1000.0;
}
Material GetMat(in varp int voxel){
    Material mat;

    mat.color.r      = imageLoad(voxelPalette, ivec2(0,voxel)).r;
    mat.color.g      = imageLoad(voxelPalette, ivec2(1,voxel)).r;
    mat.color.b      = imageLoad(voxelPalette, ivec2(2,voxel)).r;
    // mat.transparancy = 1.0 - imageLoad(voxelPalette, ivec2(3,voxel)).r;
    mat.emmitance    =       imageLoad(voxelPalette, ivec2(4,voxel)).r;
    mat.roughness    =       imageLoad(voxelPalette, ivec2(5,voxel)).r;
    mat.transparancy = 0;

    // mat.smoothness = 0.5;
    // mat.smoothness = 0;
    // if(voxel < 30) 
    // mat.color.rgb = vec3(0.9);
    // mat.color.rgb = clamp(mat.color.rgb,0.2,1);
    // mat.emmitance = .0;
return mat;
}
varp vec3 get_origin_from_depth(varp float depth, varp vec2 uv_pos){
    const varp vec2 view_size = vec2(view_width, view_height);
    const varp vec2 clip_pos_scaled = (2.0*view_size)*(uv_pos)-view_size;
    
    varp vec3 origin = PushConstants.camera_pos.xyz + 
        (horizline*clip_pos_scaled.x) + 
        (vertiline*clip_pos_scaled.y) +
        (PushConstants.camera_direction.xyz*depth);
    return origin;
}


// layout(local_size_x = 8, local_size_y = 8) in;
void main(void){
    // size = imageSize(matNorm);

    vec3 globalLightDir = normalize(vec3(0.5, 0.5, -0.9));

    cameraRayDirPlane = normalize(vec3(PushConstants.camera_direction.xy, 0));
    horizline = normalize(cross(cameraRayDirPlane, vec3(0,0,1)));
    vertiline = normalize(cross(PushConstants.camera_direction.xyz, horizline));
    
    ivec2 pix = ivec2(gl_FragCoord.xy);
    const varp vec2 pos = vec2(pix) / vec2(size.x - 1, size.y - 1);

    vec3 final_color = vec3(0);

    const       Material stored_mat = GetMat(load_mat());
    const varp      vec3 stored_accumulated_reflection = vec3(1);
    const varp      vec3 stored_accumulated_light = vec3(0);
    const highp      vec3 stored_direction = PushConstants.camera_direction.xyz;
    const highp      vec3 stored_origin = get_origin_from_depth(load_depth(), non_clip_pos);
    // const highp      vec3 stored_origin = get_origin_from_depth(load_depth(), pos);
    const varp      vec3 stored_normal = load_norm();

    vec3 incoming_light = sample_radiance(stored_origin + 0.1*stored_normal, stored_normal);

    final_color = 2.0* incoming_light * stored_mat.color;
    // final_color = stored_mat.color;
    // final_color = incoming_light;
    // final_color = stored_origin;
    // final_color = vec3(load_depth()/1000.0);
    // imageStore(outFrame, pix, vec4((final_color), 1));
    // imageStore(outFrame, pix, vec4((stored_normal), 1));
    // final_color = stored_normal;
    // final_color = abs(final_color);
    // float ma = max(final_color.x, max(final_color.y, final_color.z));
    // float mi = min(final_color.x, min(final_color.y, final_color.z));
    // if(ma>1) 
    //     final_color /= ma;
    // else final_color /= mi;
    frame_color = vec4((final_color),1);
}