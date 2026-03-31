#pragma once
#include "ext_frustum.h"
#include "SceneView.h"
#include "terrain_tile_tree_helpers.h"
#include "Camera/CameraComponent.h"
#include "Math/Vector.h"

inline FMatrix44d get_camera_projection_matrix(const UCameraComponent* camera_component) {
    const auto projection_mode = camera_component->ProjectionMode;
    if (projection_mode == ECameraProjectionMode::Perspective) {
        return FReversedZPerspectiveMatrix(
            camera_component->FieldOfView,
            camera_component->AspectRatio,
            camera_component->OrthoNearClipPlane,
            camera_component->OrthoFarClipPlane
        );
    }

    const auto ortho_width = camera_component->OrthoWidth;
    const auto ortho_height = ortho_width / camera_component->AspectRatio;
    return FReversedZOrthoMatrix(
        ortho_width,
        ortho_height,
        camera_component->OrthoNearClipPlane,
        camera_component->OrthoFarClipPlane
    );
}

inline void tile_tree_compute_requests(
    FTileTree* tile_tree,
    const FTransform& terrain_tf,
    const FVector3d& view_world_position,
    const FMatrix44d& view_from_world,
    const FMatrix44d& clip_from_view
) {
    const FMatrix44d clip_from_world = clip_from_view * view_from_world;

    const auto hs = ext::iter::map<
        UE::Geometry::FHalfspace3d,
        6,
        FVector4f(*)(const UE::Geometry::FHalfspace3d&)>(
        // The GPU prepass computes/culls terrain tiles in world space, so the frustum planes
        // must be derived from the world->clip matrix as well. Feeding clip_from_local here
        // over-culls once the terrain actor is translated, rotated, or scaled.
        Frustum::from_clip_from_world(clip_from_world).half_spaces,
        [](const UE::Geometry::FHalfspace3d& space) {
            return FVector4f{
                static_cast<float>(space.Normal.X),
                static_cast<float>(space.Normal.Y),
                static_cast<float>(space.Normal.Z),
                static_cast<float>(space.Constant)
            };
        });

    tile_tree->view_world_position = FVector3f(view_world_position);

    tile_tree->view_local_position =
        FVector3d(terrain_tf.InverseTransformPosition(FVector(view_world_position)));

    tile_tree->half_spaces = hs;
    tile_tree->update();
}

inline void tile_tree_compute_requests(
    FTileTree* tile_tree,
    const FTransform& terrain_tf,
    const FSceneView& view
) {
    tile_tree_compute_requests(
        tile_tree,
        terrain_tf,
        FVector3d(view.ViewMatrices.GetViewOrigin()),
        FMatrix44d(view.ViewMatrices.GetViewMatrix()),
        FMatrix44d(view.ViewMatrices.GetProjectionNoAAMatrix())
    );
}

inline void tile_tree_adjust_to_tile_atlas(
    const FTileTree* tile_tree,
    FTileAtlas* tile_atlas
) {
    for (auto& [tile, entry] : ext::iter::zip(tile_tree->tiles, tile_tree->data)) {
        entry = tile_atlas->get_best_tile(tile.coordinate);
    }
}

inline void tile_tree_update_terrain_view_buffer(
    FRDGBuilder& gb,
    FTileTree* tile_tree
) {
    const FRDGBufferDesc approximate_height_desc = FRDGBufferDesc::CreateStructuredDesc(
        sizeof(float),
        1);

    AllocatePooledBuffer(
        approximate_height_desc,
        tile_tree->approximate_height_buffer_pooled,
        TEXT("UDLOD.ApproximateHeightBuffer")
    );

    tile_tree->approximate_height_buffer = gb.RegisterExternalBuffer(
        tile_tree->approximate_height_buffer_pooled,
        TEXT("UDLOD.ApproximateHeightBuffer")
    );

    float* approximate_height = gb.AllocPOD<float>();
    *approximate_height = tile_tree->approximate_height;
    gb.QueueBufferUpload(tile_tree->approximate_height_buffer, approximate_height, sizeof(float));

    TerrainView* terrain_view = gb.AllocParameters<TerrainView>();
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

    const FRDGBufferDesc terrain_view_desc = FRDGBufferDesc::CreateStructuredDesc<TerrainView>(1);
    AllocatePooledBuffer(
        terrain_view_desc,
        tile_tree->terrain_view_buffer_pooled,
        TEXT("UDLOD.TerrainViewUploadBuffer")
    );

    tile_tree->terrain_view_buffer = gb.RegisterExternalBuffer(
        tile_tree->terrain_view_buffer_pooled,
        TEXT("UDLOD.TerrainViewUploadBuffer")
    );

    gb.QueueBufferUpload(tile_tree->terrain_view_buffer, terrain_view, sizeof(TerrainView));

    const auto tile_tree_entry_count = tile_tree->data.size();
    const FRDGBufferDesc tile_tree_desc =
        FRDGBufferDesc::CreateStructuredDesc<TileTreeEntry>(tile_tree_entry_count);

    AllocatePooledBuffer(
        tile_tree_desc,
        tile_tree->tile_tree_buffer_pooled,
        TEXT("UDLOD.TileTreeEntriesUploadBuffer")
    );

    tile_tree->tile_tree_buffer = gb.RegisterExternalBuffer(
        tile_tree->tile_tree_buffer_pooled,
        TEXT("UDLOD.TileTreeEntriesUploadBuffer")
    );

    const auto tile_tree_entries = gb.AllocPODArrayView<TileTreeEntry>(tile_tree_entry_count);
    const TArray<TileTreeEntry>& source_entries = tile_tree->data.get_storage();
    for (uint32 index = 0; index < tile_tree_entry_count; ++index) {
        tile_tree_entries[index] = source_entries[static_cast<int32>(index)];
    }

    gb.QueueBufferUpload<TileTreeEntry>(
        tile_tree->tile_tree_buffer,
        tile_tree_entries
    );
}

inline void tile_atlas_update(
    FTileTree* tile_tree,
    FTileAtlas* tile_atlas
) {
    auto released_tiles = ext::iter::drain(tile_tree->released_tiles);
    for (const auto& coordinate : released_tiles) { tile_atlas->release_tile(coordinate); }

    auto requested_tiles = ext::iter::drain(tile_tree->requested_tiles);
    for (const auto& coordinate : requested_tiles) { tile_atlas->request_tile(coordinate); }
}
