#if WITH_DEV_AUTOMATION_TESTS

#include "tests_shared.h"

#include "preprocess_tile_coordinate.h"

using namespace preprocess::tests;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPreprocessTileCoordinateHierarchyAndPathTest,
    "UDLODPreprocessor.Preprocess.TileCoordinate.HierarchyAndPath",
    TestFlags)

bool FPreprocessTileCoordinateHierarchyAndPathTest::RunTest(const FString& Parameters) {
    const FTileCoordinate tile{1u, 3u, FIntPoint{9, 10}};

    TestEqual(TEXT("Tile string"), tile.to_string(), FString(TEXT("1_3_9_10")));
    TestEqual(
        TEXT("Tile path"),
        tile.path(TEXT("tiles/")),
        FString(TEXT("tiles/3/1_1/1_3_9_10.tif")));

    const auto children = tile.children();
    TestEqual(TEXT("Child count"), children.Num(), 4);
    if (children.Num() != 4) { return false; }

    TestTrue(TEXT("Child 0"), children[0] == FTileCoordinate(1u, 4u, FIntPoint{18, 20}));
    TestTrue(TEXT("Child 1"), children[1] == FTileCoordinate(1u, 4u, FIntPoint{19, 20}));
    TestTrue(TEXT("Child 2"), children[2] == FTileCoordinate(1u, 4u, FIntPoint{18, 21}));
    TestTrue(TEXT("Child 3"), children[3] == FTileCoordinate(1u, 4u, FIntPoint{19, 21}));

    const auto parent = tile.parent();
    TestTrue(TEXT("Parent exists"), parent.IsSet());
    if (!parent.IsSet()) { return false; }
    TestTrue(
        TEXT("Parent coordinate"),
        parent.GetValue() == FTileCoordinate(1u, 2u, FIntPoint{4, 5}));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPreprocessTileCoordinateNeighborsAndCoordinateRoundTripTest,
    "UDLODPreprocessor.Preprocess.TileCoordinate.NeighborsAndCoordinateRoundTrip",
    TestFlags)

bool FPreprocessTileCoordinateNeighborsAndCoordinateRoundTripTest::RunTest(
    const FString& Parameters) {
    const FTileCoordinate edge_tile{0u, 2u, FIntPoint{0, 0}};
    auto neighbors = edge_tile.neighbors();

    TestEqual(TEXT("Neighbor count"), neighbors.Num(), 8);
    if (neighbors.Num() != 8) { return false; }

    TestTrue(TEXT("Top neighbor invalid"), neighbors[0].Get<0>() == FTileCoordinate::INVALID());
    TestTrue(
        TEXT("Right neighbor"),
        neighbors[1].Get<0>() == FTileCoordinate(0u, 2u, FIntPoint{1, 0}));
    TestTrue(
        TEXT("Bottom neighbor"),
        neighbors[2].Get<0>() == FTileCoordinate(0u, 2u, FIntPoint{0, 1}));
    TestTrue(TEXT("Left neighbor invalid"), neighbors[3].Get<0>() == FTileCoordinate::INVALID());
    TestTrue(
        TEXT("Bottom-right neighbor"),
        neighbors[6].Get<0>() == FTileCoordinate(0u, 2u, FIntPoint{1, 1}));

    const auto [face, uv] = FCoordinate::from_unit_position(FVector3d{0.2, 0.0, -0.1});
    TestEqual(TEXT("Coordinate face"), face, 0u);
    TestEqual(TEXT("Coordinate U"), uv.X, 0.7);
    TestEqual(TEXT("Coordinate V"), uv.Y, 0.4);

    const FVector3d normalized = position_local_to_unit(
        FVector3d{2.0, 4.0, 0.0},
        FVector3d{2.0, 2.0, 2.0});
    TestTrue(
        TEXT("Local-to-unit result is normalized"),
        FMath::IsNearlyEqual(normalized.Length(), 1.0));

    return true;
}

#endif
