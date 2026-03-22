#if WITH_DEV_AUTOMATION_TESTS

#include "tests_shared.h"

#include "preprocess_result.h"

using namespace preprocess;
using namespace preprocess::tests;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPreprocessResultToStringTest,
    "UDLODPreprocessor.Preprocess.Result.ToString",
    TestFlags)

bool FPreprocessResultToStringTest::RunTest(const FString& Parameters) {
    const FPreprocessError gdal_error = FPreprocessError::Gdal(
        CPLE_OpenFailed,
        TEXT("failed to open"));
    const FPreprocessError parse_error = FPreprocessError::Parse(TEXT("bad input"));

    TestEqual(
        TEXT("Unknown type string"),
        FPreprocessError::UnknownRasterbandDataType().ToString(),
        FString(TEXT("Unknown rasterband data type")));
    TestEqual(
        TEXT("Transform failure string"),
        FPreprocessError::TransformOperationFailed().ToString(),
        FString(TEXT("Transform operation failed")));
    TestEqual(
        TEXT("No-data range string"),
        FPreprocessError::NoDataOutOfRange().ToString(),
        FString(TEXT("The no data value is outside of the datatypes range.")));
    TestEqual(
        TEXT("GDAL string"),
        gdal_error.ToString(),
        FString(TEXT("GDAL error (4): failed to open")));
    TestEqual(
        TEXT("Parse string"),
        parse_error.ToString(),
        FString(TEXT("Parse error: bad input")));
    TestEqual(
        TEXT("Canceled string"),
        FPreprocessError::Canceled().ToString(),
        FString(TEXT("Preprocessing canceled")));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPreprocessResultFactoryHelpersTest,
    "UDLODPreprocessor.Preprocess.Result.FactoryHelpers",
    TestFlags)

bool FPreprocessResultFactoryHelpersTest::RunTest(const FString& Parameters) {
    const auto parse_result = make_parse_error_result<int32>(TEXT("parse helper"));
    TestFalse(TEXT("Parse helper returns unexpected"), parse_result.has_value());
    if (parse_result.has_value()) { return false; }
    TestEqual(
        TEXT("Parse helper message"),
        parse_result.error().ToString(),
        FString(TEXT("Parse error: parse helper")));

    const FPreprocessError gdal_error = FPreprocessError::From(CPLE_IllegalArg);
    TestEqual(
        TEXT("From helper uses GDAL type"),
        static_cast<int32>(gdal_error.Type),
        static_cast<int32>(EPreprocessError::Gdal));

    return true;
}

#endif
