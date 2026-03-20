#pragma once

#include "ext_affine.h"
#include "RenderGraphUtils.h"
#include "terrain_shaders.h"
#include "preprocess_tile_coordinate.h"

inline Terrain new_terrain(
    const uint32 lod_count,
    const FVector3f& scale,
    const float min_height,
    const float max_height,
    const float height_scale,
    const FTransform& tf
) {
    const FMatrix3x4 world_from_local = to_world_from_local_packed(tf);
    const auto [rows, last] = to_local_from_world_normal_packed(tf);
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
    const uint32 lod
) {
    const auto count = FMath::Exp2(static_cast<double>(lod));
    ViewCoordinate view_coordinate;
    view_coordinate.xy = FUint32Point{
        static_cast<uint32>(coordinate.uv.X * count),
        static_cast<uint32>(coordinate.uv.Y * count)
    };

    view_coordinate.uv = FVector2f{
        static_cast<float>(FMath::Frac(coordinate.uv.X)),
        static_cast<float>(FMath::Frac(coordinate.uv.Y))
    };

    return view_coordinate;
}