#version 450

layout(location = 0) in vec2 non_clip_pos;
layout(location = 0) out vec4 frame_color;

layout(input_attachment_index = 0, set = 0, binding = 0) uniform subpassInput matNorm;
layout(input_attachment_index = 1, set = 0, binding = 1) uniform subpassInput inFrame;


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

    vec4 old_color = subpassLoad(inFrame);

    // frame_color = sin(old_color * 42.2);
    frame_color = old_color;
    // frame_color = vec4(non_clip_pos, 0.0, 0.0);
} 