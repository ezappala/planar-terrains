#if WITH_DEV_AUTOMATION_TESTS

#include "tests_shared.h"

#include "preprocess_exec.h"
#include "preprocess_fill.h"
#include "preprocess_reproject.h"
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
    FPreprocessExecInitializeUsesSourceDataTypesAndAttachmentTempDirsTest,
    "UDLODPreprocessor.Preprocess.Exec.InitializeUsesSourceDataTypesAndAttachmentTempDirs",
    TestFlags)

bool FPreprocessExecInitializeUsesSourceDataTypesAndAttachmentTempDirsTest::RunTest(
    const FString& Parameters) {
    GDALAllRegister();

    const FString root = make_unique_test_root(TEXT("ExecInitializeSourceDataTypes"));
    ON_SCOPE_EXIT { delete_tree(root); };

    const FString heightmap_path = FPaths::Combine(root, TEXT("height_u16.tif"));
    const FString albedo_path = FPaths::Combine(root, TEXT("albedo_u8.tif"));

    {
        GDALDatasetRef heightmap = create_tiff_dataset<uint16>(heightmap_path, 4, 4, 1);
        TestEqual(
            TEXT("Heightmap band color interpretation set"),
            heightmap->GetRasterBand(1)->SetColorInterpretation(GCI_GrayIndex),
            CE_None);
        heightmap->FlushCache();
    }

    {
        GDALDatasetRef albedo = create_tiff_dataset<uint8>(albedo_path, 4, 4, 3);
        TestEqual(TEXT("Albedo band 1 interp"), albedo->GetRasterBand(1)->SetColorInterpretation(GCI_RedBand), CE_None);
        TestEqual(TEXT("Albedo band 2 interp"), albedo->GetRasterBand(2)->SetColorInterpretation(GCI_GreenBand), CE_None);
        TestEqual(TEXT("Albedo band 3 interp"), albedo->GetRasterBand(3)->SetColorInterpretation(GCI_BlueBand), CE_None);
        albedo->FlushCache();
    }

    FTerrainPreprocessSettings settings{};
    settings.heightmap_src_path = heightmap_path;
    settings.albedo_src_path = albedo_path;
    settings.terrain_path = FPaths::Combine(root, TEXT("terrain"));
    settings.temp_path = FPaths::Combine(root, TEXT("tmp"));
    settings.heightmap_data_type = FPreprocessDataType::Source();
    settings.albedo_data_type = FPreprocessDataType::Source();
    settings.heightmap_attachment_label = TEXT("height");
    settings.albedo_attachment_label = TEXT("albedo");

    const auto initialize_result = initialize(settings);
    TestTrue(TEXT("Initialization succeeds"), initialize_result.has_value());
    if (!initialize_result.has_value()) { return false; }

    auto& initialize_value = initialize_result.value();
    auto& heightmap_context = initialize_value.Get<0>().Get<1>();
    auto& albedo_context = initialize_value.Get<1>().Get<1>();

    TestEqual(
        TEXT("Heightmap source datatype is preserved"),
        static_cast<int32>(heightmap_context.data_type),
        static_cast<int32>(GDT_UInt16));
    TestEqual(
        TEXT("Albedo source datatype is preserved"),
        static_cast<int32>(albedo_context.data_type),
        static_cast<int32>(GDT_Byte));
    TestEqual(
        TEXT("Heightmap temp dir is attachment-specific"),
        heightmap_context.temp_dir,
        FPaths::Combine(settings.temp_path, settings.heightmap_attachment_label));
    TestEqual(
        TEXT("Albedo temp dir is attachment-specific"),
        albedo_context.temp_dir,
        FPaths::Combine(settings.temp_path, settings.albedo_attachment_label));

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

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPreprocessSplitCopiesRgbaTileDataTest,
    "UDLODPreprocessor.Preprocess.Split.CopiesRgbaTileData",
    TestFlags)

