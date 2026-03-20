#pragma once
#include "HalfspaceTypes.h"
#include "Containers/StaticArray.h"

inline UE::Geometry::FHalfspace3d half_space_from_vec4(const FVector4d& v) {
    return UE::Geometry::FHalfspace3d{v.X, v.Y, v.Z, v.W};
}

struct Frustum {
    TStaticArray<UE::Geometry::FHalfspace3d, 6> half_spaces;

    static Frustum from_clip_from_world_no_far(const FMatrix44d& clip_from_world) {
        const FVector4d row0{clip_from_world.M[0][0], clip_from_world.M[0][1], clip_from_world.M[0][2], clip_from_world.M[0][3]};
        const FVector4d row1{clip_from_world.M[1][0], clip_from_world .M[1][1], clip_from_world.M[1][2], clip_from_world.M[1][3]};
        const FVector4d row2{clip_from_world.M[2][0], clip_from_world.M[2][1], clip_from_world.M[2][2], clip_from_world.M[2][3]};
        const FVector4d row3{clip_from_world.M[3][0], clip_from_world.M[3][1], clip_from_world.M[3][2], clip_from_world.M[3][3]};

        return Frustum{
            {
                half_space_from_vec4(row3 + row0),
                half_space_from_vec4(row3 - row0),
                half_space_from_vec4(row3 + row1),
                half_space_from_vec4(row3 - row1),
                half_space_from_vec4(row3 + row2),
                UE::Geometry::FHalfspace3d{}
            }
        };
    }

    static Frustum from_clip_from_world(const FMatrix44d& clip_from_world) {
        auto f = from_clip_from_world_no_far(clip_from_world);
        f.half_spaces[5] = half_space_from_vec4(
            FVector4d{
                clip_from_world.M[2][0],
                clip_from_world.M[2][1],
                clip_from_world.M[2][2],
                clip_from_world.M[2][3]
            }
        );

        return f;
    }
};
