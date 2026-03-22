#if WITH_DEV_AUTOMATION_TESTS

#include "tests_shared.h"

#include "preprocess_dataset.h"
#include "HAL/PlatformProcess.h"
#include "Misc/ScopeExit.h"

using namespace preprocess;
using namespace preprocess::tests;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPreprocessDatasetCreateEmptyDatasetTest,
    "UDLODPreprocessor.Preprocess.Dataset.CreateEmptyDataset",
    TestFlags)

bool FPreprocessDatasetCreateEmptyDatasetTest::RunTest(const FString& Parameters) {
    GDALAllRegister();

    const FString root = make_unique_test_root(TEXT("DatasetCreateEmpty"));
    ON_SCOPE_EXIT { delete_tree(root); };

    IFileManager::Get().MakeDirectory(*root, true);

    FPreprocessContext context = make_context(
        root,
        EAttachmentFormat::Rgba8U,
        128,
        2,
        static_cast<EGDALDataType>(GDT_Byte));
    context.no_data_value = 255.0;
    context.rasterbands = {
        make_band(EGDALColorInterp::GCI_RedBand),
        make_band(EGDALColorInterp::GCI_GreenBand),
        make_band(EGDALColorInterp::GCI_BlueBand),
        make_band(EGDALColorInterp::GCI_AlphaBand)
    };

    const FString dataset_path = FPaths::Combine(root, TEXT("rgba8_tile.tif"));
    TOptional transform{GeoTransformRef{new double[6]{1000.0, 2.0, 0.0, 2000.0, 0.0, -2.0}}};

    auto result = create_empty_dataset<uint8>(
        dataset_path,
        FUint64Point{128, 96},
        MoveTemp(transform),
        context);

    TestTrue(TEXT("Dataset should be created"), result.has_value());
    if (!result.has_value()) {
        return false;
    }

    GDALDatasetRef& dataset = result.value();
    dataset->FlushCache();

    TestTrue(TEXT("Dataset file exists at requested path"), FPaths::FileExists(dataset_path));
    TestEqual(TEXT("Dataset width"), dataset->GetRasterXSize(), 128);
    TestEqual(TEXT("Dataset height"), dataset->GetRasterYSize(), 96);
    TestEqual(TEXT("Dataset raster count"), dataset->GetRasterCount(), 4);

    const GeoTransformRef actual_transform = geo_transform(dataset);
    TestEqual(TEXT("GeoTransform[0]"), actual_transform[0], 1000.0);
    TestEqual(TEXT("GeoTransform[1]"), actual_transform[1], 2.0);
    TestEqual(TEXT("GeoTransform[3]"), actual_transform[3], 2000.0);
    TestEqual(TEXT("GeoTransform[5]"), actual_transform[5], -2.0);

    for (int32 index = 0; index < context.rasterbands.Num(); ++index) {
        GDALRasterBand* band = dataset->GetRasterBand(index + 1);
        TestNotNull(TEXT("Band exists"), band);
        if (band == nullptr) {
            return false;
        }

        int has_no_data = 0;
        const double no_data = band->GetNoDataValue(&has_no_data);

        TestTrue(TEXT("Band has no-data value"), has_no_data != 0);
        TestEqual(TEXT("Band no-data value"), no_data, context.no_data_value.GetValue());
        TestEqual(
            TEXT("Band color interpretation"),
            band->GetColorInterpretation(),
            static_cast<int32>(context.rasterbands[index].color_interp));
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPreprocessDatasetTileLifecycleTest,
    "UDLODPreprocessor.Preprocess.Dataset.TileLifecycle",
    TestFlags)

bool FPreprocessDatasetTileLifecycleTest::RunTest(const FString& Parameters) {
    GDALAllRegister();

    const FString root = make_unique_test_root(TEXT("DatasetTileLifecycle"));
    ON_SCOPE_EXIT { delete_tree(root); };

    FPreprocessContext context = make_context(root);
    const FTileCoordinate tile{1u, 3u, FIntPoint{9, 10}};
    const FString tile_path = tile.path(context.tile_dir);

    auto create_result = create_tile_dataset<float>(tile, context);
    TestTrue(TEXT("Tile dataset should be created"), create_result.has_value());
    if (!create_result.has_value()) {
        return false;
    }

    GDALDatasetRef create_dataset = MoveTemp(create_result.value());
    create_dataset->FlushCache();
    create_dataset.Reset();
    FPlatformProcess::Sleep(1.0f);

    TestTrue(TEXT("Tile directory exists"), IFileManager::Get().DirectoryExists(*FPaths::GetPath(tile_path)));
    TestTrue(TEXT("Tile file exists"), FPaths::FileExists(tile_path));

    auto update_result = update_tile_dataset(tile, context);
    TestTrue(TEXT("Existing tile can be reopened for update"), update_result.has_value());
    if (!update_result.has_value()) {
        return false;
    }
    TestEqual(TEXT("Updated dataset access"), update_result.value()->GetAccess(), GA_Update);

    auto load_existing_result = load_tile_dataset_if_exists(tile, context);
    TestTrue(TEXT("Existing tile can be loaded"), load_existing_result.has_value());
    if (!load_existing_result.has_value()) {
        return false;
    }
    TestTrue(TEXT("Existing tile returns a dataset"), load_existing_result.value().IsSet());

    const FTileCoordinate missing_tile{1u, 3u, FIntPoint{10, 11}};
    auto load_missing_result = load_tile_dataset_if_exists(missing_tile, context);
    TestTrue(TEXT("Missing tile query succeeds"), load_missing_result.has_value());
    if (!load_missing_result.has_value()) {
        return false;
    }
    TestFalse(TEXT("Missing tile returns NullOpt"), load_missing_result.value().IsSet());

    auto update_missing_result = update_tile_dataset(missing_tile, context);
    TestFalse(TEXT("Missing tile cannot be opened for update"), update_missing_result.has_value());

    return true;
}

#endif
