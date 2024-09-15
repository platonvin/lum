//exists to make compiler happy
//TODO: move code here so parsing headers isnt painfully slow
// #include "renderer/render.hpp"
#include <cstdio>
#include <unordered_map>
#define OGT_VOXEL_MESHIFY_IMPLEMENTATION
#include "ogt_voxel_meshify.hpp"

#include <glm/glm.hpp>
using namespace ogt;
using namespace glm;

#include "meshopt.hpp"

static void my_polygon_meshify_voxels_in_face_direction(
    const uint8_t* voxels,
    int32_t size_x, int32_t size_y, int32_t size_z,                // how many voxels in each of X,Y,Z dimensions
    int32_t k_stride_x, int32_t k_stride_y, int32_t k_stride_z,    // the memory stride for each of those X,Y,Z dimensions within the voxel data.
    const glm::imat4x4& transform,                           // transform to convert from X,Y,Z to "objectSpace"
    ogt_int_mesh* mesh)
{
    // enable aggressive voxel optimization for now.
    uint32_t max_voxels_per_slice = size_x * size_y;
    assert(max_voxels_per_slice <= 65536);	// voxel_polygonized and slice_colors requires this.
    ogt_mesh_bitset_64k voxel_polygonized;
    uint8_t slice_colors[65536];

//column major
// struct ogt_mesh_transform  {
//     float m00, m01, m02, m03;   // column 0 of 4x4 matrix, 1st three elements = x axis vector, last element always 0.0
//     float m10, m11, m12, m13;   // column 1 of 4x4 matrix, 1st three elements = y axis vector, last element always 0.0
//     float m20, m21, m22, m23;   // column 2 of 4x4 matrix, 1st three elements = z axis vector, last element always 0.0
//     float m30, m31, m32, m33;   // column 3 of 4x4 matrix. 1st three elements = translation vector, last element always 1.0
// };

    // determine if the transform parity has flipped in a way that winding would have been switched.
    const ivec3 side = ivec3(transform[0]);
    const ivec3 up   = ivec3(transform[1]);
    const ivec3 fwd  = ivec3(transform[2]);
    bool is_parity_flipped = dot(vec3(fwd), cross(vec3(side), vec3(up))) < 0.0f;

    glm::ivec3 normal = transform * ivec4(ivec3(0, 0, 1), 0);

    for ( int32_t k = 0; k < (int32_t)size_z; k++ ) {
        bool is_last_slice = (k == (size_z-1)) ? true : false;

        // clear this slice
        voxel_polygonized.clear(max_voxels_per_slice);

        // first, fill this slice with all colors for the voxel grid but set to zero where the
        // slice has a non-empty voxel in the corresponding location in the k+1 slice.
        uint32_t num_non_empty_cells = 0;
        for (int32_t j = 0; j < size_y; j++) {
            for (int32_t i = 0; i < size_x; i++) {
                int32_t index_in_slice = i+(j*size_x);
                uint8_t cell_color = voxels[i*k_stride_x + j*k_stride_y + (k+0)*k_stride_z];

                // if the this cell on this slice is occluded by the corresponding cell on the next slice, we 
                // mark this polygon as voxelized already so it doesn't get included in any polygons for the current slice.
                // we also inherit the next slice's color to ensure the polygon flood fill inserts 
                // discontinuities where necessary in order to generate a water-tight tessellation
                // to the next slice.
                uint8_t next_cell_color = !is_last_slice ? voxels[i * k_stride_x + j * k_stride_y + (k + 1) * k_stride_z] : 0;
                if (next_cell_color != 0) {
                    cell_color = next_cell_color;
                    voxel_polygonized.set(index_in_slice);
                }
                slice_colors[index_in_slice] = cell_color;

                num_non_empty_cells += (cell_color != 0) ? 1 : 0;
            }
        }
        // skip to the next slice if the entire slice is empty.
        if (!num_non_empty_cells)
            continue;
    
        // polygonize all voxels for this slice.
        for (int32_t j = 0; j < (int32_t)size_y; j++) {
            for (int32_t i = 0; i < (int32_t)size_x; i++) {
                int32_t index_in_slice = i + j * size_x;
                // this voxel does not need to be polygonized on this slice? 
                // early out: empty-cell, we don't consider it.
                uint8_t color_index = slice_colors[index_in_slice];
                if ( color_index == 0 )
                    continue;

                // this voxel is already polygonized, skip it.
                if (voxel_polygonized.is_set(index_in_slice))
                    continue;

                // we always start polygon rasterization with any lower-left corner in (i,j) 
                // space and fill outward from there. So skip any coords that don't match this
                // criteria.
                //if ((i > 0 && slice_colors[index_in_slice-1] == color_index) || 
                //	(j > 0 && slice_colors[index_in_slice-size_x] == color_index))
                //	continue;

                const uint32_t MAX_VERTS = 4096;
                ogt_mesh_vec2i verts[MAX_VERTS];
                uint32_t vert_count = _construct_polygon_for_slice(verts, MAX_VERTS, i, j, size_x, size_y, slice_colors, voxel_polygonized);
                // const ogt_mesh_rgba& color = (ogt_mesh_rgba){};
                // generate the verts in the output mesh
                uint32_t base_vertex_index = mesh->vertex_count;
                for (uint32_t vert_index = 0; vert_index < vert_count; vert_index++) {
                    mesh->vertices[mesh->vertex_count++] = (ogt_int_mesh_vertex){
                        .pos           = ivec3(transform * ivec4(verts[vert_index].x, verts[vert_index].y, (k+1), 1)), 
                        .normal        = ivec3(normal), 
                        .palette_index = (uint32_t) color_index
                    };
                }

                // generate the indices in the output mesh.
                uint32_t tessellated_index_count = _tessellate_polygon(&mesh->indices[mesh->index_count], verts, vert_count);

                // flip the winding of tessellated triangles to account for an inversion in the transform.
                if (is_parity_flipped) {
                    for (uint32_t index = 0; index < tessellated_index_count; index += 3) {
                        uint32_t i0 = mesh->indices[mesh->index_count + index + 0];
                        uint32_t i1 = mesh->indices[mesh->index_count + index + 1];
                        uint32_t i2 = mesh->indices[mesh->index_count + index + 2];
                        mesh->indices[mesh->index_count + index + 0] = base_vertex_index + i2;
                        mesh->indices[mesh->index_count + index + 1] = base_vertex_index + i1;
                        mesh->indices[mesh->index_count + index + 2] = base_vertex_index + i0;
                    }
                }
                else {
                    for (uint32_t index = 0; index < tessellated_index_count; index += 3) {
                        uint32_t i0 = mesh->indices[mesh->index_count + index + 0];
                        uint32_t i1 = mesh->indices[mesh->index_count + index + 1];
                        uint32_t i2 = mesh->indices[mesh->index_count + index + 2];
                        mesh->indices[mesh->index_count + index + 0] = base_vertex_index + i0;
                        mesh->indices[mesh->index_count + index + 1] = base_vertex_index + i1;
                        mesh->indices[mesh->index_count + index + 2] = base_vertex_index + i2;
                    }
                }

                mesh->index_count += tessellated_index_count;
            }
        }
    }
}

