#if WITH_DEV_AUTOMATION_TESTS

#include "tests_shared.h"

#include "preprocess_downsample.h"

using namespace preprocess;
using namespace preprocess::tests;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPreprocessDownsampleComputeTilesToDownsampleTest,
    "UDLODPreprocessor.Preprocess.Downsample.ComputeTilesToDownsample",
    TestFlags)

bool FPreprocessDownsampleComputeTilesToDownsampleTest::RunTest(const FString& Parameters) {
    const TArray input_tiles{
        FTileCoordinate{0u, 2u, FIntPoint{0, 0}},
        FTileCoordinate{0u, 2u, FIntPoint{1, 0}},
        FTileCoordinate{0u, 3u, FIntPoint{4, 4}}
    };

    const TArray<TArray<FTileCoordinate>> groups = compute_tiles_to_downsample(input_tiles);

    TestEqual(TEXT("LOD groups"), groups.Num(), 3);
    if (groups.Num() != 3) { return false; }

    TestEqual(TEXT("LOD 2 tile count"), groups[0].Num(), 1);
    TestTrue(
        TEXT("LOD 2 parent exists"),
        groups[0].Contains(FTileCoordinate{0u, 2u, FIntPoint{2, 2}}));

    TestEqual(TEXT("LOD 1 tile count"), groups[1].Num(), 2);
    TestTrue(
        TEXT("LOD 1 contains first ancestor"),
        groups[1].Contains(FTileCoordinate{0u, 1u, FIntPoint{0, 0}}));
    TestTrue(
        TEXT("LOD 1 contains second ancestor"),
        groups[1].Contains(FTileCoordinate{0u, 1u, FIntPoint{1, 1}}));

    TestEqual(TEXT("LOD 0 tile count"), groups[2].Num(), 1);
    TestTrue(
        TEXT("LOD 0 contains root ancestor"),
        groups[2].Contains(FTileCoordinate{0u, 0u, FIntPoint{0, 0}}));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPreprocessDownsamplePreservesChildQuadrantsTest,
    "UDLODPreprocessor.Preprocess.Downsample.PreservesChildQuadrants",
    TestFlags)

bool FPreprocessDownsamplePreservesChildQuadrantsTest::RunTest(const FString& Parameters) {
    GDALAllRegister();

    const FString root = make_unique_test_root(TEXT("DownsamplePreservesChildQuadrants"));
    ON_SCOPE_EXIT { delete_tree(root); };

    FPreprocessContext context = make_context(root, EAttachmentFormat::R32F, 8, 1);
    const isize_c border_offset{1, 1};
    const usize_c tile_size{6, 6};

    const TArray<TPair<FTileCoordinate, float>> children{
        TPair<FTileCoordinate, float>{FTileCoordinate{0u, 1u, FIntPoint{0, 0}}, 10.0f},
        TPair<FTileCoordinate, float>{FTileCoordinate{0u, 1u, FIntPoint{1, 0}}, 20.0f},
        TPair<FTileCoordinate, float>{FTileCoordinate{0u, 1u, FIntPoint{0, 1}}, 30.0f},
        TPair<FTileCoordinate, float>{FTileCoordinate{0u, 1u, FIntPoint{1, 1}}, 40.0f}
    };

    for (const auto& [child, value] : children) {
        auto create_result = create_tile_dataset<float>(child, context);
        TestTrue(TEXT("Child tile is created"), create_result.has_value());
        if (!create_result.has_value()) { return false; }

        GDALDatasetRef dataset = MoveTemp(create_result.value());
        TArray<float> child_values;
        child_values.Init(value, tile_size.Get<0>() * tile_size.Get<1>());
        ext::Buffer child_buffer{MoveTemp(child_values), tile_size};
        const auto write_result = write<float>(
            dataset->GetRasterBand(1),
            border_offset,
            tile_size,
            child_buffer);
        TestTrue(TEXT("Child center write succeeds"), write_result.has_value());
        if (!write_result.has_value()) { return false; }
        dataset->FlushCache();
    }

    const FTileCoordinate parent{0u, 0u, FIntPoint{0, 0}};
    const auto downsample_result = downsample<float>({parent}, context);
    TestTrue(TEXT("Downsample succeeds"), downsample_result.has_value());
    if (!downsample_result.has_value()) { return false; }

    auto parent_dataset_result = load_tile_dataset_if_exists(parent, context);
    TestTrue(TEXT("Parent tile exists"), parent_dataset_result.has_value());
    if (!parent_dataset_result.has_value() || !parent_dataset_result.value().IsSet()) {
        return false;
    }

    const GDALDatasetRef& parent_dataset = parent_dataset_result.value().GetValue();
    auto read_result = read_as<float>(
        parent_dataset->GetRasterBand(1),
        border_offset,
        tile_size,
        tile_size);
    TestTrue(TEXT("Parent center read succeeds"), read_result.has_value());
    if (!read_result.has_value()) { return false; }

    const TArray<float>& values = read_result.value().data();
    for (int32 y = 0; y < 6; ++y) {
        for (int32 x = 0; x < 6; ++x) {
            const float expected =
                y < 3
                ? (x < 3 ? 10.0f : 20.0f)
                : (x < 3 ? 30.0f : 40.0f);
            TestEqual(
                *FString::Printf(TEXT("Parent value at (%d,%d)"), x, y),
                values[y * 6 + x],
                expected);
        }
    }

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPreprocessDownsamplePreservesChildOrientationTest,
    "UDLODPreprocessor.Preprocess.Downsample.PreservesChildOrientation",
    TestFlags)

