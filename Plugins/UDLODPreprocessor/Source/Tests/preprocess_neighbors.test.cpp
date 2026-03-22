#if WITH_DEV_AUTOMATION_TESTS

#include "tests_shared.h"

#include "preprocess_neighbors.h"

using namespace preprocess;
using namespace preprocess::tests;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPreprocessNeighborsCopiesAllBandsTest,
    "UDLODPreprocessor.Preprocess.Neighbors.CopiesAllBands",
    TestFlags)

bool FPreprocessNeighborsCopiesAllBandsTest::RunTest(const FString& Parameters) {
    GDALAllRegister();

    const GDALDatasetRef tile_dataset = create_memory_dataset<uint16>(4, 4, 2);
    const GDALDatasetRef neighbor_dataset = create_memory_dataset<uint16>(4, 4, 2);

    {
        ext::Buffer band1{
            TArray<uint16>{
                10,
                11,
                12,
                13,
                14,
                15,
                16,
                17,
                18,
                19,
                20,
                21,
                22,
                23,
                24,
                25},
            usize_c{4, 4}
        };
        ext::Buffer band2{
            TArray<uint16>{
                100,
                101,
                102,
                103,
                104,
                105,
                106,
                107,
                108,
                109,
                110,
                111,
                112,
                113,
                114,
                115},
            usize_c{4, 4}
        };

        TestTrue(
            TEXT("Neighbor band 1 seeded"),
            write<uint16>(neighbor_dataset->GetRasterBand(1), {0, 0}, {4, 4}, band1).has_value());
        TestTrue(
            TEXT("Neighbor band 2 seeded"),
            write<uint16>(neighbor_dataset->GetRasterBand(2), {0, 0}, {4, 4}, band2).has_value());
    }

    const TStaticArray<isize_c, NUM_NEIGHBORS> src_offsets{
        isize_c{1, 0},
        isize_c{3, 1},
        isize_c{1, 3},
        isize_c{0, 1},
        isize_c{0, 0},
        isize_c{3, 0},
        isize_c{3, 3},
        isize_c{0, 3}
    };
    const TStaticArray<isize_c, NUM_NEIGHBORS> dst_offsets{
        isize_c{1, 0},
        isize_c{3, 1},
        isize_c{1, 3},
        isize_c{0, 1},
        isize_c{0, 0},
        isize_c{3, 0},
        isize_c{3, 3},
        isize_c{0, 3}
    };
    const TStaticArray<usize_c, NUM_NEIGHBORS> sizes{
        usize_c{2, 1},
        usize_c{1, 2},
        usize_c{2, 1},
        usize_c{1, 2},
        usize_c{1, 1},
        usize_c{1, 1},
        usize_c{1, 1},
        usize_c{1, 1}
    };

    const auto copy_result = neighbor_data<uint16>(
        tile_dataset,
        neighbor_dataset,
        FaceRotation::Identical,
        0,
        src_offsets,
        dst_offsets,
        sizes);
    TestTrue(TEXT("Neighbor copy succeeds"), copy_result.has_value());
    if (!copy_result.has_value()) { return false; }

    auto band1 = read_as<uint16>(tile_dataset->GetRasterBand(1), {1, 0}, {2, 1}, {2, 1});
    auto band2 = read_as<uint16>(tile_dataset->GetRasterBand(2), {1, 0}, {2, 1}, {2, 1});
    TestTrue(TEXT("Tile band 1 read succeeds"), band1.has_value());
    TestTrue(TEXT("Tile band 2 read succeeds"), band2.has_value());
    if (!band1.has_value() || !band2.has_value()) { return false; }

    TestEqual(TEXT("Band 1 first copied value"), band1.value().data()[0], static_cast<uint16>(11));
    TestEqual(TEXT("Band 1 second copied value"), band1.value().data()[1], static_cast<uint16>(12));
    TestEqual(TEXT("Band 2 first copied value"), band2.value().data()[0], static_cast<uint16>(101));
    TestEqual(
        TEXT("Band 2 second copied value"),
        band2.value().data()[1],
        static_cast<uint16>(102));

    return true;
}

#endif
