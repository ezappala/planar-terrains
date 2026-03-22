#if WITH_DEV_AUTOMATION_TESTS

#include "tests_shared.h"

#include <bit>

#include "preprocess_gdal_extended.h"

using namespace preprocess;
using namespace preprocess::tests;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPreprocessGdalExtendedReadWriteRoundTripTest,
    "UDLODPreprocessor.Preprocess.GdalExtended.ReadWriteRoundTrip",
    TestFlags)

bool FPreprocessGdalExtendedReadWriteRoundTripTest::RunTest(const FString& Parameters) {
    const GDALDatasetRef dataset = create_memory_dataset<float>(2, 2);
    GDALRasterBand* band = dataset->GetRasterBand(1);

    ext::Buffer write_buffer{
        TArray{1.0f, 2.0f, 3.0f, 4.0f},
        usize_c{2, 2}
    };

    const auto write_result = write<float>(band, isize_c{0, 0}, usize_c{2, 2}, write_buffer);
    TestTrue(TEXT("Write succeeds"), write_result.has_value());
    if (!write_result.has_value()) { return false; }

    const auto read_result = read_as<float>(band, isize_c{0, 0}, usize_c{2, 2}, usize_c{2, 2});
    TestTrue(TEXT("Read succeeds"), read_result.has_value());
    if (!read_result.has_value()) { return false; }

    ext::Buffer<float> read_buffer = read_result.value();
    TestEqual(TEXT("Buffer length"), read_buffer.len(), 4u);
    TestEqual(TEXT("Pixel 0"), read_buffer.data()[0], 1.0f);
    TestEqual(TEXT("Pixel 1"), read_buffer.data()[1], 2.0f);
    TestEqual(TEXT("Pixel 2"), read_buffer.data()[2], 3.0f);
    TestEqual(TEXT("Pixel 3"), read_buffer.data()[3], 4.0f);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPreprocessGdalExtendedRasterbandEnumerationTest,
    "UDLODPreprocessor.Preprocess.GdalExtended.RasterbandEnumeration",
    TestFlags)

bool FPreprocessGdalExtendedRasterbandEnumerationTest::RunTest(const FString& Parameters) {
    const GDALDatasetRef dataset = create_memory_dataset<uint16>(4, 4, 2);

    const TArray<std::expected<GDALRasterBand*, CPLErrorNum>> band_results = rasterbands(dataset);
    TestEqual(TEXT("Rasterband result count"), band_results.Num(), 2);
    if (band_results.Num() != 2) { return false; }
    TestTrue(TEXT("First rasterband is valid"), band_results[0].has_value());
    TestTrue(TEXT("Second rasterband is valid"), band_results[1].has_value());

    const auto collected_bands = try_collect_rasterbands(dataset);
    TestTrue(TEXT("Collecting rasterbands succeeds"), collected_bands.has_value());
    if (!collected_bands.has_value()) { return false; }
    TestEqual(TEXT("Collected rasterband count"), collected_bands.value().Num(), 2);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPreprocessGdalExtendedApplyBitmaskTest,
    "UDLODPreprocessor.Preprocess.GdalExtended.ApplyBitmask",
    TestFlags)

bool FPreprocessGdalExtendedApplyBitmaskTest::RunTest(const FString& Parameters) {
    TestEqual(
        TEXT("Integer mask clears low bit"),
        apply_bitmask<uint16>(7, 0),
        static_cast<uint16>(6));
    TestEqual(
        TEXT("Integer mask sets low bit"),
        apply_bitmask<uint16>(6, 1),
        static_cast<uint16>(7));

    const float float_value = std::bit_cast<float>(0x3F800003u);
    const float masked_off = apply_bitmask(float_value, 0);
    const float masked_on = apply_bitmask(float_value, 1);
    const double double_value = std::bit_cast<double>(0x3FF0000000000003ULL);
    const double masked_double_off = apply_bitmask(double_value, 0);
    const double masked_double_on = apply_bitmask(double_value, 1);

    TestEqual(TEXT("Float mask clears low bit"), std::bit_cast<uint32>(masked_off), 0x3F800002u);
    TestEqual(
        TEXT("Float mask preserves low bit when set"),
        std::bit_cast<uint32>(masked_on),
        0x3F800003u);
    TestEqual(
        TEXT("Double mask clears low bit"),
        std::bit_cast<uint64>(masked_double_off),
        0x3FF0000000000002ULL);
    TestEqual(
        TEXT("Double mask preserves low bit when set"),
        std::bit_cast<uint64>(masked_double_on),
        0x3FF0000000000003ULL);

    return true;
}

#endif
