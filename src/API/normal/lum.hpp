#pragma once
#ifndef __LUM_HPP__
#define __LUM_HPP__

#include "renderer/render.hpp"
#include "containers/arena.hpp"

// Lum is a lightweight wrapper around LumRenderer, with more stable and nicer API
// when properly compiled, it does not add any significant overhead

// even tho world is limited, it is not supposed to be the whole game world
// instead, treat it like "sliding window view" into your game world

// opaque ref-handlers that are supposed to be reused when possible (aka instancing):
typedef InternalMeshModel*      MeshModel; // non-world-grid-aligned voxel models
typedef BlockID_t               MeshBlock; // world-grid-aligned blocks of 16^3 voxels
typedef InternalMeshFoliage     MeshFoliage; // small non-voxel meshes, typically rendered in big groups. Mostly defined by vertex shader
typedef InternalMeshLiquid      MeshLiquid; // cascaded DFT for liquids like water
typedef InternalMeshVolumetric  MeshVolumetric; // Lambert law smoke
typedef InternalUiMesh*         MeshUi; // RmlUi interface thing

// queued requests to render different objects. They will be sorted and drawn
struct model_render_request{
    MeshModel mesh;
    MeshTransform trans;
    float cam_dist;
};
struct block_render_request{
    MeshBlock block;
    ivec3 pos;
    float cam_dist;
};
struct foliage_render_request{
    MeshFoliage foliage;
    ivec3 pos;
    float cam_dist;
};
struct liquid_render_request{
    MeshLiquid liquid;
    ivec3 pos;
    float cam_dist;
};
struct volumetric_render_request{
    MeshVolumetric volumetric;
    ivec3 pos;
    float cam_dist;
};

// yep its 13 kbytes
struct Lum {
public:
    // core setup methods - initialize Vulkan (or OpenGL in future) renderer
    Lum() : mesh_models_storage(4096), mesh_foliage_storage(64) {}

    void init();

    // world is "sliding window view" into your game world
    // so this functions are optional. You can just set each block manually through the same system you will use to project game world into Lum
    void loadWorld(BlockID_t* world_data);
    void loadWorld(const char* file); // 

    // functions to manage world representation
    void      setWorldBlock(ivec3 pos, MeshBlock block);
    void      setWorldBlock(int x, int y, int z, MeshBlock block);
    MeshBlock getWorldBlock(ivec3 pos);
    MeshBlock getWorldBlock(int x, int y, int z);
    
    void loadBlock(MeshBlock block, BlockWithMesh* block_data);
    void loadBlock(MeshBlock block, const char* file);
    void uploadBlockPaletteToGPU();
    void uploadMaterialPaletteToGPU();

    // MagicaVoxel file to Mesh struct
    MeshModel loadMesh(const char* file, bool try_extract_palette = true);
    MeshModel loadMesh(Voxel* mesh_data, int x_size, int y_size, int z_size);
    // makes future uses invalid, but does not break previously submitted frames
    void      freeMesh(MeshModel model); 
    
    // vertex_shader_file is a shader file that will create vertices and output mat_norm. Template located at shaders/templates/foliage.vert
    // Why shader and not vertices like []{vec3 pos, int mat}?
    // 1. There is plenty of parameters you might want to control - weight (for wind), color/pos/size/shift variation
    // Also with shader you can program any animations you want. Look into https://gpuopen.com/learn/mesh_shaders/mesh_shaders-procedural_grass_rendering/ for some info
    // 2. Perfomance. It somewhat prevents you from expensive foliag models. It would be stupid to have single grass blade to have more vertices than all the other models
    MeshFoliage loadFoliage(const char* vertex_shader_file, int vertices_per_foliage, int dencity);
    // these are just few floforats (no data loaded to GPU)
    // still opaque handler  structural coherence and future extensions
    MeshVolumetric loadVolumetric(float max_dencity, float dencity_deviation, u8vec3 color);
    MeshLiquid loadLiquid(MatID_t main_mat, MatID_t foam_mat);

    void startFrame();

    // draws every block in the world. 
    // This could be done inside renderFrame, but separate for more explicity 
    void drawWorld(); // blocks in the world
    // This could be done inside renderFrame, but separate for more explicity 
    void drawParticles();
    
    // voxel mesh(model). Non-world-grid-aligned, up to 255^3 size
    void drawMesh(InternalMeshModel* mesh, MeshTransform* trans);

    // non-voxel things Lum also supports. They all can be drawn inside voxel block or each other (why tho?)
    // e.g. if you have block that is a voxel chair, you are allowed to draw grass in the same block, and it will render perfectly fine
    void drawFoliageBlock(MeshFoliage, ivec3 pos);
    void drawLiquidBlock(MeshLiquid, ivec3 pos);
    void drawVolumetricBlock(MeshVolumetric, ivec3 pos);

    void prepareFrame(); // update system state and culling. Use after all drawThing()'s
    void submitFrame(); // renders an actual frame to screen

    // cleanup resources (please use me)
    void cleanup();
    void waitIdle();
    GLFWwindow* getGLFWptr();

public:
    bool should_close;
    LumRenderer render = {};
    LumSettings settings; // Vulkan + some rendering settings like vsync / fullscreen
    // this gets loaded to GPU but copy (or origin?) still stored on CPU cause maybe you need it (for physics & debug i guess)
    Material mat_palette[MATERIAL_PALETTE_SIZE];
    vector<BlockWithMesh> block_palette;
    Arena<InternalMeshModel> mesh_models_storage;
    Arena<InternalMeshFoliage> mesh_foliage_storage;
    // vector<InternalMeshLiquid>
    // vector<InternalMeshVolumetric>
    // they could be processed directly into vkCmdBuffer, but i am not ready to do that yet
    // benefits are that they can be (and they are) sorted 
    vector<block_render_request> block_que = {};
    vector<model_render_request> mesh_que = {};
    vector<foliage_render_request> foliage_que = {};
    vector<liquid_render_request> liquid_que = {};
    vector<volumetric_render_request> volumetric_que = {};

    double curr_time = 0;
    double prev_time = 0;
    double delt_time = 0;
};

#endif // __LUM_HPP__