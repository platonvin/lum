#version 450
layout(early_fragment_tests) in;

#define varp highp
precision varp int;
precision varp float;

// layout(location = 0) lowp flat in uvec4 mat_norm;
layout(location = 0) in vec3 orig;

layout(location = 0) lowp out uvec4 outMatNorm;

void main() {
    uvec3 normal_encoded;

    //normal to surface
    vec3 normal = normalize(cross(
        dFdxFine(orig),
        dFdyFine(orig)
    ));
    normal = vec3(0,0,1);
    normal_encoded = uvec3(((normal+1.0)/2.0)*255.0);
    
    outMatNorm = uvec4(30, normal_encoded);
    // outMatNorm = mat_norm;
}