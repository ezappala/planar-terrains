#if WITH_DEV_AUTOMATION_TESTS

#include "tests_shared.h"

#include "ext_buffer.h"

using namespace udlodext::tests;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FUDLODExtBufferShapeAndIndexingTest,
    "UDLODExt.Buffer.ShapeAndIndexing",
    TestFlags)

bool FUDLODExtBufferShapeAndIndexingTest::RunTest(const FString& Parameters) {
    ext::Buffer<int32> buffer(
        TArray<int32>{10, 11, 12, 13, 14, 15},
        ext::types::usize_c{3u, 2u});

    TestEqual(TEXT("Width matches first shape element"), buffer.width(), static_cast<SIZE_T>(3));
    TestEqual(TEXT("Height matches second shape element"), buffer.height(), static_cast<SIZE_T>(2));
    TestEqual(TEXT("Length matches backing storage"), buffer.len(), static_cast<SIZE_T>(6));
    TestFalse(TEXT("Populated buffer is not empty"), buffer.is_empty());
    TestEqual(TEXT("Row-major origin lookup"), buffer[ext::types::isize_c{0, 0}], 10);
    TestEqual(TEXT("Row-major first row third column"), buffer[ext::types::isize_c{0, 2}], 12);
    TestEqual(TEXT("Row-major second row second column"), buffer[ext::types::isize_c{1, 1}], 14);
    TestEqual(
        TEXT("Row-major vector index"),
        buffer.vec_index_for(ext::types::isize_c{1, 2}),
        static_cast<SIZE_T>(5));

    TArray<int32> visited;
    for (const int32 value : buffer) { visited.Add(value); }
    TestTrue(
        TEXT("Range-for iterates contiguous storage in order"),
        visited == TArray<int32>{10, 11, 12, 13, 14, 15});

    ext::Buffer<float> empty({}, ext::types::usize_c{0u, 0u});
    TestTrue(TEXT("Zero-sized buffer reports empty"), empty.is_empty());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FUDLODExtBufferMoveUnpacksShapeAndDataTest,
    "UDLODExt.Buffer.MoveUnpacksShapeAndData",
    TestFlags)

bool FUDLODExtBufferMoveUnpacksShapeAndDataTest::RunTest(const FString& Parameters) {
    ext::Buffer<uint16> buffer(
        TArray<uint16>{7, 8, 9, 10},
        ext::types::usize_c{2u, 2u});

    auto [shape, data] = MoveTemp(buffer).into_shape_and_vec();
    TestEqual(TEXT("Moved shape width"), shape.Get<0>(), static_cast<SSIZE_T>(2));
    TestEqual(TEXT("Moved shape height"), shape.Get<1>(), static_cast<SSIZE_T>(2));
    TestTrue(TEXT("Moved data preserves values"), data == TArray<uint16>{7, 8, 9, 10});
    return true;
}

#endif
