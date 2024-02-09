#define VOX_IMPLEMENTATION 
#define OGT_VOXEL_MESHIFY_IMPLEMENTATION 
#include "visible_world.hpp"

extern Renderer render;

template<typename Type> tuple<Type*, u32> readFileBuffer(const char* path) {
    FILE* fp = fopen(path, "rb");
    u32 buffer_size = _filelength(_fileno(fp));
    Type* buffer = new Type[buffer_size];
    fread(buffer, buffer_size, 1, fp);
    fclose(fp);

    return {buffer, buffer_size};
}

void VisualWorld::init(){
    auto [buffer, buffer_size] = readFileBuffer<u8>("assets/scene.vox");
    const ogt::vox_scene* scene = ogt::vox_read_scene(buffer, buffer_size);
    delete[] buffer;

        printl(scene->num_cameras);
        printl(scene->num_groups);
        printl(scene->num_instances);
        printl(scene->num_layers);
        printl(scene->num_models);

    assert(scene->num_models < BLOCK_PALETTE_SIZE-1);
    for(i32 i=0; i<scene->num_models; i++){
    //    this->blocksPalette[i+1] = scene->models[i]->voxel_data;
        assert(scene->models[i]->size_x == BLOCK_SIZE);
        assert(scene->models[i]->size_y == BLOCK_SIZE);
        assert(scene->models[i]->size_z == BLOCK_SIZE);
        u32 sizeToCopy = BLOCK_SIZE*BLOCK_SIZE*BLOCK_SIZE;
        memcpy(&this->blocksPalette[i+1], scene->models[i]->voxel_data, sizeToCopy);
    }
    //i=0 alwaus empty mat/color
    for(i32 i=0; i<MATERIAL_PALETTE_SIZE; i++){
        this->matPalette[i].color = vec4(
            scene->palette.color[i].r,
            scene->palette.color[i].g,
            scene->palette.color[i].b,
            scene->palette.color[i].a
        );
        this->matPalette[i].emmit = scene->materials.matl[i].emit;
        this->matPalette[i].rough = scene->materials.matl[i].rough;
    }

    //Chunks is just 3d psewdo dynamic array. Same as united blocks. Might change on settings change
    this->loadedChunks.resize(1,1,1);
    this->unitedBlocks.resize(3,3,3);

    ChunkInMem singleChunk = {};
    ogt::ogt_voxel_meshify_context ctx = {};
    ogt::ogt_mesh_rgba rgbaPalette[256] = {};
    //comepletely unnesessary but who cares
    for(i32 i=0; i<256; i++){
        ivec4 icolor = ivec4(this->matPalette->color);
        rgbaPalette[i].r = icolor.r;
        rgbaPalette[i].g = icolor.g;
        rgbaPalette[i].b = icolor.b;
        rgbaPalette[i].a = icolor.a;
    }
    // rgbaPalette.a
    ogt::ogt_mesh* mesh = ogt::ogt_mesh_from_paletted_voxels_polygon(&ctx, (const u8*)this->blocksPalette[1].voxels, BLOCK_SIZE, BLOCK_SIZE, BLOCK_SIZE, rgbaPalette);

    vector<Vertex> vertices;
    vector<u32   >  indices;
    
printl(mesh->vertex_count);
printl(mesh->index_count);
    for(i32 i=0; i<mesh->vertex_count; i++){
        Vertex v = {};
            v.pos.x = mesh->vertices[i].pos.x;
            v.pos.y = mesh->vertices[i].pos.y;
            v.pos.z = mesh->vertices[i].pos.z;

            v.norm.x = mesh->vertices[i].normal.x;
            v.norm.y = mesh->vertices[i].normal.y;
            v.norm.z = mesh->vertices[i].normal.z;

        assert(mesh->vertices[i].palette_index < 256);
        assert(mesh->vertices[i].palette_index != 0);
        v.matID = mesh->vertices[i].palette_index;

        vertices.push_back(v);
    }
    for(i32 i=0; i<mesh->index_count; i++){
        u32 index = mesh->indices[i];
        indices.push_back(index);
    }
    //so we have a mesh in indexed vertex form
    //without transformations
    singleChunk.blocks[0][0][0] = 1;
    singleChunk.mesh.data = render.create_RayGen_VertexBuffers(vertices, indices);
    singleChunk.mesh.data.icount = indices.size();

    float rotationX = glm::radians(45.0f);
    float rotationY = glm::radians(30.0f);
    float rotationZ = glm::radians(20.0f);

    singleChunk.mesh.transform = mat4(1);
    //applying rotation
    singleChunk.mesh.transform = rotate(singleChunk.mesh.transform, rotationZ, vec3(0.0f, 0.0f, 1.0f));
    singleChunk.mesh.transform = rotate(singleChunk.mesh.transform, rotationY, vec3(0.0f, 1.0f, 0.0f));
    singleChunk.mesh.transform = rotate(singleChunk.mesh.transform, rotationX, vec3(1.0f, 0.0f, 0.0f));
    //applying position
    singleChunk.mesh.transform = translate(singleChunk.mesh.transform, vec3(0.0f, 0.0f, 0.0f));

    this->objects.push_back(singleChunk.mesh);
    
    this->loadedChunks(0,0,0) = singleChunk;

    this->unitedBlocks(0,0,0) = 1; //so 1 from block palette. For testing
    this->unitedBlocks(0,0,1) = 1; //so 1 from block palette. For testing
    this->unitedBlocks(0,0,2) = 1; //so 1 from block palette. For testing
    this->unitedBlocks(0,1,1) = 1; //so 1 from block palette. For testing
}

void VisualWorld::update(){
/*
nothing here... Yet
*/
}

void VisualWorld::cleanup(){
/*
nothing here... Yet
*/
}