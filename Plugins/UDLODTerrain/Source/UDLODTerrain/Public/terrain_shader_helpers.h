#pragma once

#include "ext_affine.h"
#include "preprocess_tile_coordinate.h"
#include "RenderGraphUtils.h"
#include "terrain_runtime_planar.h"
#include "terrain_shaders.h"

inline Terrain new_terrain(
    const uint32 lod_count,
    const FVector3f& scale,
    const float min_height,
    const float max_height,
    const float height_scale,
    const FTransform& tf
) {
    FTransform scaled_transform = tf;
    scaled_transform.SetScale3D(tf.GetScale3D() * FVector(scale));

    const FMatrix3x4 world_from_local = to_world_from_local_packed(scaled_transform);
    const auto [rows, last] = to_local_from_world_normal_packed(scaled_transform);
    const auto local_from_world_transpose_a = rows;
    const auto local_from_world_transpose_b = last;

    Terrain terrain;
    terrain.lod_count = lod_count;
    terrain.scale = scale;
    terrain.min_height = min_height * height_scale;
    terrain.max_height = max_height * height_scale;
    terrain.height_scale = height_scale;
    terrain.world_from_unit = world_from_local;
    terrain.unit_from_world_transpose_a = local_from_world_transpose_a;
    terrain.unit_from_world_transpose_b = local_from_world_transpose_b;

    return terrain;
}

inline ViewCoordinate view_coordinate_from_coordinate(
    const FCoordinate& coordinate,
    const FIntPoint& tile_count
) {
    const double count_x = FMath::Max(static_cast<double>(tile_count.X), 1.0);
    const double count_y = FMath::Max(static_cast<double>(tile_count.Y), 1.0);
    const double scaled_x = FMath::Clamp(coordinate.uv.X * count_x, 0.0, count_x - 1e-6);
    const double scaled_y = FMath::Clamp(coordinate.uv.Y * count_y, 0.0, count_y - 1e-6);

    ViewCoordinate view_coordinate;
    view_coordinate.xy = FUint32Point{
        static_cast<uint32>(FMath::FloorToDouble(scaled_x)),
        static_cast<uint32>(FMath::FloorToDouble(scaled_y))
    };

    view_coordinate.uv = FVector2f{
        static_cast<float>(scaled_x - FMath::FloorToDouble(scaled_x)),
        static_cast<float>(scaled_y - FMath::FloorToDouble(scaled_y))
    };

    return view_coordinate;
}