bool FPreprocessDownsamplePreservesChildOrientationTest::RunTest(const FString& Parameters) {
    GDALAllRegister();

    const FString root = make_unique_test_root(TEXT("DownsamplePreservesChildOrientation"));
    ON_SCOPE_EXIT { delete_tree(root); };

    FPreprocessContext context = make_context(root, EAttachmentFormat::R32F, 10, 1);
    const isize_c border_offset{1, 1};
    const usize_c child_tile_size{8, 8};
    const usize_c parent_child_size{4, 4};

    const FTileCoordinate child{0u, 1u, FIntPoint{0, 0}};
    auto create_result = create_tile_dataset<float>(child, context);
    TestTrue(TEXT("Child tile is created"), create_result.has_value());
    if (!create_result.has_value()) { return false; }

    GDALDatasetRef child_dataset = MoveTemp(create_result.value());
    TArray<float> child_values;
    child_values.Reserve(64);
    for (int32 block_y = 0; block_y < 4; ++block_y) {
        for (int32 row = 0; row < 2; ++row) {
            for (int32 block_x = 0; block_x < 4; ++block_x) {
                const float value = static_cast<float>(block_y * 4 + block_x + 1);
                child_values.Add(value);
                child_values.Add(value);
            }
        }
    }
    ext::Buffer<float> child_buffer{MoveTemp(child_values), child_tile_size};
    TestTrue(
        TEXT("Child orientation pattern write succeeds"),
        write<float>(child_dataset->GetRasterBand(1), border_offset, child_tile_size, child_buffer).has_value());
    child_dataset->FlushCache();

    const FTileCoordinate parent{0u, 0u, FIntPoint{0, 0}};
    const auto downsample_result = downsample<float>({parent}, context);
    TestTrue(TEXT("Downsample succeeds"), downsample_result.has_value());
    if (!downsample_result.has_value()) { return false; }

    auto parent_dataset_result = load_tile_dataset_if_exists(parent, context);
    TestTrue(TEXT("Parent tile exists"), parent_dataset_result.has_value());
    if (!parent_dataset_result.has_value() || !parent_dataset_result.value().IsSet()) {
        return false;
    }

    const GDALDatasetRef& parent_dataset = parent_dataset_result.value().GetValue();
    auto read_result = read_as<float>(
        parent_dataset->GetRasterBand(1),
        border_offset,
        parent_child_size,
        parent_child_size);
    TestTrue(TEXT("Parent top-left quadrant read succeeds"), read_result.has_value());
    if (!read_result.has_value()) { return false; }

    const TArray<float>& values = read_result.value().data();
    for (int32 y = 0; y < 4; ++y) {
        for (int32 x = 0; x < 4; ++x) {
            const float expected = static_cast<float>(y * 4 + x + 1);
            TestEqual(
                *FString::Printf(TEXT("Parent orientation value at (%d,%d)"), x, y),
                values[y * 4 + x],
                expected);
        }
    }

    return true;
}

#endif
