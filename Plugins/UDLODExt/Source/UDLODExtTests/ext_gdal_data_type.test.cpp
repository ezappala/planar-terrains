#if WITH_DEV_AUTOMATION_TESTS

#include "tests_shared.h"

#include "ext_gdal_data_type.h"

using namespace udlodext::tests;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FUDLODExtGdalDataTypeEnumMappingTest,
    "UDLODExt.GDALDataType.EnumMapping",
    TestFlags)

bool FUDLODExtGdalDataTypeEnumMappingTest::RunTest(const FString& Parameters) {
    const TArray<TTuple<EGDALDataType, GDALDataType>> cases{
        {EGDALDataType::GDT_Unknown, GDT_Unknown},
        {EGDALDataType::GDT_Byte, GDT_Byte},
        {EGDALDataType::GDT_Int8, GDT_Int8},
        {EGDALDataType::GDT_UInt16, GDT_UInt16},
        {EGDALDataType::GDT_Int16, GDT_Int16},
        {EGDALDataType::GDT_UInt32, GDT_UInt32},
        {EGDALDataType::GDT_Int32, GDT_Int32},
        {EGDALDataType::GDT_UInt64, GDT_UInt64},
        {EGDALDataType::GDT_Int64, GDT_Int64},
        {EGDALDataType::GDT_Float32, GDT_Float32},
        {EGDALDataType::GDT_Float64, GDT_Float64},
        {EGDALDataType::GDT_CInt16, GDT_CInt16},
        {EGDALDataType::GDT_CInt32, GDT_CInt32},
        {EGDALDataType::GDT_CFloat32, GDT_CFloat32},
        {EGDALDataType::GDT_CFloat64, GDT_CFloat64},
    };

    for (const auto& [input, expected] : cases) {
        TestEqual(
            FString::Printf(TEXT("Enum %d maps to GDAL type"), static_cast<uint8>(input)),
            to_gdal_data_type(input),
            expected);
    }

    AddExpectedMessagePlain(
        TEXT("Unrecognized data type 255, defaulting to Unknown"),
        ELogVerbosity::Warning,
        EAutomationExpectedMessageFlags::Exact,
        1);
    TestEqual(
        TEXT("Unknown enum values fall back to GDAL unknown"),
        to_gdal_data_type(static_cast<EGDALDataType>(255)),
        GDT_Unknown);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FUDLODExtGdalDataTypeTraitMappingTest,
    "UDLODExt.GDALDataType.TraitMapping",
    TestFlags)

bool FUDLODExtGdalDataTypeTraitMappingTest::RunTest(const FString& Parameters) {
    TestEqual(
        TEXT("void trait maps to unknown"),
        ext::GDALDataType::GDALDataType<void>::gdal_original(),
        ext::GDALDataType::GDT_Unknown);
    TestEqual(
        TEXT("uint8 trait maps to byte"),
        ext::GDALDataType::GDALDataType<uint8>::gdal_original(),
        ext::GDALDataType::GDT_Byte);
    TestEqual(
        TEXT("int16 trait maps to int16"),
        ext::GDALDataType::GDALDataType<int16>::gdal_original(),
        ext::GDALDataType::GDT_Int16);
    TestEqual(
        TEXT("uint16 trait maps to uint16"),
        ext::GDALDataType::GDALDataType<uint16>::gdal_original(),
        ext::GDALDataType::GDT_UInt16);
    TestEqual(
        TEXT("int32 trait maps to int32"),
        ext::GDALDataType::GDALDataType<int32>::gdal_original(),
        ext::GDALDataType::GDT_Int32);
    TestEqual(
        TEXT("uint32 trait maps to uint32"),
        ext::GDALDataType::GDALDataType<uint32>::gdal_original(),
        ext::GDALDataType::GDT_UInt32);
    TestEqual(
        TEXT("float trait maps to float32"),
        ext::GDALDataType::GDALDataType<float>::gdal_original(),
        ext::GDALDataType::GDT_Float32);
    TestEqual(
        TEXT("double trait maps to float64"),
        ext::GDALDataType::GDALDataType<double>::gdal_original(),
        ext::GDALDataType::GDT_Float64);
    return true;
}

#endif
