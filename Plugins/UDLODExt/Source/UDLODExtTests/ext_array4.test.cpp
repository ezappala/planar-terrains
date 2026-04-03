#if WITH_DEV_AUTOMATION_TESTS

#include "tests_shared.h"

#include "ext_array4.h"

using namespace udlodext::tests;

namespace {
TArray4D<int32> make_linear_array4() {
    TArray4D<int32> array(2, 2, 2, 2);
    int32 value = 0;
    for (SIZE_T i0 = 0; i0 < array.get_dim0(); ++i0) {
        for (SIZE_T i1 = 0; i1 < array.get_dim1(); ++i1) {
            for (SIZE_T i2 = 0; i2 < array.get_dim2(); ++i2) {
                for (SIZE_T i3 = 0; i3 < array.get_dim3(); ++i3) {
                    array(i0, i1, i2, i3) = value++;
                }
            }
        }
    }
    return array;
}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FUDLODExtArray4ConstructionAndMutationTest,
    "UDLODExt.Array4.ConstructionAndMutation",
    TestFlags)

bool FUDLODExtArray4ConstructionAndMutationTest::RunTest(const FString& Parameters) {
    TArray4D<int32> empty;
    TestTrue(TEXT("Default-constructed array is empty"), empty.IsEmpty());
    TestEqual(TEXT("Default size is zero"), empty.size(), static_cast<SIZE_T>(0));

    TArray4D<int32> array(2, 1, 2, 2, 5);
    TestEqual(TEXT("dim0"), array.get_dim0(), static_cast<SIZE_T>(2));
    TestEqual(TEXT("dim1"), array.get_dim1(), static_cast<SIZE_T>(1));
    TestEqual(TEXT("dim2"), array.get_dim2(), static_cast<SIZE_T>(2));
    TestEqual(TEXT("dim3"), array.get_dim3(), static_cast<SIZE_T>(2));
    TestEqual(TEXT("Total size"), array.size(), static_cast<SIZE_T>(8));
    TestFalse(TEXT("Sized array is not empty"), array.IsEmpty());

    int32 value = 0;
    for (SIZE_T i0 = 0; i0 < array.get_dim0(); ++i0) {
        for (SIZE_T i1 = 0; i1 < array.get_dim1(); ++i1) {
            for (SIZE_T i2 = 0; i2 < array.get_dim2(); ++i2) {
                for (SIZE_T i3 = 0; i3 < array.get_dim3(); ++i3) {
                    array(i0, i1, i2, i3) = value++;
                }
            }
        }
    }

    TestTrue(
        TEXT("Storage is laid out with the last axis varying fastest"),
        array.get_storage() == TArray<int32>{0, 1, 2, 3, 4, 5, 6, 7});

    array.fill(-2);
    array.map_in_place([](int32& element) { element *= -3; });
    TestTrue(
        TEXT("fill and map_in_place update every element"),
        array.get_storage() == TArray<int32>{6, 6, 6, 6, 6, 6, 6, 6});
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FUDLODExtArray4ViewsSlicesAndReshapeTest,
    "UDLODExt.Array4.ViewsSlicesAndReshape",
    TestFlags)

bool FUDLODExtArray4ViewsSlicesAndReshapeTest::RunTest(const FString& Parameters) {
    TArray4D<int32> array = make_linear_array4();
    TestTrue(TEXT("Reshape succeeds when element count matches"), array.reshape(1, 4, 2, 2));
    TestEqual(TEXT("Reshaped dim0"), array.get_dim0(), static_cast<SIZE_T>(1));
    TestEqual(TEXT("Reshaped dim1"), array.get_dim1(), static_cast<SIZE_T>(4));
    TestFalse(TEXT("Reshape rejects mismatched sizes"), array.reshape(3, 3, 1, 1));

    TArray4D<int32> view_source = make_linear_array4();
    auto full_view = view_source.AsView();
    full_view(1, 0, 1, 1) = 77;
    TestEqual(TEXT("Full view writes through to storage"), view_source(1, 0, 1, 1), 77);

    auto axis0 = view_source.SliceAxis(0, 1, 1);
    axis0(0, 1, 1, 0) = 88;
    TestEqual(TEXT("Axis 0 slice tracks original memory"), view_source(1, 1, 1, 0), 88);

    auto axis1 = view_source.SliceAxis(1, 1, 1);
    axis1(1, 0, 0, 1) = 99;
    TestEqual(TEXT("Axis 1 slice tracks original memory"), view_source(1, 1, 0, 1), 99);

    auto axis2 = view_source.SliceAxis(2, 1, 1);
    axis2(0, 1, 0, 1) = 111;
    TestEqual(TEXT("Axis 2 slice tracks original memory"), view_source(0, 1, 1, 1), 111);

    auto axis3 = view_source.SliceAxis(3, 1, 1);
    axis3(1, 0, 1, 0) = 123;
    TestEqual(TEXT("Axis 3 slice tracks original memory"), view_source(1, 0, 1, 1), 123);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FUDLODExtArray4HashAndReleaseStorageTest,
    "UDLODExt.Array4.HashAndReleaseStorage",
    TestFlags)

bool FUDLODExtArray4HashAndReleaseStorageTest::RunTest(const FString& Parameters) {
    TArray4D<int32> lhs = make_linear_array4();
    TArray4D<int32> rhs = make_linear_array4();

    TestTrue(TEXT("Equal arrays compare equal"), lhs == rhs);
    TestEqual(TEXT("Equal arrays hash the same"), GetTypeHash(lhs), GetTypeHash(rhs));

    rhs(1, 1, 1, 1) = -1;
    TestFalse(TEXT("Different arrays compare unequal"), lhs == rhs);

    TArray<int32> released = lhs.release_storrage();
    TestTrue(
        TEXT("release_storrage returns the contiguous payload"),
        released == TArray<int32>{0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15});
    return true;
}

#endif