//one day i will use it
// #include "../khash.hpp"
namespace ogt{
static uint32_t pack_voxel_vertex(ogt_int_mesh_vertex v){
    uint32_t packed_v = {};
    unsigned char* packer = (unsigned char*) &packed_v;
    packer[0] = (unsigned char)v.pos.x;
    packer[1] = (unsigned char)v.pos.y;
    packer[2] = (unsigned char)v.pos.z;

    packer[3] = (unsigned char)0;
    // packer[4] = v.normal.y;
    // packer[5] = v.normal.z;

    // packer[6] = (unsigned char)v.palette_index;
    // packer[7] = 0;

    // packed_v
    return packed_v;
} 
typedef vec<3, uint32_t, glm::defaultp> IndexTri ;

static IndexTri rotate_itrig(IndexTri tri){
    return {
        tri.y,
        tri.z,
        tri.x,
        };
}
static uint32_t count_replacements(IndexTri &tri, ogt_int_mesh*mesh, std::unordered_map<uint32_t, uint32_t> &vertex_map){
    uint32_t replacement_counter = 0;

    uint32_t idx2replace = tri.y;
    ogt_int_mesh_vertex vtx2replace = mesh->vertices[idx2replace];
    uint32_t packed_vtx = pack_voxel_vertex(vtx2replace);

    replacement_counter += vertex_map.count(packed_vtx);

    idx2replace = tri.z;
    vtx2replace = mesh->vertices[idx2replace];
    packed_vtx = pack_voxel_vertex(vtx2replace);
    replacement_counter +=vertex_map.count(packed_vtx);

    return replacement_counter;
}

void my_int_mesh_optimize(const ogt_voxel_meshify_context* ctx, ogt_int_mesh* mesh){
    /*
    now we want to optimize it
    every 3 vertices repeat themselves in terms of normals and mats
    we can avoid it by having one vertex for normal and mat via flat, and other two just for positions

    so, algorithm:
    for every index triangle
    for its second and third vertices
    replace them with existing ones from map. If no in map, put them in map
    so we try to replace vertices with ones that can be used while removing old ones
    also, might try rotating triangle
    */
    IndexTri* itrigs = (IndexTri*) mesh->indices;
    uint32_t itrig_count = mesh->index_count / 3;
    
    assert((mesh->index_count % 3) == 0);
    
    std::unordered_map<uint32_t, uint32_t> vertex_map = {};

    for(int i=0; i<itrig_count; i++){
        uint32_t idx2replace = {};
        ogt_int_mesh_vertex vtx2replace = {};
        uint32_t packed_vtx = {};

        //try rotating triangles
        IndexTri triag = itrigs[i];
        uint32_t replacements_0 = count_replacements(triag, mesh, vertex_map);
        triag = rotate_itrig(triag);
        uint32_t replacements_1 = count_replacements(triag, mesh, vertex_map);
        triag = rotate_itrig(triag);
        uint32_t replacements_2 = count_replacements(triag, mesh, vertex_map);

        if((replacements_0 >= replacements_1) and (replacements_0 >= replacements_2)){
            itrigs[i] = itrigs[i];
        } else if((replacements_1 >= replacements_0) and (replacements_1 >= replacements_2)){
            itrigs[i] = rotate_itrig(itrigs[i]);
        } else if((replacements_2 >= replacements_1) and (replacements_2 >= replacements_0)){
            itrigs[i] = rotate_itrig(rotate_itrig(itrigs[i]));
        }

        idx2replace = itrigs[i].y;
        vtx2replace = mesh->vertices[idx2replace];
        packed_vtx = pack_voxel_vertex(vtx2replace);
        if (vertex_map.count(packed_vtx) != 0){
            //then exists vertex that we can replace current one with
            uint32_t new_idx = vertex_map[packed_vtx];
            itrigs[i].y = new_idx;
        } else {
            //add to the map
            vertex_map[packed_vtx] = idx2replace;
        }
        idx2replace = itrigs[i].z;
        vtx2replace = mesh->vertices[idx2replace];
        packed_vtx = pack_voxel_vertex(vtx2replace);
        if (vertex_map.count(packed_vtx) != 0){
            uint32_t new_idx = vertex_map[packed_vtx];
            itrigs[i].z = new_idx; 
        } else {
            vertex_map[packed_vtx] = idx2replace;
        }
    }
    
// printf(":%d\n", __LINE__);
    //remapping indexes to remove unused vertices
    //first, we need to count how many times every vertex is accessed
    uint32_t* access_counter = new uint32_t[mesh->vertex_count](); //weird c++ syntax for calloc
    for (int i=0; i<mesh->index_count; i++){
        access_counter[mesh->indices[i]]++;
    } 
    // for (int i=0; i<mesh->vertex_count; i++){
    //     printf("%d\n", access_counter[i]);
    // } 
       
// printf(":%d\n", __LINE__);

    //now we can create map for remapping while also removing unused vertices
    uint32_t* remapping = new uint32_t[mesh->vertex_count](); //weird c++ syntax for calloc
    uint32_t remapping_index = 0;
    for (int i=0; i<mesh->vertex_count; i++){
        if(access_counter[i] == 0) {
            // continue;
            // printl(remapping_index);
            // printl(i);
        } else {
            remapping[i] = remapping_index;
            mesh->vertices[remapping_index] = mesh->vertices[i];
            remapping_index++;
        }
    }
    // mesh->index_count = remapping_index;
// printf(":%d\n", __LINE__);
    //now replace old indices with new remapped ones
    for (int i=0; i<mesh->index_count; i++){
        uint32_t old_idx = mesh->indices[i];
        assert(old_idx < mesh->vertex_count);
        
        assert(access_counter[old_idx] != 0);
        uint32_t new_idx = remapping[old_idx];
        assert(new_idx < remapping_index);
        mesh->indices[i] = new_idx;
    }   
    // printl(mesh->vertex_count);
    // printl(remapping_index);
    //its free to do so because of free() trick used in allocating this
    mesh->vertex_count = remapping_index;
    // for (int i=0; i<mesh->vertex_count; i++){
    //     printf("x:%1d y:%1d z:%1d\n", mesh->vertices[i].normal.x, mesh->vertices[i].normal.y, mesh->vertices[i].normal.z);
    // }
    delete[] access_counter;
    delete[] remapping;

    //sorting for vertex cache coherence
    meshopt_optimizeVertexCache(mesh->indices, mesh->indices, mesh->index_count, mesh->vertex_count);
    auto r = meshopt_optimizeVertexFetch(mesh->vertices, mesh->indices, mesh->index_count, mesh->vertices, mesh->vertex_count, sizeof(ogt_int_mesh_vertex));
    // printl(r)
    // printl("")
}
    
ogt_int_mesh* my_int_mesh_from_paletted_voxels(const ogt_voxel_meshify_context* ctx, const uint8_t* voxels, uint32_t size_x, uint32_t size_y, uint32_t size_z){
    uint32_t max_face_count   = _count_voxel_sized_faces( voxels, size_x, size_y, size_z );
    uint32_t max_vertex_count = max_face_count * 4;
    uint32_t max_index_count  = max_face_count * 6;

    uint32_t mesh_size = sizeof(ogt_int_mesh) + (max_vertex_count * sizeof(ogt_int_mesh_vertex)) + (max_index_count * sizeof(uint32_t));
    ogt_int_mesh* mesh = (ogt_int_mesh*)_voxel_meshify_malloc(ctx, mesh_size);
    if (!mesh)
        return NULL;

    mesh->vertices = (ogt_int_mesh_vertex*)&mesh[1];
    mesh->indices  = (uint32_t*)&mesh->vertices[max_vertex_count];
    mesh->vertex_count = 0;
    mesh->index_count  = 0;
    
    const int32_t k_stride_x = 1;
    const int32_t k_stride_y = size_x;
    const int32_t k_stride_z = size_x * size_y;
    
    // do the +y PASS
    {
        imat4x4 transform_pos_y = imat4x4(
            0, 0, 1, 0,
            1, 0, 0, 0,
            0, 1, 0, 0,
            0, 0, 0, 0);

        my_polygon_meshify_voxels_in_face_direction(
            voxels,
            size_z, size_x, size_y,
            k_stride_z, k_stride_x, k_stride_y,
            transform_pos_y,
            mesh);
    }
    
    // do the -y PASS
    {
        imat4x4 transform_neg_y = imat4x4(
            0,      0, 1, 0,
            1,      0, 0, 0,
            0,     -1, 0, 0,
            0, size_y, 0, 0);

        my_polygon_meshify_voxels_in_face_direction(
            voxels + (size_y - 1) * k_stride_y, 
            size_z, size_x, size_y,
            k_stride_z, k_stride_x,-k_stride_y,
            transform_neg_y,
            mesh);
    }
    // do the +X pass
    {
        imat4x4 transform_pos_x = imat4x4(
            0, 1, 0, 0,
            0, 0, 1, 0,
            1, 0, 0, 0,
            0, 0, 0, 0);

        my_polygon_meshify_voxels_in_face_direction(
            voxels, 
            size_y, size_z, size_x,
            k_stride_y, k_stride_z, k_stride_x,
            transform_pos_x,
            mesh);
    }
    // do the -X pass
    {
        imat4x4 transform_neg_x = imat4x4(
             0, 1, 0, 0,
             0, 0, 1, 0,
            -1, 0, 0, 0,
        size_x, 0, 0, 0);

        my_polygon_meshify_voxels_in_face_direction(
            voxels + (size_x - 1) * k_stride_x,
            size_y, size_z, size_x,
            k_stride_y, k_stride_z, -k_stride_x,
            transform_neg_x,
            mesh);
    }
    // do the +Z pass
    {
        imat4x4 transform_pos_z = imat4x4(
            1, 0, 0, 0,
            0, 1, 0, 0,
            0, 0, 1, 0,
            0, 0, 0, 0);

        my_polygon_meshify_voxels_in_face_direction(
            voxels, 
            size_x, size_y, size_z,
            k_stride_x, k_stride_y, k_stride_z,
            transform_pos_z,
            mesh);
    }
    // do the -Z pass
    {
        imat4x4 transform_neg_z = imat4x4(
            1, 0,     0, 0,
            0, 1,     0, 0,
            0, 0,    -1, 0,
            0, 0,size_z, 0);

        my_polygon_meshify_voxels_in_face_direction(
            voxels + (size_z-1) * k_stride_z, 
            size_x, size_y, size_z,
            k_stride_x, k_stride_y, -k_stride_z,
            transform_neg_z,
            mesh);
    }

    assert( mesh->vertex_count <= max_vertex_count);
    assert( mesh->index_count <= max_index_count);

    return mesh;
}
}