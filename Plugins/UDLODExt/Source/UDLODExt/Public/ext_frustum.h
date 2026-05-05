#pragma once

#include "HalfspaceTypes.h"
#include "Containers/StaticArray.h"
#include "Math/Matrix.h"
#include "Math/Vector.h"

struct Frustum {
    TStaticArray<UE::Geometry::FHalfspace3d, 6> half_spaces;
    uint32 num_halfspaces = 0;

    static FVector4d MatrixColumn(const FMatrix44d& m, const int32 col) {
        return FVector4d{
            m.M[0][col],
            m.M[1][col],
            m.M[2][col],
            m.M[3][col]
        };
    }

    static UE::Geometry::FHalfspace3d HalfspaceFromClipInequality(const FVector4d& plane) {
        FVector3d normal{plane.X, plane.Y, plane.Z};
        double constant = -plane.W;

        const double len = normal.Length();
        if (len > UE_SMALL_NUMBER) {
            normal /= len;
            constant /= len;
        }

        return UE::Geometry::FHalfspace3d{normal, constant};
    }

    static Frustum FromClipFromSpaceNoFar(const FMatrix44d& clip_from_space) {
        const FVector4d col0 = MatrixColumn(clip_from_space, 0); // clip x
        const FVector4d col1 = MatrixColumn(clip_from_space, 1); // clip y
        const FVector4d col2 = MatrixColumn(clip_from_space, 2); // clip z
        const FVector4d col3 = MatrixColumn(clip_from_space, 3); // clip w

        Frustum f;
        f.num_halfspaces = 5;

        f.half_spaces[0] = HalfspaceFromClipInequality(col3 + col0); // left:   x >= -w
        f.half_spaces[1] = HalfspaceFromClipInequality(col3 - col0); // right:  x <=  w
        f.half_spaces[2] = HalfspaceFromClipInequality(col3 + col1); // bottom: y >= -w
        f.half_spaces[3] = HalfspaceFromClipInequality(col3 - col1); // top:    y <=  w
        f.half_spaces[4] = HalfspaceFromClipInequality(col2); // near:   z >=  0

        f.half_spaces[5] = UE::Geometry::FHalfspace3d{FVector3d::ZeroVector, -1.0};

        return f;
    }

    static Frustum FromClipFromSpaceWithFar(const FMatrix44d& clip_from_space) {
        Frustum f = FromClipFromSpaceNoFar(clip_from_space);

        const FVector4d col2 = MatrixColumn(clip_from_space, 2); // clip z
        const FVector4d col3 = MatrixColumn(clip_from_space, 3); // clip w

        f.num_halfspaces = 6;

        // Far: z <= w, therefore w - z >= 0.
        f.half_spaces[5] = HalfspaceFromClipInequality(col3 - col2);

        return f;
    }
};
