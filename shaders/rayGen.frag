#version 450

// layout(location = 0) in float depth;
layout(location = 0)      in vec2 uv_shift;
layout(location = 1) flat in vec3 norm;
layout(location = 2) flat in uint mat;
// layout(location = 3)      in float depth; //uint8 thing. But in float

layout(location = 0) out vec4 outMatNorm;
layout(location = 1) out vec2 outOldUv;
// layout(location = 2) out float outDepth;
// layout(location = 2) out uvec4 outGbuffer_downscaled;
// layout(location = 1) out vec3 outNorm;
// layout(location = 2) out vec3 outPosDiff;

void main() {
    // uint uv_encoded = packUnorm2x16(old_uv);

    outOldUv = uv_shift;

    outMatNorm.x = (float(mat)-127.0)/127.0;
    outMatNorm.gba = norm.xyz;

    // outDepth = depth;
    // outGbuffer_downscaled = outGbuffer;
    // outPosMat.xyz = pos;
    // outPosMat.xyz = pos;
    // outPosMat.w = MatID;

    // outNorm = norm;
    // outPosDiff = pos_old;
}