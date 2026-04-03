#if WITH_DEV_AUTOMATION_TESTS

#include "tests_shared.h"

#include "ext_math.h"

using namespace udlodext::tests;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FUDLODExtMathScaleTest,
    "UDLODExt.Math.ScaleUsesPlanarXZ",
    TestFlags)

bool FUDLODExtMathScaleTest::RunTest(const FString& Parameters) {
    const FVector3f scaled = ext::math::scale(512.5);

    check_near(*this, TEXT("Planar X scale"), scaled.X, 512.5f);
    check_near(*this, TEXT("Vertical scale stays one"), scaled.Y, 1.0f);
    check_near(*this, TEXT("Planar Z scale"), scaled.Z, 512.5f);
    return true;
}

#endif
