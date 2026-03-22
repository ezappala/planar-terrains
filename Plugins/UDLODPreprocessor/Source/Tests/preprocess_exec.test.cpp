#if WITH_DEV_AUTOMATION_TESTS

#include "tests_shared.h"

#include "preprocess_exec.h"
#include "preprocess_split.h"
#include "terrain_settings.h"

using namespace preprocess;
using namespace preprocess::tests;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPreprocessExecResolveNoDataValueTest,
    "UDLODPreprocessor.Preprocess.Exec.ResolveNoDataValue",
    TestFlags)

bool FPreprocessExecResolveNoDataValueTest::RunTest(const FString& Parameters) {
    const GDALDatasetRef dataset = create_memory_dataset<float>(4, 4);
    GDALRasterBand* band = dataset->GetRasterBand(1);
    const CPLErr set_no_data_result = band->SetNoDataValue(-123.5);

    TestEqual(TEXT("Setting source no-data succeeds"), set_no_data_result, CE_None);

    TArray source_bands{make_band(EGDALColorInterp::GCI_GrayIndex)};
    const TOptional<double> source_value = resolve_no_data_value(
        dataset,
        source_bands,
        FPreprocessNoData::Source());
    TestTrue(TEXT("Source no-data is resolved"), source_value.IsSet());
    if (!source_value.IsSet()) { return false; }
    TestEqual(TEXT("Source no-data value"), source_value.GetValue(), -123.5);
    TestEqual(TEXT("Source rasterband count unchanged"), source_bands.Num(), 1);

    TArray explicit_bands{make_band(EGDALColorInterp::GCI_GrayIndex)};
    const TOptional<double> explicit_value = resolve_no_data_value(
        dataset,
        explicit_bands,
        FPreprocessNoData::NoData(42.0));
    TestTrue(TEXT("Explicit no-data is returned"), explicit_value.IsSet());
    if (!explicit_value.IsSet()) { return false; }
    TestEqual(TEXT("Explicit no-data value"), explicit_value.GetValue(), 42.0);
    TestEqual(TEXT("Explicit rasterband count unchanged"), explicit_bands.Num(), 1);

    TArray alpha_bands{make_band(EGDALColorInterp::GCI_RedBand)};
    const TOptional<double> alpha_value = resolve_no_data_value(
        dataset,
        alpha_bands,
        FPreprocessNoData::Alpha());
    TestFalse(TEXT("Alpha mode clears numeric no-data"), alpha_value.IsSet());
    TestEqual(TEXT("Alpha rasterband appended"), alpha_bands.Num(), 2);
    TestEqual(
        TEXT("Alpha rasterband interpretation"),
        static_cast<int32>(alpha_bands.Last().color_interp),
        static_cast<int32>(EGDALColorInterp::GCI_AlphaBand));

    const GDALDatasetRef dataset_without_no_data = create_memory_dataset<float>(2, 2);
    TArray source_without_no_data_bands{
        make_band(EGDALColorInterp::GCI_GrayIndex)
    };
    const TOptional<double> missing_source_value = resolve_no_data_value(
        dataset_without_no_data,
        source_without_no_data_bands,
        FPreprocessNoData::Source());
    TestFalse(
        TEXT("Source mode stays unset when source has no no-data"),
        missing_source_value.IsSet());
    TestEqual(
        TEXT("Missing source no-data does not append rasterbands"),
        source_without_no_data_bands.Num(),
        1);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPreprocessExecInitializeTest,
    "UDLODPreprocessor.Preprocess.Exec.Initialize",
    TestFlags)

