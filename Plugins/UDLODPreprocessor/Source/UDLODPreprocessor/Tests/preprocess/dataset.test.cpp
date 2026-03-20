#if WITH_DEV_AUTOMATION_TESTS

#include "preprocess_attachment_config.h"
#include "preprocess_dataset.h"
#include "preprocess_tile_coordinate.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"
#include "Misc/ScopeExit.h"

namespace preprocess::tests {
static FString MakeUniqueTestRoot() {
    return FPaths::ConvertRelativePathToFull(
        FPaths::ProjectSavedDir() /
        TEXT("Automation") /
        TEXT("PreprocessDataset") /
        FGuid::NewGuid().ToString(EGuidFormats::Digits));
}

static void DeleteTree(const FString& Root) {
    if (!Root.IsEmpty()) { IFileManager::Get().DeleteDirectory(*Root, false, true); }
}

static FRasterbandConfig MakeBand(const EGDALColorInterp Interp) {
    FRasterbandConfig Band;
    Band.color_interp = Interp;
    return Band;
}

static FPreprocessContext MakeContext(
    const FString& Root,
    const EAttachmentFormat Format = EAttachmentFormat::R32F,
    const uint32 TextureSize = 64) {
    FPreprocessContext Context{};

    Context.tile_dir = Root / TEXT("tiles");
    Context.temp_dir = Root / TEXT("temp");
    Context.fill_radius = 0.0f;
    Context.overwrite = true;
    Context.min_height = 0.0f;
    Context.max_height = 1.0f;
    Context.create_mask = false;
    Context.attachment_label = TEXT("test_attachment");
    Context.terrain_path = Root / TEXT("terrain");
    Context.no_data_value = -9999.0;

    Context.attachment.format = Format;
    Context.attachment.texture_size = TextureSize;

    return Context;
}

static FTileCoordinate MakeTestTileCoordinate() {
    // Adjust this one line if your FTileCoordinate constructor differs.
    return FTileCoordinate{0, 3, 5};
}
}

using namespace preprocess;
using namespace preprocess::tests;

static constexpr EAutomationTestFlags DatasetTestFlags =
    EAutomationTestFlags::ProductFilter |
    EAutomationTestFlags::CommandletContext |
    EAutomationTestFlags::ClientContext |
    EAutomationTestFlags::ServerContext;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCreateEmptyDatasetSetsRasterbandMetadataTest,
    "Project.Preprocess.Dataset.CreateEmptyDataset.SetsRasterbandMetadata",
    DatasetTestFlags)

