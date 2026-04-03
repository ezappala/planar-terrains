#pragma once

#include "Math/Vector.h"
#include "Math/Vector2D.h"
#include "preprocess_tile_coordinate.h"

namespace terrain::runtime::planar {
inline FVector3f scale(const double side_length) {
    return {
        static_cast<float>(side_length),
        static_cast<float>(side_length),
        1.0f
    };
}

inline FVector3d local_position_from_uv(const FVector2d& uv, const double height, const double side_length) {
    return {
        (uv.X - 0.5) * side_length,
        (uv.Y - 0.5) * side_length,
        height
    };
}

inline FVector3d local_position_from_coordinate(
    const FCoordinate& coordinate,
    const double height,
    const double side_length
) {
    return local_position_from_uv(coordinate.uv, height, side_length);
}

inline FCoordinate coordinate_from_local_position(
    const FVector3d& local_position,
    const double side_length
) {
    const double safe_side_length = FMath::Max(side_length, UE_DOUBLE_SMALL_NUMBER);
    const FVector2d uv{
        FMath::Clamp(local_position.X / safe_side_length + 0.5, 0.0, 1.0),
        FMath::Clamp(local_position.Y / safe_side_length + 0.5, 0.0, 1.0)
    };
    return FCoordinate{0u, uv};
}
}
