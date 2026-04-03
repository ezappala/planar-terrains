#if WITH_DEV_AUTOMATION_TESTS

#include "tests_shared.h"

#include "ext_affine.h"

using namespace udlodext::tests;

namespace {
FMat3f unpack_normal_matrix(const FNormalMatrixPacked& packed) {
    return FMat3f{
        packed.rows.M[0][0],
        packed.rows.M[0][1],
        packed.rows.M[0][2],
        packed.rows.M[0][3],
        packed.rows.M[1][0],
        packed.rows.M[1][1],
        packed.rows.M[1][2],
        packed.rows.M[1][3],
        packed.last
    };
}

float matrix3_value(const FMat3f& matrix, const int32 row, const int32 column) {
    switch (row) {
    case 0: return matrix.Row0[column];
    case 1: return matrix.Row1[column];
    default: return matrix.Row2[column];
    }
}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FUDLODExtAffineFromTransformAndTransposeTest,
    "UDLODExt.Affine.FromTransformAndTranspose",
    TestFlags)

bool FUDLODExtAffineFromTransformAndTransposeTest::RunTest(const FString& Parameters) {
    const FTransform transform(
        FRotator(10.0, 20.0, 30.0),
        FVector(11.0, 12.0, 13.0),
        FVector(2.0, 3.0, 4.0));
    const FTransform3f transform3f(transform);

    const FAffine3f affine = FAffine3f::FromTransform(transform);
    const FVector3f expected_x = transform3f.GetRotation().GetAxisX() * transform3f.GetScale3D().X;
    const FVector3f expected_y = transform3f.GetRotation().GetAxisY() * transform3f.GetScale3D().Y;
    const FVector3f expected_z = transform3f.GetRotation().GetAxisZ() * transform3f.GetScale3D().Z;

    const bool x_ok =
        check_near(*this, TEXT("X axis x"), affine.x_axis.X, expected_x.X) &&
        check_near(*this, TEXT("X axis y"), affine.x_axis.Y, expected_x.Y) &&
        check_near(*this, TEXT("X axis z"), affine.x_axis.Z, expected_x.Z);
    const bool y_ok =
        check_near(*this, TEXT("Y axis x"), affine.y_axis.X, expected_y.X) &&
        check_near(*this, TEXT("Y axis y"), affine.y_axis.Y, expected_y.Y) &&
        check_near(*this, TEXT("Y axis z"), affine.y_axis.Z, expected_y.Z);
    const bool z_ok =
        check_near(*this, TEXT("Z axis x"), affine.z_axis.X, expected_z.X) &&
        check_near(*this, TEXT("Z axis y"), affine.z_axis.Y, expected_z.Y) &&
        check_near(*this, TEXT("Z axis z"), affine.z_axis.Z, expected_z.Z);
    const bool translation_ok =
        check_near(*this, TEXT("Translation x"), affine.translation.X, 11.0f) &&
        check_near(*this, TEXT("Translation y"), affine.translation.Y, 12.0f) &&
        check_near(*this, TEXT("Translation z"), affine.translation.Z, 13.0f);

    const FMatrix3x4f_Packed packed = ToTranspose(affine);
    const bool packed_ok =
        check_near(*this, TEXT("Packed row0 x"), packed.Row0.X, affine.x_axis.X) &&
        check_near(*this, TEXT("Packed row0 y"), packed.Row0.Y, affine.y_axis.X) &&
        check_near(*this, TEXT("Packed row0 z"), packed.Row0.Z, affine.z_axis.X) &&
        check_near(*this, TEXT("Packed row0 w"), packed.Row0.W, affine.translation.X) &&
        check_near(*this, TEXT("Packed row1 x"), packed.Row1.X, affine.x_axis.Y) &&
        check_near(*this, TEXT("Packed row1 y"), packed.Row1.Y, affine.y_axis.Y) &&
        check_near(*this, TEXT("Packed row1 z"), packed.Row1.Z, affine.z_axis.Y) &&
        check_near(*this, TEXT("Packed row1 w"), packed.Row1.W, affine.translation.Y) &&
        check_near(*this, TEXT("Packed row2 x"), packed.Row2.X, affine.x_axis.Z) &&
        check_near(*this, TEXT("Packed row2 y"), packed.Row2.Y, affine.y_axis.Z) &&
        check_near(*this, TEXT("Packed row2 z"), packed.Row2.Z, affine.z_axis.Z) &&
        check_near(*this, TEXT("Packed row2 w"), packed.Row2.W, affine.translation.Z);

    return x_ok && y_ok && z_ok && translation_ok && packed_ok;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FUDLODExtAffinePackedMatrixHelpersTest,
    "UDLODExt.Affine.PackedMatrixHelpers",
    TestFlags)

bool FUDLODExtAffinePackedMatrixHelpersTest::RunTest(const FString& Parameters) {
    const FTransform transform(
        FRotator(15.0, -25.0, 35.0),
        FVector(-4.0, 8.0, 12.0),
        FVector(2.0, 3.0, 5.0));

    const FMatrix3x4 actual_world = to_world_from_local_packed(transform);
    FMatrix3x4 expected_world;
    FTransform3f(transform).ToMatrixWithScale().To3x4MatrixTranspose(&expected_world.M[0][0]);

    bool world_ok = true;
    for (int32 row = 0; row < 3; ++row) {
        for (int32 column = 0; column < 4; ++column) {
            world_ok &= check_near(
                *this,
                FString::Printf(TEXT("World packed [%d][%d]"), row, column),
                actual_world.M[row][column],
                expected_world.M[row][column]);
        }
    }

    const FAffine3f affine = FAffine3f::FromTransform(transform);
    const FMat3f linear{
        affine.x_axis.X,
        affine.y_axis.X,
        affine.z_axis.X,
        affine.x_axis.Y,
        affine.y_axis.Y,
        affine.z_axis.Y,
        affine.x_axis.Z,
        affine.y_axis.Z,
        affine.z_axis.Z
    };

    const FMat3f expected_normal = linear.Inverse().Transpose();
    const FMat3f actual_normal = unpack_normal_matrix(to_local_from_world_normal_packed(transform));

    bool normal_ok = true;
    for (int32 row = 0; row < 3; ++row) {
        for (int32 column = 0; column < 3; ++column) {
            normal_ok &= check_near(
                *this,
                FString::Printf(TEXT("Normal packed [%d][%d]"), row, column),
                matrix3_value(actual_normal, row, column),
                matrix3_value(expected_normal, row, column));
        }
    }

    return world_ok && normal_ok;
}

#endif
