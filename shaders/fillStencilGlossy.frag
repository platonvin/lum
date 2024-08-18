#version 450 

layout(input_attachment_index = 0, set = 0, binding = 0) uniform usubpassInput matNorm;
layout(set = 0, binding = 1, r32f       ) restrict readonly uniform image2D voxelPalette;

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

    mat.color.r      = imageLoad(voxelPalette, ivec2(0,voxel)).r;
    mat.color.g      = imageLoad(voxelPalette, ivec2(1,voxel)).r;
    mat.color.b      = imageLoad(voxelPalette, ivec2(2,voxel)).r;
    // mat.transparancy = 1.0 - imageLoad(voxelPalette, ivec2(3,voxel)).r;
    mat.emmitance    =       imageLoad(voxelPalette, ivec2(4,voxel)).r;
    mat.roughness    =       imageLoad(voxelPalette, ivec2(5,voxel)).r;
    return mat;
}
void main() 
{
    float rough = GetMat(load_mat()).roughness;

    if(rough > 0.5){
        discard;
    }
}