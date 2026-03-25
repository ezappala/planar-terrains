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
    const FSceneView& view
) {
    const FVector3d view_world_position(view.ViewMatrices.GetViewOrigin());

    const FMatrix44d view_from_world(view.ViewMatrices.GetViewMatrix());
    const FMatrix44d clip_from_view(view.ViewMatrices.GetProjectionNoAAMatrix());

    const FMatrix44d clip_from_world = clip_from_view * view_from_world;

    const FMatrix44d world_from_local(terrain_tf.ToMatrixWithScale());

    const FMatrix44d clip_from_local = clip_from_world * world_from_local;

    const auto hs = ext::iter::map<
        UE::Geometry::FHalfspace3d,
        6,
        FVector4f(*)(const UE::Geometry::FHalfspace3d&)>(
        Frustum::from_clip_from_world(clip_from_local).half_spaces,
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
    const float approximate_height = tile_tree->approximate_height;
    tile_tree->approximate_height_buffer = CreateStructuredBuffer(
        gb,
        TEXT("UDLOD.ApproximateHeightBuffer"),
        sizeof(float),
        1,
        &approximate_height,
        sizeof(float)
    );
    tile_tree->terrain_view_buffer = terrain_view_from_tile_tree(gb, tile_tree);
    tile_tree->tile_tree_buffer = tile_tree_from_cpu_tree(gb, tile_tree);
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
