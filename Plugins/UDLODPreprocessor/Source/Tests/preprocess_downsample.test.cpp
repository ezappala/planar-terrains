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

#endif