bool FCreateEmptyDatasetSetsRasterbandMetadataTest::RunTest(const FString& Parameters) {
    GDALAllRegister();

    const FString Root = MakeUniqueTestRoot();
    ON_SCOPE_EXIT { DeleteTree(Root); };

    IFileManager::Get().MakeDirectory(*Root, true);

    FPreprocessContext Context = MakeContext(Root, EAttachmentFormat::Rgba8U, 128);
    Context.rasterbands = {
        MakeBand(EGDALColorInterp::GCI_RedBand),
        MakeBand(EGDALColorInterp::GCI_GreenBand),
        MakeBand(EGDALColorInterp::GCI_BlueBand),
        MakeBand(EGDALColorInterp::GCI_AlphaBand)
    };

    const FString Path = Root / TEXT("rgba8_tile.tif");
    const FUint64Point Size{128, 96};

    auto Result = create_empty_dataset<uint8>(Path, Size, NullOpt, Context);

    TestTrue(TEXT("create_empty_dataset should succeed"), Result.has_value());
    if (!Result.has_value()) { return false; }

    auto& Dataset = Result.value();

    TestTrue(TEXT("Dataset file should exist"), FPaths::FileExists(Path));
    TestEqual(TEXT("Width"), Dataset->GetRasterXSize(), static_cast<int32>(Size.X));
    TestEqual(TEXT("Height"), Dataset->GetRasterYSize(), static_cast<int32>(Size.Y));
    TestEqual(TEXT("Raster count"), Dataset->GetRasterCount(), 4);

    for (int32 BandIndex = 0; BandIndex < 4; ++BandIndex) {
        GDALRasterBand* Band = Dataset->GetRasterBand(BandIndex + 1);
        TestNotNull(*FString::Printf(TEXT("Band %d should exist"), BandIndex + 1), Band);
        if (Band == nullptr) { return false; }

        int bHasNoData = FALSE;
        const double NoData = Band->GetNoDataValue(&bHasNoData);

        TestTrue(
            *FString::Printf(TEXT("Band %d should have no-data"), BandIndex + 1),
            bHasNoData != FALSE);
        TestEqual(
            *FString::Printf(TEXT("Band %d no-data"), BandIndex + 1),
            NoData,
            *Context.no_data_value);

        const auto ExpectedInterp = static_cast<GDALColorInterp>(Context.rasterbands[BandIndex].
            color_interp);
        TestEqual(
            *FString::Printf(TEXT("Band %d color interp"), BandIndex + 1),
            Band->GetColorInterpretation(),
            ExpectedInterp);
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FGeoTransformRoundTripTest,
    "Project.Preprocess.Dataset.GeoTransform.RoundTrips",
    DatasetTestFlags)

bool FGeoTransformRoundTripTest::RunTest(const FString& Parameters) {
    GDALAllRegister();

    const FString Root = MakeUniqueTestRoot();
    ON_SCOPE_EXIT { DeleteTree(Root); };

    IFileManager::Get().MakeDirectory(*Root, true);

    FPreprocessContext Context = MakeContext(Root, EAttachmentFormat::R32F, 64);
    Context.rasterbands = {
        MakeBand(EGDALColorInterp::GCI_GrayIndex)
    };

    const FString Path = Root / TEXT("geotransform.tif");
    const auto Result = create_empty_dataset<float>(Path, FUint64Point{32, 16}, NullOpt, Context);

    TestTrue(TEXT("create_empty_dataset should succeed"), Result.has_value());
    if (!Result.has_value()) { return false; }

    const auto& Dataset = Result.value();

    double Expected[6] = {
        1000.0,
        2.0,
        0.0,
        2000.0,
        0.0,
        -2.0
    };

    const CPLErr SetErr = Dataset->SetGeoTransform(Expected);
    TestEqual(TEXT("Setting geotransform should succeed"), SetErr, CE_None);

    const GeoTransformRef ActualTransform = geo_transform(Dataset);
    const double* Actual = ActualTransform.Get();

    for (int32 I = 0; I < 6; ++I) {
        TestEqual(
            *FString::Printf(TEXT("GeoTransform[%d]"), I),
            Actual[I],
            Expected[I]);
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FCreateTileDatasetCreatesDirectoriesAndFileTest,
    "Project.Preprocess.Dataset.CreateTileDataset.CreatesDirectoriesAndFile",
    DatasetTestFlags)

bool FCreateTileDatasetCreatesDirectoriesAndFileTest::RunTest(const FString& Parameters) {
    GDALAllRegister();

    const FString Root = MakeUniqueTestRoot();
    ON_SCOPE_EXIT { DeleteTree(Root); };

    FPreprocessContext Context = MakeContext(Root, EAttachmentFormat::R32F, 256);
    Context.rasterbands = {
        MakeBand(EGDALColorInterp::GCI_GrayIndex)
    };

    const FTileCoordinate Tile = MakeTestTileCoordinate();
    const FString TilePath = Tile.path(Context.tile_dir);
    const FString ParentDir = FPaths::GetPath(TilePath);

    auto Result = create_tile_dataset<float>(Tile, Context);

    TestTrue(TEXT("create_tile_dataset should succeed"), Result.has_value());
    if (!Result.has_value()) { return false; }

    auto& Dataset = Result.value();

    TestTrue(
        TEXT("Parent directory should exist"),
        IFileManager::Get().DirectoryExists(*ParentDir));
    TestTrue(TEXT("Tile dataset should exist"), FPaths::FileExists(TilePath));
    TestEqual(
        TEXT("Tile width"),
        Dataset->GetRasterXSize(),
        Context.attachment.texture_size);
    TestEqual(
        TEXT("Tile height"),
        Dataset->GetRasterYSize(),
        Context.attachment.texture_size);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FUpdateTileDatasetOpensExistingFileTest,
    "Project.Preprocess.Dataset.UpdateTileDataset.OpensExistingFile",
    DatasetTestFlags)

bool FUpdateTileDatasetOpensExistingFileTest::RunTest(const FString& Parameters) {
    GDALAllRegister();

    const FString Root = MakeUniqueTestRoot();
    ON_SCOPE_EXIT { DeleteTree(Root); };

    FPreprocessContext Context = MakeContext(Root, EAttachmentFormat::R32F, 64);
    Context.rasterbands = {
        MakeBand(EGDALColorInterp::GCI_GrayIndex)
    };

    const FTileCoordinate Tile = MakeTestTileCoordinate();

    auto CreateResult = create_tile_dataset<float>(Tile, Context);
    TestTrue(TEXT("Tile should be created before update"), CreateResult.has_value());
    if (!CreateResult.has_value()) { return false; }

    auto UpdateResult = update_tile_dataset(Tile, Context);

    TestTrue(
        TEXT("update_tile_dataset should succeed for existing tile"),
        UpdateResult.has_value());
    if (!UpdateResult.has_value()) { return false; }

    auto& Dataset = UpdateResult.value();
    TestEqual(TEXT("Updated dataset raster count"), Dataset->GetRasterCount(), 1);
    TestEqual(TEXT("Updated dataset access mode"), Dataset->GetAccess(), GA_Update);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FUpdateTileDatasetFailsWhenMissingTest,
    "Project.Preprocess.Dataset.UpdateTileDataset.FailsWhenMissing",
    DatasetTestFlags)

bool FUpdateTileDatasetFailsWhenMissingTest::RunTest(const FString& Parameters) {
    GDALAllRegister();

    const FString Root = MakeUniqueTestRoot();
    ON_SCOPE_EXIT { DeleteTree(Root); };

    FPreprocessContext Context = MakeContext(Root, EAttachmentFormat::R32F, 64);
    Context.rasterbands = {
        MakeBand(EGDALColorInterp::GCI_GrayIndex)
    };

    const FTileCoordinate MissingTile = MakeTestTileCoordinate();

    const auto Result = update_tile_dataset(MissingTile, Context);

    TestFalse(TEXT("update_tile_dataset should fail when tile does not exist"), Result.has_value());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FLoadTileDatasetIfExistsReturnsNullOptWhenMissingTest,
    "Project.Preprocess.Dataset.LoadTileDatasetIfExists.ReturnsNullOptWhenMissing",
    DatasetTestFlags)

bool FLoadTileDatasetIfExistsReturnsNullOptWhenMissingTest::RunTest(const FString& Parameters) {
    GDALAllRegister();

    const FString Root = MakeUniqueTestRoot();
    ON_SCOPE_EXIT { DeleteTree(Root); };

    FPreprocessContext Context = MakeContext(Root, EAttachmentFormat::R32F, 64);
    Context.rasterbands = {
        MakeBand(EGDALColorInterp::GCI_GrayIndex)
    };

    const FTileCoordinate MissingTile = MakeTestTileCoordinate();

    const auto Result = load_tile_dataset_if_exists(MissingTile, Context);

    TestTrue(TEXT("Missing tile query should still succeed"), Result.has_value());
    if (!Result.has_value()) { return false; }

    TestFalse(TEXT("Missing tile should return NullOpt"), Result.value().IsSet());
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FLoadTileDatasetIfExistsLoadsExistingDatasetTest,
    "Project.Preprocess.Dataset.LoadTileDatasetIfExists.LoadsExistingDataset",
    DatasetTestFlags)

bool FLoadTileDatasetIfExistsLoadsExistingDatasetTest::RunTest(const FString& Parameters) {
    GDALAllRegister();

    const FString Root = MakeUniqueTestRoot();
    ON_SCOPE_EXIT { DeleteTree(Root); };

    FPreprocessContext Context = MakeContext(Root, EAttachmentFormat::R32F, 64);
    Context.rasterbands = {
        MakeBand(EGDALColorInterp::GCI_GrayIndex)
    };

    const FTileCoordinate Tile = MakeTestTileCoordinate();

    auto CreateResult = create_tile_dataset<float>(Tile, Context);
    TestTrue(TEXT("Tile should be created before load"), CreateResult.has_value());
    if (!CreateResult.has_value()) { return false; }

    auto LoadResult = load_tile_dataset_if_exists(Tile, Context);

    TestTrue(TEXT("Loading existing tile should succeed"), LoadResult.has_value());
    if (!LoadResult.has_value()) { return false; }

    TestTrue(TEXT("Existing tile should return a dataset"), LoadResult.value().IsSet());
    if (!LoadResult.value().IsSet()) { return false; }

    auto& Dataset = LoadResult.value().GetValue();
    TestEqual(TEXT("Loaded dataset raster count"), Dataset->GetRasterCount(), 1);
    TestEqual(
        TEXT("Loaded dataset width"),
        Dataset->GetRasterXSize(),
        Context.attachment.texture_size);
    TestEqual(
        TEXT("Loaded dataset height"),
        Dataset->GetRasterYSize(),
        Context.attachment.texture_size);

    return true;
}

#endif // WITH_DEV_AUTOMATION_TESTS
