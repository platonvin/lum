#version 450

// layout(location = 0) in float depth;
layout(location = 0) in vec3 pos;
layout(location = 1) in flat vec3 norm;
layout(location = 2) in vec3 pos_old;
layout(location = 3) in flat float MatID; //uint8 thing. But in float

layout(location = 0) out vec4 outPosMat;
layout(location = 1) out vec3 outNorm;
layout(location = 2) out vec3 outPosDiff;

void main() {
    outPosMat.xyz = pos;
    // outPosMat.xyz = pos;
    outPosMat.w = MatID;

    outNorm = norm;
    outPosDiff = pos_old;
}