#version 450

layout(location = 0) in vec3 posOut;
layout(location = 1) in vec3 normOut;
layout(location = 2) in float MatIDOut;

layout(location = 0) out vec4 outPosMat;
layout(location = 1) out vec3 outNorm;

void main() {
    outPosMat.xyz = posOut;
    outPosMat.w = MatIDOut;

    outNorm = normOut;
}