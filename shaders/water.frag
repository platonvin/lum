#version 450
layout(early_fragment_tests) in;

#define varp highp

precision varp int;
precision varp float;

layout(location = 0) lowp flat in uvec4 mat_norm;


layout(location = 0) lowp out uvec4 outMatNorm;

void main() {
    outMatNorm = uvec4(mat_norm);
}