#if WITH_DEV_AUTOMATION_TESTS

#include "tests_shared.h"

#include "ext_matrix2x4.h"
#include "Serialization/BufferArchive.h"
#include "Serialization/MemoryReader.h"

using namespace udlodext::tests;

namespace {
bool check_matrix2x4(
    FAutomationTestBase& test,
    const FString& label,
    const FMatrix2x4& actual,
    std::initializer_list<float> expected) {
    check(expected.size() == 8);

    int32 index = 0;
    bool all_ok = true;
    for (const float value : expected) {
        const int32 row = index / 4;
        const int32 column = index % 4;
        all_ok &= check_near(
            test,
            FString::Printf(TEXT("%s [%d][%d]"), *label, row, column),
            actual.M[row][column],
            value);
        ++index;
    }
    return all_ok;
}
} // namespace

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FUDLODExtMatrix2x4SettersAndMetadataTest,
    "UDLODExt.Matrix2x4.SettersAndMetadata",
    TestFlags)

bool FUDLODExtMatrix2x4SettersAndMetadataTest::RunTest(const FString& Parameters) {
    FMatrix source = FMatrix::Identity;
    double next = 1.0;
    for (int32 row = 0; row < 4; ++row) {
        for (int32 column = 0; column < 4; ++column) {
            source.M[row][column] = next++;
        }
    }

    FMatrix2x4 direct{};
    direct.SetMatrix(source);
    const bool direct_ok = check_matrix2x4(
        *this,
        TEXT("SetMatrix"),
        direct,
        {1.0f, 2.0f, 3.0f, 4.0f, 5.0f, 6.0f, 7.0f, 8.0f});

    FMatrix2x4 transpose{};
    transpose.SetMatrixTranspose(source);
    const bool transpose_ok = check_matrix2x4(
        *this,
        TEXT("SetMatrixTranspose"),
        transpose,
        {1.0f, 5.0f, 9.0f, 13.0f, 2.0f, 6.0f, 10.0f, 14.0f});

    FMatrix2x4 identity{};
    identity.SetIdentity();
    const bool identity_ok = check_matrix2x4(
        *this,
        TEXT("SetIdentity"),
        identity,
        {1.0f, 0.0f, 0.0f, 0.0f, 0.0f, 1.0f, 0.0f, 0.0f});

    TestEqual(
        TEXT("Shader metadata row count"),
        TShaderParameterTypeInfo<FMatrix2x4>::NumRows,
        2);
    TestEqual(
        TEXT("Shader metadata column count"),
        TShaderParameterTypeInfo<FMatrix2x4>::NumColumns,
        4);
    TestEqual(
        TEXT("Shader metadata alignment"),
        TShaderParameterTypeInfo<FMatrix2x4>::Alignment,
        16);
    TestTrue(
        TEXT("Matrix2x4 lives in constant buffers"),
        TShaderParameterTypeInfo<FMatrix2x4>::bIsStoredInConstantBuffer);
    return direct_ok && transpose_ok && identity_ok;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FUDLODExtMatrix2x4ArchiveRoundTripTest,
    "UDLODExt.Matrix2x4.ArchiveRoundTrip",
    TestFlags)

bool FUDLODExtMatrix2x4ArchiveRoundTripTest::RunTest(const FString& Parameters) {
    FMatrix2x4 original{};
    float next = 0.5f;
    for (int32 row = 0; row < 2; ++row) {
        for (int32 column = 0; column < 4; ++column) {
            original.M[row][column] = next;
            next += 0.5f;
        }
    }

    FBufferArchive writer;
    writer << original;

    FMemoryReader reader(writer);
    FMatrix2x4 round_trip{};
    reader << round_trip;

    return check_matrix2x4(
        *this,
        TEXT("Archive round-trip"),
        round_trip,
        {0.5f, 1.0f, 1.5f, 2.0f, 2.5f, 3.0f, 3.5f, 4.0f});
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FUDLODExtMatrix2x4TransposeHelpersTest,
    "UDLODExt.Matrix2x4.TransposeHelpers",
    TestFlags)

bool FUDLODExtMatrix2x4TransposeHelpersTest::RunTest(const FString& Parameters) {
    const TArray<FMatrix44f> source{
        make_matrix44f({
            1.0f, 2.0f, 3.0f, 4.0f,
            5.0f, 6.0f, 7.0f, 8.0f,
            9.0f, 10.0f, 11.0f, 12.0f,
            13.0f, 14.0f, 15.0f, 16.0f
        }),
        make_matrix44f({
            21.0f, 22.0f, 23.0f, 24.0f,
            25.0f, 26.0f, 27.0f, 28.0f,
            29.0f, 30.0f, 31.0f, 32.0f,
            33.0f, 34.0f, 35.0f, 36.0f
        }),
        make_matrix44f({
            41.0f, 42.0f, 43.0f, 44.0f,
            45.0f, 46.0f, 47.0f, 48.0f,
            49.0f, 50.0f, 51.0f, 52.0f,
            53.0f, 54.0f, 55.0f, 56.0f
        })
    };

    FMatrix2x4 single{};
    TransposeTransform(single, source[0]);
    const bool single_ok = check_matrix2x4(
        *this,
        TEXT("Single transpose"),
        single,
        {1.0f, 5.0f, 9.0f, 13.0f, 2.0f, 6.0f, 10.0f, 14.0f});

    TArray<FMatrix2x4> batched;
    batched.SetNum(source.Num());
    TransposeTransforms(batched.GetData(), source.GetData(), source.Num());

    const bool first_ok = check_matrix2x4(
        *this,
        TEXT("Batch transpose 0"),
        batched[0],
        {1.0f, 5.0f, 9.0f, 13.0f, 2.0f, 6.0f, 10.0f, 14.0f});
    const bool second_ok = check_matrix2x4(
        *this,
        TEXT("Batch transpose 1"),
        batched[1],
        {21.0f, 25.0f, 29.0f, 33.0f, 22.0f, 26.0f, 30.0f, 34.0f});
    const bool third_ok = check_matrix2x4(
        *this,
        TEXT("Batch transpose 2"),
        batched[2],
        {41.0f, 45.0f, 49.0f, 53.0f, 42.0f, 46.0f, 50.0f, 54.0f});

    return single_ok && first_ok && second_ok && third_ok;
}

#endif
