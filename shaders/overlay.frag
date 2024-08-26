#version 450

precision highp float;
precision highp int;

layout(location = 0) in mediump vec2 fragUV;
layout(location = 1) in mediump vec4 fragColor;

layout(set = 0, binding = 0) uniform sampler2D ui_elem_texture;

layout(location = 0) out vec4 outColor;

const float COLOR_ENCODE_VALUE = 8.0;
vec3 decode_color(vec3 encoded_color){
    return clamp(encoded_color,0,1)*vec3(COLOR_ENCODE_VALUE);
}
vec3 encode_color(vec3 color){
    return clamp(color/vec3(COLOR_ENCODE_VALUE), 0,1);
}

void main() {
    vec2 final_uv = vec2(fragUV.x, fragUV.y);
    vec4 sampledColor = texture(ui_elem_texture, final_uv); 

    //as stated in rmlui docs
    vec4 final_color  = sampledColor;
         final_color *= fragColor;
    
    outColor = final_color;
} 