bool FPreprocessExecInitializeTest::RunTest(const FString& Parameters) {
    GDALAllRegister();

    const FString root = make_unique_test_root(TEXT("ExecInitialize"));
    ON_SCOPE_EXIT { delete_tree(root); };

    const FString heightmap_path = FPaths::Combine(root, TEXT("height.tif"));
    const FString albedo_path = FPaths::Combine(root, TEXT("albedo.tif"));

    {
        GDALDatasetRef heightmap = create_tiff_dataset<float>(heightmap_path, 4, 4, 1);
        GDALRasterBand* band = heightmap->GetRasterBand(1);
        TestEqual(TEXT("Heightmap no-data set"), band->SetNoDataValue(-7.5), CE_None);
        TestEqual(
            TEXT("Heightmap color interpretation set"),
            band->SetColorInterpretation(GCI_GrayIndex),
            CE_None);
        heightmap->FlushCache();
    }

    {
        GDALDatasetRef albedo = create_tiff_dataset<uint8>(albedo_path, 4, 4, 3);
        TestEqual(
            TEXT("Albedo band 1 interp"),
            albedo->GetRasterBand(1)->SetColorInterpretation(GCI_RedBand),
            CE_None);
        TestEqual(
            TEXT("Albedo band 2 interp"),
            albedo->GetRasterBand(2)->SetColorInterpretation(GCI_GreenBand),
            CE_None);
        TestEqual(
            TEXT("Albedo band 3 interp"),
            albedo->GetRasterBand(3)->SetColorInterpretation(GCI_BlueBand),
            CE_None);
        albedo->FlushCache();
    }

    FTerrainPreprocessSettings settings{};
    settings.heightmap_src_path = heightmap_path;
    settings.albedo_src_path = albedo_path;
    settings.terrain_path = FPaths::Combine(root, TEXT("terrain"));
    settings.temp_path = FPaths::Combine(root, TEXT("temp"));
    settings.overwrite = true;
    settings.fill_radius = 3.0f;
    settings.heightmap_no_data = FPreprocessNoData::Source();
    settings.albedo_no_data = FPreprocessNoData::Alpha();
    settings.heightmap_data_type = FPreprocessDataType::DataType(
        static_cast<EGDALDataType>(GDT_Float32));
    settings.albedo_data_type = FPreprocessDataType::DataType(static_cast<EGDALDataType>(GDT_Byte));
    settings.heightmap_create_mask = false;
    settings.albedo_create_mask = true;
    settings.heightmap_lod_count = 5;
    settings.albedo_lod_count = 3;
    settings.heightmap_attachment_label = TEXT("height");
    settings.albedo_attachment_label = TEXT("albedo");
    settings.texture_size = 32;
    settings.border_size = 2;
    settings.mip_level_count = 2;
    settings.heightmap_attachment_format = EAttachmentFormat::R32F;
    settings.albedo_attachment_format = EAttachmentFormat::Rgba8U;

    const auto initialize_result = initialize(settings);
    TestTrue(TEXT("Initialization succeeds"), initialize_result.has_value());
    if (!initialize_result.has_value()) {
        return false;
    }

    auto& initialize_value = initialize_result.value();
    auto& heightmap_init = initialize_value.Get<0>();
    auto& albedo_init = initialize_value.Get<1>();
    auto& heightmap_dataset = heightmap_init.Get<0>();
    auto& heightmap_context = heightmap_init.Get<1>();
    auto& albedo_dataset = albedo_init.Get<0>();
    auto& albedo_context = albedo_init.Get<1>();

    TestNotNull(TEXT("Heightmap dataset opens"), heightmap_dataset.Get());
    TestNotNull(TEXT("Albedo dataset opens"), albedo_dataset.Get());
    TestEqual(
        TEXT("Heightmap dataset access is read-only"),
        heightmap_dataset->GetAccess(),
        GA_ReadOnly);
    TestEqual(TEXT("Albedo dataset access is read-only"), albedo_dataset->GetAccess(), GA_ReadOnly);
    TestTrue(TEXT("Heightmap source no-data preserved"), heightmap_context.no_data_value.IsSet());
    if (!heightmap_context.no_data_value.IsSet()) { return false; }
    TestEqual(
        TEXT("Heightmap source no-data value"),
        heightmap_context.no_data_value.GetValue(),
        -7.5);
    TestFalse(
        TEXT("Albedo alpha mode clears numeric no-data"),
        albedo_context.no_data_value.IsSet());
    TestEqual(TEXT("Albedo rasterbands include alpha"), albedo_context.rasterbands.Num(), 4);
    TestEqual(
        TEXT("Appended alpha interpretation"),
        static_cast<int32>(albedo_context.rasterbands.Last().color_interp),
        static_cast<int32>(EGDALColorInterp::GCI_AlphaBand));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPreprocessExecInitializeRejectsMissingSourcesTest,
    "UDLODPreprocessor.Preprocess.Exec.InitializeRejectsMissingSources",
    TestFlags)

