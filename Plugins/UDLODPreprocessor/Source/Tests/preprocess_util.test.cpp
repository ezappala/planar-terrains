#if WITH_DEV_AUTOMATION_TESTS

#include "tests_shared.h"

#include "preprocess_util.h"
#include "Misc/FileHelper.h"
#include "Misc/ScopeExit.h"

using namespace preprocess;
using namespace preprocess::tests;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPreprocessUtilDirectoryHelpersTest,
    "UDLODPreprocessor.Preprocess.Util.DirectoryHelpers",
    TestFlags)

bool FPreprocessUtilDirectoryHelpersTest::RunTest(const FString& Parameters) {
    const FString root = make_unique_test_root(TEXT("UtilDirectories"));
    ON_SCOPE_EXIT { delete_tree(root); };

    IFileManager::Get().MakeDirectory(*root, true);
    const FString file_path = FPaths::Combine(root, TEXT("marker.txt"));
    TestTrue(
        TEXT("Marker file written"),
        FFileHelper::SaveStringToFile(TEXT("marker"), *file_path));

    util::clear_directory(root);
    TestTrue(TEXT("Directory recreated"), IFileManager::Get().DirectoryExists(*root));
    TestFalse(TEXT("Directory contents removed"), FPaths::FileExists(file_path));

    util::delete_directory(root);
    TestFalse(TEXT("Directory deleted"), IFileManager::Get().DirectoryExists(*root));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPreprocessUtilNoDataAndDatatypeHelpersTest,
    "UDLODPreprocessor.Preprocess.Util.NoDataAndDatatypeHelpers",
    TestFlags)

bool FPreprocessUtilNoDataAndDatatypeHelpersTest::RunTest(const FString& Parameters) {
    const GDALDatasetRef with_no_data = create_memory_dataset<float>(4, 4);
    TestEqual(
        TEXT("Setting no-data succeeds"),
        with_no_data->GetRasterBand(1)->SetNoDataValue(-11.0),
        CE_None);

    const TOptional<double> no_data_value = util::get_no_data_value(with_no_data);
    TestTrue(TEXT("No-data value is present"), no_data_value.IsSet());
    if (!no_data_value.IsSet()) { return false; }
    TestEqual(TEXT("No-data value"), no_data_value.GetValue(), -11.0);

    const GDALDatasetRef without_no_data = create_memory_dataset<float>(4, 4);
    TestFalse(
        TEXT("Missing no-data returns NullOpt"),
        util::get_no_data_value(without_no_data).IsSet());

    FPreprocessContext context = make_context(make_unique_test_root(TEXT("UtilDatatype")));
    context.data_type = static_cast<EGDALDataType>(GDT_UInt16);
    TestEqual(
        TEXT("Datatype transpose"),
        util::transpose_gdal_datatype(context),
        ext::GDALDataType::GDT_UInt16);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPreprocessUtilSaveTerrainConfigWritesInsideTerrainPathTest,
    "UDLODPreprocessor.Preprocess.Util.SaveTerrainConfigWritesInsideTerrainPath",
    TestFlags)

bool FPreprocessUtilSaveTerrainConfigWritesInsideTerrainPathTest::RunTest(
    const FString& Parameters) {
    const FString root = make_unique_test_root(TEXT("UtilSaveTerrainConfig"));
    ON_SCOPE_EXIT { delete_tree(root); };

    FPreprocessContext context = make_context(root);
    context.terrain_path = FPaths::Combine(root, TEXT("terrain"));
    context.attachment_label = TEXT("height");
    context.lod_count = 9;

    IFileManager::Get().MakeDirectory(*context.terrain_path, true);

    const FString expected_path = FPaths::Combine(context.terrain_path, TEXT("config.json"));
    const FString unexpected_path = context.terrain_path + TEXT("config.json");
    const TArray tiles{FTileCoordinate{0u, 2u, FIntPoint{3, 4}}};

    util::save_terrain_config(tiles, context);

    TestTrue(
        TEXT("Config is written inside the terrain directory"),
        FPaths::FileExists(expected_path));
    TestFalse(
        TEXT("Config is not written by raw string concatenation"),
        expected_path != unexpected_path && FPaths::FileExists(unexpected_path));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPreprocessUtilSaveTerrainConfigHeightResetRemovesStaleAttachmentsTest,
    "UDLODPreprocessor.Preprocess.Util.SaveTerrainConfigHeightResetRemovesStaleAttachments",
    TestFlags)

bool FPreprocessUtilSaveTerrainConfigHeightResetRemovesStaleAttachmentsTest::RunTest(
    const FString& Parameters) {
    const FString root = make_unique_test_root(TEXT("UtilSaveTerrainConfigReset"));
    ON_SCOPE_EXIT { delete_tree(root); };

    const FString terrain_path = FPaths::Combine(root, TEXT("terrain"));
    IFileManager::Get().MakeDirectory(*terrain_path, true);

    FPreprocessContext albedo_context = make_context(root, EAttachmentFormat::Rgba8U);
    albedo_context.terrain_path = terrain_path;
    albedo_context.attachment_label = TEXT("albedo");
    util::save_terrain_config({}, albedo_context);

    FPreprocessContext height_context = make_context(root);
    height_context.terrain_path = terrain_path;
    height_context.attachment_label = TEXT("height");
    height_context.lod_count = 2;
    const TArray tiles{FTileCoordinate{0u, 0u, FIntPoint{0, 0}}};
    util::save_terrain_config(tiles, height_context);

    const auto config = FTerrainConfig::from_file(
        FPaths::Combine(terrain_path, TEXT("config.json")));
    TestTrue(TEXT("Config reload succeeds"), config.IsSet());
    if (!config.IsSet()) { return false; }

    TestEqual(TEXT("Height-only config has one attachment"), config->attachments.Num(), 1);
    TestTrue(TEXT("Height attachment remains"), config->attachments.Contains(TEXT("height")));
    TestFalse(
        TEXT("Stale albedo attachment is removed"),
        config->attachments.Contains(TEXT("albedo")));

    return true;
}

#endif
