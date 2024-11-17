#pragma once

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtc/type_precision.hpp>
#include <glm/ext/vector_float4.hpp>
#include <glm/ext/vector_float3.hpp>
#include <glm/ext/vector_int3.hpp>
#include <glm/gtx/quaternion.hpp>

namespace LumInternal {

//Everything is X -> Y -> Z order in arrays (vectors)
const int BLOCK_SIZE = 16; // each block in common world is BLOCK_SIZE x BLOCK_SIZE x BLOCK_SIZE
const int MATERIAL_PALETTE_SIZE = 256; //0 always empty
const int RCACHE_RAYS_PER_PROBE = 32;
const int BLOCK_PALETTE_SIZE_X = 64;
const int BLOCK_PALETTE_SIZE_Y = 64;
const int BLOCK_PALETTE_SIZE = (BLOCK_PALETTE_SIZE_X* BLOCK_PALETTE_SIZE_Y);

const int MAX_FRAMES_IN_FLIGHT = 2;

typedef glm::u8  MatID_t;
typedef glm::i16 BlockID_t;
typedef glm::u8  Voxel;
const BlockID_t SPECIAL_BLOCK = 32767;
typedef struct Material {
    glm::vec4 color; //color AND transparancy
    glm::f32 emmit; //emmits same color as reflect
    glm::f32 rough;
} Material;


typedef struct Particle {
    glm::vec3 pos;
    glm::vec3 vel;
    glm::f32 lifeTime;
    MatID_t matID;
} Particle;

// separate from Mesh so you can "instance" mesh
typedef struct MeshTransform {
    glm::quat rot = glm::quat_identity<float, glm::defaultp>();   // rotation quaternion
    glm::vec3 shift = glm::vec3(0); // float because not necessarily snapped to grid
} MeshTransform;

typedef Voxel BlockVoxels[BLOCK_SIZE][BLOCK_SIZE][BLOCK_SIZE];

typedef struct LumSpecificSettings {
    glm::ivec3 world_size = glm::ivec3(48, 48, 16);
    int static_block_palette_size = 15;
    int maxParticleCount = 8128;
} LumSpecificSettings;

typedef struct Camera {
    glm::dvec3 cameraPos = glm::vec3(60, 0, 194);
    glm::dvec3 cameraDir = glm::normalize(glm::vec3(0.61, 1.0, -0.8));
    glm::dmat4 cameraTransform = glm::identity<glm::dmat4>();
    double pixelsInVoxel = 5.0;
    glm::dvec2 originViewSize = glm::dvec2(1920, 1080);
    glm::dvec2 viewSize = originViewSize / pixelsInVoxel;
    glm::dvec3 cameraRayDirPlane = glm::normalize (glm::dvec3 (cameraDir.x, cameraDir.y, 0));
    glm::dvec3 horizline = glm::normalize (cross (cameraRayDirPlane, glm::dvec3 (0, 0, 1)));
    glm::dvec3 vertiline = glm::normalize (cross (cameraDir, horizline));

    void updateCamera();
} Camera;

}