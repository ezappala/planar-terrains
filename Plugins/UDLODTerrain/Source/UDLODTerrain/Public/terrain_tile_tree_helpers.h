#pragma once

#include "terrain_shaders.h"
#include "terrain_tile_tree.h"

inline FRDGBufferRef terrain_view_from_tile_tree(
    FRDGBuilder& gb,
    const FTileTree* tile_tree,
    const uint32 debug_flags,
    const uint32 planar_gradient_mode
) {
    FTerrainViewUpload* terrain_view = gb.AllocPOD<FTerrainViewUpload>();
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
    terrain_view->debug_flags = debug_flags;
    terrain_view->planar_gradient_mode = planar_gradient_mode;

    for (uint32 lod_index = 0; lod_index < UE_UDLOD_MAX_SHADER_LOD_COUNT; ++lod_index) {
        const FVector2d lod_tile_count = tile_tree->get_tile_count(lod_index);
        const uint32 lod_count_x = static_cast<uint32>(
            FMath::Max<int32>(1, FMath::RoundToInt(lod_tile_count.X))
        );
        const uint32 lod_count_y = static_cast<uint32>(
            FMath::Max<int32>(1, FMath::RoundToInt(lod_tile_count.Y))
        );
        terrain_view->lod_tile_counts[lod_index] = FUintVector4(lod_count_x, lod_count_y, 0u, 0u);
    }

    for (uint32 coordinate_index = 0; coordinate_index < UE_ARRAY_COUNT(terrain_view->coordinates);
         ++coordinate_index) {
        const FVector2d tile_count = tile_tree->get_tile_count(tile_tree->view_lod);
        const int32 tile_count_x = FMath::Max(
            1,
            static_cast<int32>(FMath::RoundToInt(tile_count.X))
        );
        const int32 tile_count_y = FMath::Max(
            1,
            static_cast<int32>(FMath::RoundToInt(tile_count.Y))
        );
        const ViewCoordinate upload_coordinate = view_coordinate_from_coordinate(
            tile_tree->view_coordinates[coordinate_index],
            FIntPoint(tile_count_x, tile_count_y)
        );
        terrain_view->coordinates[coordinate_index].xy = upload_coordinate.xy;
        terrain_view->coordinates[coordinate_index].uv = upload_coordinate.uv;
    }
    terrain_view->height_scale = 1.0f;
    terrain_view->world_position = tile_tree->view_world_position;
    for (uint32 half_space_index = 0; half_space_index < UE_ARRAY_COUNT(terrain_view->half_spaces);
         ++half_space_index) {
        terrain_view->half_spaces[half_space_index] = tile_tree->half_spaces[half_space_index];
    }
    const auto data = CreateStructuredBuffer(
        gb,
        TEXT("UDLOD.TerrainViewUploadBuffer"),
        sizeof(FTerrainViewUpload),
        1,
        terrain_view,
        sizeof(FTerrainViewUpload),
        ERDGInitialDataFlags::None);

    return data;
}

inline FRDGBufferRef tile_tree_from_cpu_tree(FRDGBuilder& gb, const FTileTree* tile_tree) {
    const uint32 tile_tree_entry_count = tile_tree->data.size();
    FTileTreeEntryUpload* upload_entries = gb.AllocPODArray<FTileTreeEntryUpload>(
        tile_tree_entry_count
    );
    const TArray<TileTreeEntry>& source_entries = tile_tree->data.get_storage();
    for (uint32 index = 0; index < tile_tree_entry_count; ++index) {
        const TileTreeEntry& source_entry = source_entries[static_cast<int32>(index)];
        upload_entries[index].atlas_index = source_entry.atlas_index;
        upload_entries[index].atlas_lod = source_entry.atlas_lod;
    }

    const auto data = CreateStructuredBuffer(
        gb,
        TEXT("UDLOD.TileTreeEntriesUploadBuffer"),
        sizeof(FTileTreeEntryUpload),
        tile_tree_entry_count,
        upload_entries,
        sizeof(FTileTreeEntryUpload) * tile_tree_entry_count,
        ERDGInitialDataFlags::None);
    return data;
}
