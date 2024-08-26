#version 450
layout(early_fragment_tests) in;

precision highp int;
precision highp float;

layout(location = 0) lowp flat in uvec4 mat_norm;

layout(location = 0) lowp out uvec4 outMatNorm;

void main() {
    // outMatNorm.x = fmat;
    // outMatNorm.gba = normalize(norm.xyz);
    
    outMatNorm = uvec4(mat_norm);
}