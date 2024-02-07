#version 450

// layout(location = 0) in vec3 fragColor;
layout(location = 0) in vec2 fragUV;  

layout(set = 0, binding = 0) uniform sampler2D frame;

layout(location = 0) out vec4 outColor;

void main() {
    // outColor = vec4(fragColor, 1.0);
    // vec2 uv = fragUV - vec2(0.5);
    vec2 vxx = (fragUV / 2 + vec2(0.5)).xx; 
    vec2 uv = fragUV / 2 + vec2(0.5);
    uv.x = (vxx.yy).y;
    // vec4 sampledColor = texture(frame, uv); 
    outColor = texture(frame, uv);
    // outColor = vec4(uv, 0.0, 1.0);
} 