bool FPreprocessExecInitializeRejectsMissingSourcesTest::RunTest(const FString& Parameters) {
    FTerrainPreprocessSettings settings{};
    settings.heightmap_src_path = TEXT("Z:/definitely-missing-heightmap.tif");
    settings.albedo_src_path = TEXT("Z:/definitely-missing-albedo.tif");

    const auto result = initialize(settings);
    TestFalse(TEXT("Initialization rejects missing datasets"), result.has_value());
    if (result.has_value()) {
        return false;
    }

    TestEqual(
        TEXT("Initialization reports parse error for missing sources"),
        result.error().Type,
        EPreprocessError::Parse);
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPreprocessSplitClipsPartialEdgeTileWindowsTest,
    "UDLODPreprocessor.Preprocess.Split.ClipsPartialEdgeTileWindows",
    TestFlags)

bool FPreprocessSplitClipsPartialEdgeTileWindowsTest::RunTest(const FString& Parameters) {
    GDALAllRegister();

    const FString root = make_unique_test_root(TEXT("SplitClipsPartialEdgeTileWindows"));
    ON_SCOPE_EXIT { delete_tree(root); };

    FPreprocessContext context = make_context(root, EAttachmentFormat::R32F, 14, 2);
    const FString source_path = FPaths::Combine(root, TEXT("source.tif"));

    {
        GDALDatasetRef source = create_tiff_dataset<float>(source_path, 100, 55, 1);
        auto* band = source->GetRasterBand(1);
        TestEqual(TEXT("Source no-data set"), band->SetNoDataValue(-9999.0), CE_None);

        TArray<float> values;
        values.Init(1.0f, 100 * 55);
        ext::Buffer buffer{MoveTemp(values), usize_c{100, 55}};
        const auto write_result = write<float>(band, isize_c{0, 0}, usize_c{100, 55}, buffer);
        TestTrue(TEXT("Source dataset write succeeds"), write_result.has_value());
        if (!write_result.has_value()) {
            return false;
        }
        source->FlushCache();
    }

    TMap<uint32, FaceInfo> faces;
    faces.Add(
        0u,
        FaceInfo{
            .lod = 0u,
            .pixel_start = {0, 0},
            .pixel_end = {100, 55},
            .path = source_path
        });

    TMap<uint32, SharedDatasetRO> datasets;
    datasets.Add(0u, SharedDatasetRO{source_path});

    const FTileCoordinate edge_tile{0u, 0u, FIntPoint{2, 5}};
    const auto split_result = split<float>({edge_tile}, faces, datasets, context);
    TestTrue(TEXT("Split succeeds for partial edge tiles"), split_result.has_value());
    if (!split_result.has_value()) {
        return false;
    }

    TestEqual(TEXT("Exactly one tile is emitted"), split_result.value().Num(), 1);
    if (split_result.value().Num() != 1) {
        return false;
    }

    TestTrue(TEXT("Expected edge tile is emitted"), split_result.value()[0] == edge_tile);

    const FString tile_path = edge_tile.path(context.tile_dir);
    TestTrue(TEXT("Edge tile dataset is created"), FPaths::FileExists(tile_path));

    auto tile_result = load_tile_dataset_if_exists(edge_tile, context);
    TestTrue(TEXT("Created edge tile can be reopened"), tile_result.has_value());
    if (!tile_result.has_value() || !tile_result.value().IsSet()) {
        return false;
    }

    const GDALDatasetRef& tile_dataset = tile_result.value().GetValue();
    const auto read_result = read_as<float>(
        tile_dataset->GetRasterBand(1),
        isize_c{2, 2},
        usize_c{10, 5},
        usize_c{10, 5});
    TestTrue(TEXT("Only the overlapping rows are readable from the copied window"), read_result.has_value());

    return read_result.has_value();
}

#endif
