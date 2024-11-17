#pragma once
#include "renderer/src/internal_render.hpp"
#ifndef __LUM_TRANSPARENT_HPP__
#define __LUM_TRANSPARENT_HPP__

/*
This is transparent version of opaque Lum::Renderer
such division allows small easy to parse headers
saves 80k lines
*/


#include "renderer.hpp"
#include <cstddef>
#include "containers/arena.hpp"

namespace Lum {

class OpaqueRendererMembers {
public:
    OpaqueRendererMembers() noexcept : OpaqueRendererMembers(1024, 1024, 64, 64, 64) {}
    OpaqueRendererMembers(
            size_t block_palette_size,
            size_t mesh_storage_size,
            size_t foliage_storage_size,
            size_t volumetric_storage_size,
            size_t liquid_storage_size) noexcept
            :
        block_palette(block_palette_size),
        mesh_models_storage(mesh_storage_size),
        mesh_foliage_storage(foliage_storage_size),
        mesh_volumetric_storage(volumetric_storage_size),
        mesh_liquid_storage(liquid_storage_size)
        // {init();} // i do not like it being that impicit
        {}
    ~OpaqueRendererMembers() noexcept 
        // {cleanup();} // i do not like it being that impicit
        {}
    public:

    LumInternal::LumInternalRenderer render;
    Lum::Settings settings; // duplicated?
    // Arenas are not only faster to allocate and more coherent memory, but also auto-free is possible
    Arena<LumInternal::InternalMeshModel> mesh_models_storage;
    Arena<LumInternal::InternalMeshFoliage> mesh_foliage_storage;
    Arena<LumInternal::InternalMeshVolumetric> mesh_volumetric_storage;
    Arena<LumInternal::InternalMeshLiquid> mesh_liquid_storage;

    std::vector<LumInternal::BlockWithMesh> block_palette;
    std::array<LumInternal::Material, LumInternal::MATERIAL_PALETTE_SIZE> mat_palette;
};

} // namespace Lum

#endif // __LUM_TRANSPARENT_HPP__
