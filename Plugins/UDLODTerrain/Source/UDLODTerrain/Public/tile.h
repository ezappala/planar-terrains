#pragma once
#include "Math/Vector2D.h"

// struct alignas(4) FTile {
//     /// @brief terrain-local xy
//     FVector2f origin;
//     /// @brief size in world units
//     float size;
//     /// @brief quadtree depth (0 == root)
//     float depth;
// };
//
// static_assert(sizeof(FTile) == 16);
// static_assert(alignof(FTile) == 4);

BEGIN_SHADER_PARAMETER_STRUCT(FTile, UDLODTERRAIN_API)
    SHADER_PARAMETER(FVector2f, origin)
    SHADER_PARAMETER(float, size)
    SHADER_PARAMETER(float, depth)
END_SHADER_PARAMETER_STRUCT()

BEGIN_SHADER_PARAMETER_STRUCT(FCount, UDLODTERRAIN_API)
    SHADER_PARAMETER(uint32, value)
END_SHADER_PARAMETER_STRUCT()
