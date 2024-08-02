#version 450

//version with interpolation

precision highp int;
precision highp float;

layout(location = 0)      in vec3 norm;
layout(location = 1) flat in uint mat;

layout(location = 0) out vec4 outMatNorm;

void main() {
    outMatNorm.x = (float(mat)-127.0)/127.0;
    outMatNorm.gba = normalize(norm.xyz);
}