#if WITH_DEV_AUTOMATION_TESTS

#include "tests_shared.h"

#include "ext_traits.h"
#include "Math/Vector.h"
#include "Templates/UniquePtr.h"

using namespace udlodext::tests;

namespace {
struct FNonEquatable {
    int32 Value = 0;
};
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FUDLODExtTraitsConceptCoverageTest,
    "UDLODExt.Traits.ConceptCoverage",
    TestFlags)

bool FUDLODExtTraitsConceptCoverageTest::RunTest(const FString& Parameters) {
    constexpr bool float_is_gdal = ext::traits::GdalType<float>;
    constexpr bool string_is_gdal = ext::traits::GdalType<FString>;
    constexpr bool int_is_copy = ext::traits::Copy<int32>;
    constexpr bool unique_ptr_is_copy = ext::traits::Copy<TUniquePtr<int32>>;
    constexpr bool int_is_partial_eq = ext::traits::PartialEq<int32>;
    constexpr bool non_equatable_is_partial_eq = ext::traits::PartialEq<FNonEquatable>;
    constexpr bool double_is_to_primitive = ext::traits::ToPrimitive<double>;
    constexpr bool vector_is_to_primitive = ext::traits::ToPrimitive<FVector>;
    constexpr bool uint32_is_numcast = ext::traits::NumCast<uint32>;
    constexpr bool string_is_numcast = ext::traits::NumCast<FString>;

    TestTrue(TEXT("float satisfies GdalType"), float_is_gdal);
    TestFalse(TEXT("FString does not satisfy GdalType"), string_is_gdal);
    TestTrue(TEXT("int32 satisfies Copy"), int_is_copy);
    TestFalse(TEXT("TUniquePtr does not satisfy Copy"), unique_ptr_is_copy);
    TestTrue(TEXT("int32 satisfies PartialEq"), int_is_partial_eq);
    TestFalse(TEXT("FNonEquatable does not satisfy PartialEq"), non_equatable_is_partial_eq);
    TestTrue(TEXT("double satisfies ToPrimitive"), double_is_to_primitive);
    TestFalse(TEXT("FVector does not satisfy ToPrimitive"), vector_is_to_primitive);
    TestTrue(TEXT("uint32 satisfies NumCast"), uint32_is_numcast);
    TestFalse(TEXT("FString does not satisfy NumCast"), string_is_numcast);
    return true;
}

#endif
