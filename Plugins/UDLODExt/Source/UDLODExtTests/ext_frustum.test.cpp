#if WITH_DEV_AUTOMATION_TESTS

#include "tests_shared.h"

#include "ext_frustum.h"

using namespace udlodext::tests;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FUDLODExtFrustumHalfspaceFromVec4Test,
    "UDLODExt.Frustum.HalfspaceFromVec4",
    TestFlags)

bool FUDLODExtFrustumHalfspaceFromVec4Test::RunTest(const FString& Parameters) {
    const UE::Geometry::FHalfspace3d plane = half_space_from_vec4(FVector4d{1.0, 2.0, 3.0, 4.0});
    return check_halfspace(*this, TEXT("Halfspace"), plane, FVector4d{1.0, 2.0, 3.0, 4.0});
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FUDLODExtFrustumPlaneExtractionTest,
    "UDLODExt.Frustum.PlaneExtraction",
    TestFlags)

bool FUDLODExtFrustumPlaneExtractionTest::RunTest(const FString& Parameters) {
    const FMatrix44d clip_from_world = make_matrix44d({
        1.0, 2.0, 3.0, 4.0,
        5.0, 6.0, 7.0, 8.0,
        9.0, 10.0, 11.0, 12.0,
        13.0, 14.0, 15.0, 16.0
    });

    const Frustum without_far = Frustum::from_clip_from_world_no_far(clip_from_world);
    const Frustum full = Frustum::from_clip_from_world(clip_from_world);

    const bool left_ok = check_halfspace(
        *this,
        TEXT("Left plane"),
        without_far.half_spaces[0],
        FVector4d{14.0, 16.0, 18.0, 20.0});
    const bool right_ok = check_halfspace(
        *this,
        TEXT("Right plane"),
        without_far.half_spaces[1],
        FVector4d{12.0, 12.0, 12.0, 12.0});
    const bool bottom_ok = check_halfspace(
        *this,
        TEXT("Bottom plane"),
        without_far.half_spaces[2],
        FVector4d{18.0, 20.0, 22.0, 24.0});
    const bool top_ok = check_halfspace(
        *this,
        TEXT("Top plane"),
        without_far.half_spaces[3],
        FVector4d{8.0, 8.0, 8.0, 8.0});
    const bool near_ok = check_halfspace(
        *this,
        TEXT("Near plane"),
        without_far.half_spaces[4],
        FVector4d{22.0, 24.0, 26.0, 28.0});
    const bool far_ok = check_halfspace(
        *this,
        TEXT("Far plane"),
        full.half_spaces[5],
        FVector4d{9.0, 10.0, 11.0, 12.0});

    return left_ok && right_ok && bottom_ok && top_ok && near_ok && far_ok;
}

#endif
