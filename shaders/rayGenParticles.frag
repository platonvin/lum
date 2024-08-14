#version 450
layout(early_fragment_tests) in;

#define varp highp

precision varp int;
precision varp float;

// layout(location = 0) lowp flat in vec3 norm;
// layout(location = 1) lowp flat in float fmat;
layout(location = 0) lowp flat in uvec4 mat_norm;

// layout(location = 1) flat in lowp uint mat;

layout(location = 0) lowp out uvec4 outMatNorm;

void main() {
    // uint uv_encoded = packUnorm2x16(old_uv);

    // outOldUv = uv_shift;

    // outMatNorm.x = (float(mat)-127.0)/127.0;
    outMatNorm = uvec4(mat_norm);

    // outDepth = depth;
    // outGbuffer_downscaled = outGbuffer;
    // outPosMat.xyz = pos;
    // outPosMat.xyz = pos;
    // outPosMat.w = MatID;

    // outNorm = norm;
    // outPosDiff = pos_old;
}