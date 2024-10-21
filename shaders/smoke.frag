#version 450

precision highp float;
precision highp int;

//dont swap
#include "common/ext.glsl"
#include "common/ubo.glsl"

// layout(location = 0) in vec3 zero_origin;
// layout(location = 0) in vec2 clip_pos;
layout(location = 0) out vec4 smoke_color;
layout(input_attachment_index = 0, set = 0, binding = 1) uniform subpassInput smoke_depth_far;
layout(input_attachment_index = 0, set = 0, binding = 2) uniform subpassInput smoke_depth_near;
layout(set = 0, binding = 3, rgba16) uniform restrict readonly image3D radianceCache;
layout(set = 0, binding = 4) uniform sampler3D noise;

const ivec3 world_size = ivec3(48,48,16);


// vec3 load_norm(){
//     // vec3 norm = (imageLoad(matNorm, pixel).gba);
//     vec3 norm = (subpassLoad(matNorm).gba);
//     // subpass
//     return norm;
// }
// int load_mat(){
//     int mat = int(round(subpassLoad(matNorm).x*127.0))+127;
//     // int mat = int(subpassLoad(matNorm).x);
//     return mat;
// }

vec3 sample_probe(ivec3 probe_ipos, vec3 direction){
    // int closest_id = clamp(int(inverseFibonacciPoint(direction).x), 0,RAYS_PER_PROBE-1);
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

vec3 sample_radiance(vec3 position){
    vec3 sampled_light;

    float total_weight =      0 ;
     vec3 total_colour = vec3(0);

    ivec3 zero_probe_ipos = clamp(ivec3(floor(position - 8.0))/16, ivec3(0), world_size);
     vec3 zero_probe_pos = vec3(zero_probe_ipos)*16.0 + 8.0;

    vec3 alpha = clamp((position - zero_probe_pos) / 16.0, 0,1);

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
        
        probe_colour = sample_probe(zero_probe_ipos + offset, direction_to_probe);

        total_weight += probe_weight;
        total_colour += probe_weight * probe_colour;
    }

    return total_colour / total_weight;
}

float decode_depth(float d){
    return (d)*1000.0;
}
float load_depth_far(){
    return decode_depth(subpassLoad(smoke_depth_far).x);
    // return (subpassLoad(smoke_depth_far).x);
    // return subpassLoad(smoke_depth_far).x;
}
float load_depth_near(){
    return decode_depth(subpassLoad(smoke_depth_near).x);
    // return (subpassLoad(smoke_depth_near).x);
    // return subpassLoad(smoke_depth_far).x;
}

vec3 cameraRayDirPlane;
vec3 horizline;
vec3 vertiline;

vec3 get_origin_from_depth(float depth, vec2 clip_pos){
    vec3 origin = ubo.campos.xyz +
        (ubo.horizline_scaled.xyz*clip_pos.x) + 
        (ubo.vertiline_scaled.xyz*clip_pos.y) +
        (ubo.camdir.xyz*depth);
    return origin;
}

vec2 rotate(vec2 v, float a) {
	float s = sin(a);
	float c = cos(a);
	mat2 m = mat2(c, s, -s, c);
	return m * v;
}

mat2 rotatem(float a) {
	float s = sin(a);
	float c = cos(a);
	mat2 m = mat2(c, s, -s, c);
	return m;
}

const float COLOR_ENCODE_VALUE = 8.0;
vec3 decode_color(vec3 encoded_color){
    return encoded_color*COLOR_ENCODE_VALUE;
}
vec3 encode_color(vec3 color){
    return color/COLOR_ENCODE_VALUE;
}

void main() {
    vec3 direction = (ubo.camdir.xyz);

    const float near = (load_depth_near());
    const float  far = (load_depth_far());

    const float diff = (far-near);

    const int max_steps = 8; //does not really matter
    const float step_size = diff/float(max_steps);

    //https://en.wikipedia.org/wiki/Beer%E2%80%93Lambert_law
    //I = I0 * exp(-K * L)
    //dI = -K*dL * I0 * exp(-K * L)
    //In+1 = (1-denisty_n*ΔL) * In

    float I0 = 1.0;
    float I = 1.0;

    vec3 position;
    const float treshhold = 0.7;
    const float multiplier = 1.7;

    float total_dencity = 0;

    // never do this
    // for(float fraction = near; fraction <= far; fraction+=step_size){ 
    
    float fraction = near;
    // [[loop]] 
    for(int i=0; i<max_steps; i++){
        fraction += step_size;
            vec2 clip_pos = gl_FragCoord.xy / ubo.frame_size * 2.0 - 1.0;
            position = get_origin_from_depth(fraction, clip_pos);
            vec3 voxel_pos = vec3(position);
            vec3 noise_clip_pos = voxel_pos / 32.0;
        vec4 noises;
                vec3 wind_direction = vec3(1,0,0);
                mat2 wind_rotate = rotatem(1.6);
            //TODO: derivatives? dFxy possibly solves mem access 
            noises.x = texture(noise, noise_clip_pos/1.0 + wind_direction*ubo.timeseed/3500.0).x;
                wind_direction.xy *= wind_rotate;
            noises.y = texture(noise, noise_clip_pos/2.1 + wind_direction*ubo.timeseed/3000.0).y;
                wind_direction.xy *= wind_rotate;
            noises.z = texture(noise, noise_clip_pos/3.2 + wind_direction*ubo.timeseed/2500.0).z;
                wind_direction.xy *= wind_rotate;
            noises.w = texture(noise, noise_clip_pos/4.3 + wind_direction*ubo.timeseed/2000.0).w;

        float close_to_border = clamp(diff,0.1,16.0)/16.0;

        float dencity = (noises.x + noises.y + noises.z - noises.w / close_to_border) / 2.0 - treshhold;

        dencity = clamp(dencity, 0,treshhold) * multiplier;
        // dencity *= exp(-clamp(diff, 0, 100)/100.0);
        // dencity += clamp(diff/10,0,1)/20.0;
        I = (1.0 - dencity * step_size) * I;
        total_dencity += dencity * step_size;
    }
    //at point of leaving smoke
    //does not look realistic but fits engine blocky style
    vec3 final_light = sample_radiance(position, direction);

    //1-I because its inverted
    float smoke_opacity = 1.0 - I;
    smoke_color = vec4(encode_color(vec3(final_light)), smoke_opacity);

} 