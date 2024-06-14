#version 450

// layout(location = 0) in float depth;
layout(location = 0)      in vec2 old_uv;
layout(location = 1) flat in uint norm_encoded;
layout(location = 2) flat in uint mat; //uint8 thing. But in float
layout(location = 3)      in float depth; //uint8 thing. But in float

layout(location = 0) out uvec4 outGbuffer;
// layout(location = 2) out uvec4 outGbuffer_downscaled;
// layout(location = 1) out vec3 outNorm;
// layout(location = 2) out vec3 outPosDiff;

void main() {
    uint uv_encoded = packUnorm2x16(old_uv);
    outGbuffer.x = uv_encoded;
    outGbuffer.y = norm_encoded;
    outGbuffer.z = mat;
    outGbuffer.w = floatBitsToUint(depth);

    // outGbuffer_downscaled = outGbuffer;
    // outPosMat.xyz = pos;
    // outPosMat.xyz = pos;
    // outPosMat.w = MatID;

    // outNorm = norm;
    // outPosDiff = pos_old;
}