bool FPreprocessSplitCopiesRgbaTileDataTest::RunTest(const FString& Parameters) {
    GDALAllRegister();

    const FString root = make_unique_test_root(TEXT("SplitCopiesRgbaTileData"));
    ON_SCOPE_EXIT { delete_tree(root); };

    FPreprocessContext context = make_context(
        root,
        EAttachmentFormat::Rgba8U,
        8,
        1,
        static_cast<EGDALDataType>(GDT_Byte));
    context.no_data_value = 0.0;
    context.rasterbands = {
        make_band(EGDALColorInterp::GCI_RedBand),
        make_band(EGDALColorInterp::GCI_GreenBand),
        make_band(EGDALColorInterp::GCI_BlueBand),
        make_band(EGDALColorInterp::GCI_AlphaBand)
    };
    context.attachment_label = TEXT("albedo");

    const FString source_path = FPaths::Combine(root, TEXT("source_rgba.tif"));
    {
        GDALDatasetRef source = create_tiff_dataset<uint8>(source_path, 6, 6, 4);
        const TArray<int32> band_values{11, 22, 33, 44};
        for (int32 band_index = 0; band_index < band_values.Num(); ++band_index) {
            TArray<uint8> values;
            values.Init(static_cast<uint8>(band_values[band_index]), 36);
            ext::Buffer<uint8> buffer{MoveTemp(values), usize_c{6, 6}};
            auto* band = source->GetRasterBand(band_index + 1);
            TestTrue(
                *FString::Printf(TEXT("Band %d write succeeds"), band_index + 1),
                write<uint8>(band, {0, 0}, {6, 6}, buffer).has_value());
        }
        TestEqual(TEXT("Band 1 interp"), source->GetRasterBand(1)->SetColorInterpretation(GCI_RedBand), CE_None);
        TestEqual(TEXT("Band 2 interp"), source->GetRasterBand(2)->SetColorInterpretation(GCI_GreenBand), CE_None);
        TestEqual(TEXT("Band 3 interp"), source->GetRasterBand(3)->SetColorInterpretation(GCI_BlueBand), CE_None);
        TestEqual(TEXT("Band 4 interp"), source->GetRasterBand(4)->SetColorInterpretation(GCI_AlphaBand), CE_None);
        source->FlushCache();
    }

    TMap<uint32, FaceInfo> faces;
    faces.Add(
        0u,
        FaceInfo{
            .lod = 0u,
            .pixel_start = {0, 0},
            .pixel_end = {6, 6},
            .path = source_path
        });

    TMap<uint32, SharedDatasetRO> datasets;
    datasets.Add(0u, SharedDatasetRO{source_path});

    const FTileCoordinate tile{0u, 0u, FIntPoint{0, 0}};
    const auto split_result = split<uint8>({tile}, faces, datasets, context);
    TestTrue(TEXT("RGBA split succeeds"), split_result.has_value());
    if (!split_result.has_value()) { return false; }

    auto tile_result = load_tile_dataset_if_exists(tile, context);
    TestTrue(TEXT("RGBA tile exists"), tile_result.has_value());
    if (!tile_result.has_value() || !tile_result.value().IsSet()) { return false; }

    const GDALDatasetRef& tile_dataset = tile_result.value().GetValue();
    const TArray<int32> expected_band_values{11, 22, 33, 44};
    for (int32 band_index = 0; band_index < expected_band_values.Num(); ++band_index) {
        auto read_result = read_as<uint8>(
            tile_dataset->GetRasterBand(band_index + 1),
            {1, 1},
            {6, 6},
            {6, 6});
        TestTrue(
            *FString::Printf(TEXT("Band %d center read succeeds"), band_index + 1),
            read_result.has_value());
        if (!read_result.has_value()) { return false; }

        for (uint8 value : read_result.value().data()) {
            TestEqual(
                *FString::Printf(TEXT("Band %d copied value"), band_index + 1),
                static_cast<int32>(value),
                expected_band_values[band_index]);
        }
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPreprocessFillNoDataPreservesRgbaCenterTest,
    "UDLODPreprocessor.Preprocess.Fill.NoDataPreservesRgbaCenter",
    TestFlags)

bool FPreprocessFillNoDataPreservesRgbaCenterTest::RunTest(const FString& Parameters) {
    GDALAllRegister();

    const FString root = make_unique_test_root(TEXT("FillNoDataPreservesRgbaCenter"));
    ON_SCOPE_EXIT { delete_tree(root); };

    FPreprocessContext context = make_context(
        root,
        EAttachmentFormat::Rgba8U,
        8,
        1,
        static_cast<EGDALDataType>(GDT_Byte));
    context.no_data_value = 0.0;
    context.rasterbands = {
        make_band(EGDALColorInterp::GCI_RedBand),
        make_band(EGDALColorInterp::GCI_GreenBand),
        make_band(EGDALColorInterp::GCI_BlueBand),
        make_band(EGDALColorInterp::GCI_AlphaBand)
    };
    context.attachment_label = TEXT("albedo");
    context.fill_radius = 4.0f;

    const FTileCoordinate tile{0u, 0u, FIntPoint{0, 0}};
    auto create_result = create_tile_dataset<uint8>(tile, context);
    TestTrue(TEXT("RGBA tile is created"), create_result.has_value());
    if (!create_result.has_value()) { return false; }

    GDALDatasetRef dataset = MoveTemp(create_result.value());
    const TArray<int32> band_values{11, 22, 33, 44};
    for (int32 band_index = 0; band_index < band_values.Num(); ++band_index) {
        TArray<uint8> values;
        values.Init(static_cast<uint8>(band_values[band_index]), 36);
        ext::Buffer<uint8> buffer{MoveTemp(values), usize_c{6, 6}};
        TestTrue(
            *FString::Printf(TEXT("Band %d center write succeeds"), band_index + 1),
            write<uint8>(dataset->GetRasterBand(band_index + 1), {1, 1}, {6, 6}, buffer).has_value());
    }
    dataset->FlushCache();

    const auto fill_result = create_mask_and_fill_no_data({tile}, context);
    TestTrue(TEXT("RGBA fill succeeds"), fill_result.has_value());
    if (!fill_result.has_value()) { return false; }

    auto tile_result = load_tile_dataset_if_exists(tile, context);
    TestTrue(TEXT("RGBA tile can be reopened"), tile_result.has_value());
    if (!tile_result.has_value() || !tile_result.value().IsSet()) { return false; }

    const GDALDatasetRef& filled_dataset = tile_result.value().GetValue();
    const TArray<int32> expected_band_values{11, 22, 33, 44};
    for (int32 band_index = 0; band_index < expected_band_values.Num(); ++band_index) {
        auto read_result = read_as<uint8>(
            filled_dataset->GetRasterBand(band_index + 1),
            {1, 1},
            {6, 6},
            {6, 6});
        TestTrue(
            *FString::Printf(TEXT("Band %d center read succeeds after fill"), band_index + 1),
            read_result.has_value());
        if (!read_result.has_value()) { return false; }

        for (uint8 value : read_result.value().data()) {
            TestEqual(
                *FString::Printf(TEXT("Band %d center value preserved"), band_index + 1),
                static_cast<int32>(value),
                expected_band_values[band_index]);
        }
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPreprocessReprojectPlanarUpdatesLodCountTest,
    "UDLODPreprocessor.Preprocess.Reproject.PlanarUpdatesLodCount",
    TestFlags)

bool FPreprocessReprojectPlanarUpdatesLodCountTest::RunTest(const FString& Parameters) {
    GDALAllRegister();

    const FString root = make_unique_test_root(TEXT("ReprojectPlanarUpdatesLodCount"));
    ON_SCOPE_EXIT { delete_tree(root); };

    FPreprocessContext context = make_context(root, EAttachmentFormat::R32F, 512, 2);
    context.temp_dir = FPaths::Combine(root, TEXT("temp"));
    IFileManager::Get().MakeDirectory(*context.temp_dir, true);

    auto source = create_memory_dataset<float>(2048, 1024, 1);
    double source_geotransform[6]{0.0, 1.0, 0.0, 0.0, 0.0, -1.0};
    TestEqual(
        TEXT("Source geotransform is configured"),
        source->SetGeoTransform(source_geotransform),
        CE_None);
    auto result = reproject_planar<float>(MoveTemp(source), context);
    TestTrue(TEXT("Planar reproject succeeds"), result.has_value());
    if (!result.has_value()) { return false; }

    TestTrue(TEXT("LOD count is populated"), context.lod_count.IsSet());
    if (!context.lod_count.IsSet()) { return false; }

    TestEqual(TEXT("LOD count matches planar dataset dimensions"), context.lod_count.GetValue(), 4);
    return true;
}

#endif
