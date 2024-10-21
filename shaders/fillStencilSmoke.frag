#version 450 

//just sets stencil values to 10 on rasterization
layout (location = 0) in float end_depth_in;

layout(location = 0) out float  far_depth_out;
layout(location = 1) out float near_depth_out;

//desired effect of separation achieved through min max blend
#include "common/ext.glsl"

void main() {
    if(!gl_FrontFacing){
        gl_FragDepth = end_depth_in - 0.01;
    } else {
        gl_FragDepth = end_depth_in;
    }

    far_depth_out = end_depth_in;
    near_depth_out = end_depth_in;
}