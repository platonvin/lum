#version 450 

layout(input_attachment_index = 0, set = 0, binding = 0) uniform usubpassInput matNorm;
layout(set = 0, binding = 1 ) uniform sampler2D voxelPalette;

//stencils smooth surface pixels bit at 01 from 00 to 01

struct Material{
    vec3 color;
    float emmitance;
    vec3 diffuse_light;
    float roughness;
};
int load_mat(){
    int mat = int(round(subpassLoad(matNorm).x));
    return mat;
}
Material GetMat(in int voxel){
    Material mat;

    mat.color.r      = texelFetch(voxelPalette, ivec2(0,voxel), 0).r;
    mat.color.g      = texelFetch(voxelPalette, ivec2(1,voxel), 0).r;
    mat.color.b      = texelFetch(voxelPalette, ivec2(2,voxel), 0).r;
    // mat.transparancy = 1.0 - texelFetch(voxelPalette, ivec2(3,voxel), 0).r;
    mat.emmitance    =       texelFetch(voxelPalette, ivec2(4,voxel), 0).r;
    mat.roughness    =       texelFetch(voxelPalette, ivec2(5,voxel), 0).r;
    return mat;
}
void main() 
{
    float rough = GetMat(load_mat()).roughness;

    if(rough > 0.5){
        discard;
    }
}