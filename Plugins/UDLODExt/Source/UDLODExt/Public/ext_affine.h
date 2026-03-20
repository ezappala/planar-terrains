#pragma once

#include "ext_matrix2x4.h"
#include "Matrix3x4.h"
#include "MatrixTypes.h"
#include "Math/Vector.h"
#include "Math/Vector4.h"

using FMat3f = UE::Geometry::TMatrix3<float>;

struct FAffine3f {
    FVector3f x_axis;
    FVector3f y_axis;
    FVector3f z_axis;
    FVector3f translation;

    static FAffine3f FromTransform(const FTransform& InTransform) {
        const FTransform3f T(InTransform);

        const FQuat4f Rotation = T.GetRotation();
        const FVector3f Scale = T.GetScale3D();

        FAffine3f Out;
        Out.x_axis = Rotation.GetAxisX() * Scale.X;
        Out.y_axis = Rotation.GetAxisY() * Scale.Y;
        Out.z_axis = Rotation.GetAxisZ() * Scale.Z;
        Out.translation = T.GetTranslation();
        return Out;
    }
};

struct FMatrix3x4f_Packed {
    FVector4f Row0;
    FVector4f Row1;
    FVector4f Row2;
};

static FMatrix3x4f_Packed ToTranspose(const FAffine3f& a) {
    return {
        FVector4f{a.x_axis.X, a.y_axis.X, a.z_axis.X, a.translation.X},
        FVector4f{a.x_axis.Y, a.y_axis.Y, a.z_axis.Y, a.translation.Y},
        FVector4f{a.x_axis.Z, a.y_axis.Z, a.z_axis.Z, a.translation.Z}
    };
}

FORCEINLINE FMatrix3x4 to_world_from_local_packed(const FTransform& tf) {
    const FMatrix44f matrix = FTransform3f(tf).ToMatrixWithScale();
    FMatrix3x4 packed;
    matrix.To3x4MatrixTranspose(&packed.M[0][0]);
    return packed;
}

struct FNormalMatrixPacked {
    FMatrix2x4 rows;
    float last;
};

FORCEINLINE FNormalMatrixPacked to_local_from_world_normal_packed(const FTransform& tf) {
    const FAffine3f a = FAffine3f::FromTransform(tf);
    const FMat3f linear{
        a.x_axis.X,
        a.y_axis.X,
        a.z_axis.X,
        a.x_axis.Y,
        a.y_axis.Y,
        a.z_axis.Y,
        a.x_axis.Z,
        a.y_axis.Z,
        a.z_axis.Z
    };

    const FMat3f normal_mat = linear.Inverse().Transpose();
    FNormalMatrixPacked out;
    out.rows.M[0][0] = normal_mat.Row0[0];
    out.rows.M[0][1] = normal_mat.Row0[1];
    out.rows.M[0][2] = normal_mat.Row0[2];
    out.rows.M[0][3] = 0.f;

    out.rows.M[1][0] = normal_mat.Row1[0];
    out.rows.M[1][1] = normal_mat.Row1[1];
    out.rows.M[1][2] = normal_mat.Row1[2];
    out.rows.M[1][3] = 0.f;

    out.last = normal_mat.Row2[2];
    return out;
}
