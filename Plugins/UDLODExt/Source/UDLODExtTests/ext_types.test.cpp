#if WITH_DEV_AUTOMATION_TESTS

#include "tests_shared.h"

#include "ext_types.h"
#include "Math/IntPoint.h"

using namespace udlodext::tests;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FUDLODExtTypesCoordinateArithmeticTest,
    "UDLODExt.Types.CoordinateArithmetic",
    TestFlags)

bool FUDLODExtTypesCoordinateArithmeticTest::RunTest(const FString& Parameters) {
    using ext::types::usize_c;

    const usize_c base{6u, 9u};
    const usize_c other{2u, 3u};

    const usize_c minus_scalar = base - 1u;
    TestEqual(TEXT("Scalar subtraction x"), minus_scalar.Get<0>(), 5u);
    TestEqual(TEXT("Scalar subtraction y"), minus_scalar.Get<1>(), 8u);

    const usize_c minus_tuple = base - other;
    TestEqual(TEXT("Tuple subtraction x"), minus_tuple.Get<0>(), 4u);
    TestEqual(TEXT("Tuple subtraction y"), minus_tuple.Get<1>(), 6u);

    const usize_c plus_scalar = base + 4u;
    TestEqual(TEXT("Scalar addition x"), plus_scalar.Get<0>(), 10u);
    TestEqual(TEXT("Scalar addition y"), plus_scalar.Get<1>(), 13u);

    const usize_c plus_tuple = base + other;
    TestEqual(TEXT("Tuple addition x"), plus_tuple.Get<0>(), 8u);
    TestEqual(TEXT("Tuple addition y"), plus_tuple.Get<1>(), 12u);

    const usize_c times_scalar = base * 2u;
    TestEqual(TEXT("Scalar multiply x"), times_scalar.Get<0>(), 12u);
    TestEqual(TEXT("Scalar multiply y"), times_scalar.Get<1>(), 18u);

    const usize_c times_tuple = base * other;
    TestEqual(TEXT("Tuple multiply x"), times_tuple.Get<0>(), 12u);
    TestEqual(TEXT("Tuple multiply y"), times_tuple.Get<1>(), 27u);

    const usize_c div_scalar = base / 3u;
    TestEqual(TEXT("Scalar division x"), div_scalar.Get<0>(), 2u);
    TestEqual(TEXT("Scalar division y"), div_scalar.Get<1>(), 3u);

    const usize_c div_tuple = base / other;
    TestEqual(TEXT("Tuple division x"), div_tuple.Get<0>(), 3u);
    TestEqual(TEXT("Tuple division y"), div_tuple.Get<1>(), 3u);

    const usize_c transposed = base.transpose();
    TestEqual(TEXT("Transpose swaps x"), transposed.Get<0>(), 9u);
    TestEqual(TEXT("Transpose swaps y"), transposed.Get<1>(), 6u);

    const FIntPoint point = static_cast<FIntPoint>(base);
    TestEqual(TEXT("FIntPoint x"), point.X, 6);
    TestEqual(TEXT("FIntPoint y"), point.Y, 9);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FUDLODExtTypesStructuredBindingsTest,
    "UDLODExt.Types.StructuredBindings",
    TestFlags)

bool FUDLODExtTypesStructuredBindingsTest::RunTest(const FString& Parameters) {
    const ext::types::isize_c coord{-2, 5};
    const auto [row, column] = coord;

    TestEqual(TEXT("Structured binding first element"), row, static_cast<SSIZE_T>(-2));
    TestEqual(TEXT("Structured binding second element"), column, static_cast<SSIZE_T>(5));

    const FIntPoint point = static_cast<FIntPoint>(coord.transpose());
    TestEqual(TEXT("Transposed FIntPoint x"), point.X, 5);
    TestEqual(TEXT("Transposed FIntPoint y"), point.Y, -2);
    return true;
}

#endif
