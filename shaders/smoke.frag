#version 450

layout(location = 0) out vec4 smoke_color;

layout(set = 0, binding = 0, r16i       ) uniform iimage3D blocks;
layout(set = 0, binding = 1, rgba16     ) uniform image3D radianceCache;

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

//derived from denoiser
//responsible for both noise-blur AND bloom because they are similar 
void main() {
    // outColor = vec4(fragColor, 1.0);
    // vec2 uv = fragUV - vec2(0.5);
    // vec2 vxx = (fragUV / 2 + vec2(0.5)).xx; 
    // vec2 uv = fragUV / 2 + vec2(0.5);
    // uv.x = (vxx.yy).y;
    // vec2 size = textureSize(ui_elem_texture, 0);
    
    // outColor = final_color;
    smoke_color = vec4(vec3(0.3), 0.5);
    // vec4 old_color = subpassLoad(inFrame);
    // int mat = load_mat();
    // frame_color = sin(old_color * 42.2);
    // frame_color = old_color;
    // frame_color = vec4(vec3(mat)/256.0,1);
    // frame_color = vec4(non_clip_pos, 0.0, 0.0);
} 