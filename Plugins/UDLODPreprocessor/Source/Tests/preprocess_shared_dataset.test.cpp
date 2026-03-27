#if WITH_DEV_AUTOMATION_TESTS

#include "tests_shared.h"

#include "preprocess_gdal_extended.h"
#include "preprocess_shared_dataset.h"
#include "HAL/PlatformProcess.h"
#include "Misc/ScopeExit.h"

using namespace preprocess;
using namespace preprocess::tests;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPreprocessSharedDatasetReopensRecreatedPathTest,
    "UDLODPreprocessor.Preprocess.SharedDataset.ReopensRecreatedPath",
    TestFlags)

bool FPreprocessSharedDatasetReopensRecreatedPathTest::RunTest(const FString& Parameters) {
    GDALAllRegister();

    const FString root = make_unique_test_root(TEXT("SharedDatasetReopensRecreatedPath"));
    ON_SCOPE_EXIT { delete_tree(root); };

    const FString dataset_path = FPaths::Combine(root, TEXT("shared.tif"));

    {
        const GDALDatasetRef dataset = create_tiff_dataset<uint8>(dataset_path, 1, 1, 1);
        ext::Buffer first_buffer{TArray<uint8>{7}, usize_c{1, 1}};
        TestTrue(
            TEXT("First dataset write succeeds"),
            write<uint8>(dataset->GetRasterBand(1), {0, 0}, {1, 1}, first_buffer).has_value());
        dataset->FlushCache();
    }

    {
        const SharedDatasetRO shared_dataset{dataset_path};
        auto read_result = read_as<uint8>(
            shared_dataset.get()->GetRasterBand(1),
            {0, 0},
            {1, 1},
            {1, 1});
        TestTrue(TEXT("Initial shared dataset read succeeds"), read_result.has_value());
        if (!read_result.has_value()) { return false; }
        TestEqual(TEXT("Initial value"), read_result.value().data()[0], static_cast<uint8>(7));
    }

    TestTrue(
        TEXT("Dataset can be deleted after shared reader scope"),
        IFileManager::Get().Delete(*dataset_path));
    FPlatformProcess::Sleep(0.1f);

    {
        const GDALDatasetRef dataset = create_tiff_dataset<uint8>(dataset_path, 1, 1, 1);
        ext::Buffer second_buffer{TArray<uint8>{42}, usize_c{1, 1}};
        TestTrue(
            TEXT("Replacement dataset write succeeds"),
            write<uint8>(dataset->GetRasterBand(1), {0, 0}, {1, 1}, second_buffer).has_value());
        dataset->FlushCache();
    }

    {
        const SharedDatasetRO shared_dataset{dataset_path};
        auto read_result = read_as<uint8>(
            shared_dataset.get()->GetRasterBand(1),
            {0, 0},
            {1, 1},
            {1, 1});
        TestTrue(TEXT("Replacement shared dataset read succeeds"), read_result.has_value());
        if (!read_result.has_value()) { return false; }
        TestEqual(TEXT("Replacement value"), read_result.value().data()[0], static_cast<uint8>(42));
    }

    return true;
}

#endif
