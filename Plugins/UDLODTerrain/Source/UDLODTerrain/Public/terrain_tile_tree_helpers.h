#pragma once

#include "terrain_shaders.h"
#include "terrain_tile_tree.h"

inline FRDGBufferRef terrain_view_from_tile_tree(FRDGBuilder& gb, const FTileTree* tile_tree) {
    auto* terrain_view = gb.AllocParameters<TerrainView>();
    terrain_view->tree_size = tile_tree->tree_size;
    terrain_view->geometry_tile_count = tile_tree->geometry_tile_count;
    terrain_view->grid_size = static_cast<float>(tile_tree->grid_size);
    terrain_view->vertices_per_row = 2 * (tile_tree->grid_size + 2);
    terrain_view->vertices_per_tile = 2 * tile_tree->grid_size * (tile_tree->grid_size + 2);
    terrain_view->morph_distance = static_cast<float>(tile_tree->morph_distance);
    terrain_view->blend_distance = static_cast<float>(tile_tree->blend_distance);
    terrain_view->load_distance = static_cast<float>(tile_tree->load_distance);
    terrain_view->subdivision_distance = static_cast<float>(tile_tree->subdivision_distance);
    terrain_view->precision_distance = static_cast<float>(tile_tree->precision_distance);
    terrain_view->morph_range = tile_tree->morph_range;
    terrain_view->blend_range = tile_tree->blend_range;
    terrain_view->face = tile_tree->view_face;
    terrain_view->lod = tile_tree->view_lod;
    terrain_view->coordinates = ext::iter::map(
        tile_tree->view_coordinates,
        [&tile_tree](const FCoordinate& view_coordinate) {
            return view_coordinate_from_coordinate(view_coordinate, tile_tree->view_lod);
        });
    terrain_view->height_scale = 1.0f;
    terrain_view->world_position = tile_tree->view_world_position;
    terrain_view->half_spaces = tile_tree->half_spaces;
    const auto data = CreateStructuredBuffer(
        gb,
        TEXT("UDLOD.TerrainViewUploadBuffer"),
        sizeof(TerrainView),
        1,
        terrain_view,
        sizeof(TerrainView),
        ERDGInitialDataFlags::None);

    return data;
}

inline FRDGBufferRef tile_tree_from_cpu_tree(FRDGBuilder& gb, const FTileTree* tile_tree) {
    // const auto tile_tree_params = gb.AllocParameters<TileTree>();
    const auto data = CreateStructuredBuffer(
        gb,
        TEXT("UDLOD.TileTreeEntriesUploadBuffer"),
        sizeof(TileTreeEntry),
        tile_tree->data.size(),
        tile_tree->data.get_storage().GetData(),
        tile_tree->data.get_storage().GetAllocatedSize(),
        ERDGInitialDataFlags::None);
    return data;
}
