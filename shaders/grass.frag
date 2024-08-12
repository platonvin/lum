#version 450
layout(early_fragment_tests) in;

//version with interpolation

precision highp int;
precision highp float;

layout(location = 0) lowp flat in float fmat;
layout(location = 1) lowp      in vec3 norm;

layout(location = 0) out vec4 outMatNorm;

void main() {
    outMatNorm.x = fmat;
    outMatNorm.gba = normalize(norm.xyz);
}