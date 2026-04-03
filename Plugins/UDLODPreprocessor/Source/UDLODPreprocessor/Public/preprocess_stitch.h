#pragma once

#include "preprocess_dataset.h"
#include "preprocess_gdal_extended.h"
#include "preprocess_neighbors.h"
#include "preprocess_result.h"
#include "preprocess_shared_dataset.h"
#include "Containers/StaticArray.h"

namespace preprocess {
template <typename T>
PreprocessResult<void> stitch_corners(
    const GDALDatasetRef& tile_dataset,
    const TStaticArray<isize_c, NUM_NEIGHBORS>& dst_offsets,
    const uint32 i,
    const FPreprocessContext& context
) {
    auto dst_offset = dst_offsets[i];
    auto border_size = context.attachment.border_size;
    auto offset_size = context.attachment.offset_size();

    const auto src_offset = FIntPoint{
        [&dst_offset, &offset_size, &border_size] {
            if (dst_offset.Get<0>() == 0) { return border_size; }
            if (dst_offset.Get<0>() == offset_size) { return static_cast<int32>(offset_size) - 1; }
            return static_cast<int32>(offset_size);
        }(),
        [&dst_offset, &border_size, &offset_size] {
            if (dst_offset.Get<1>() == 0) { return border_size; }
            if (dst_offset.Get<1>() == offset_size) { return static_cast<int32>(offset_size) - 1; }
            return static_cast<int32>(offset_size);
        }()
    };

    const auto corner = i - 4;

    static const FIntPoint corner_offsets[4][3] = {
        {{0, 0}, {-1, 0}, {0, -1}},
        {{0, 0}, {1, 0}, {0, -1}},
        {{0, 0}, {1, 0}, {0, 1}},
        {{0, 0}, {-1, 0}, {0, 1}}
    };

    for (int32 raster_index = 1; raster_index <= tile_dataset->GetRasterCount(); ++raster_index) {
        GDALRasterBand* raster = tile_dataset->GetRasterBand(raster_index);
        if (raster == nullptr) {
            return std::unexpected{FPreprocessError::Gdal(CPLE_IllegalArg)};
        }

        double sum = 0.0;
        for (uint8 j = 0; j < 3; ++j) {
            const auto offset = corner_offsets[corner][j];
            ext::BufferResult<T> buffer_result =
                read_as<T>(
                    raster,
                    {
                        src_offset.X + offset.X,
                        src_offset.Y + offset.Y
                    },
                    {1, 1},
                    {1, 1});
            if (!buffer_result.has_value()) {
                return std::unexpected{
                    FPreprocessError::Gdal(buffer_result.error())
                };
            }
            ext::Buffer<T> buffer = buffer_result.value();
            TArray<T>& data = buffer.data();
            sum += static_cast<double>(data[0]);
        }

        const double avg = sum / 3.0;
        auto buffer = ext::Buffer<T>{{static_cast<T>(avg)}, {1, 1}};
        const auto write_result = write<T>(
            raster,
            dst_offset,
            {1, 1},
            buffer);

        if (!write_result.has_value()) {
            return std::unexpected{
                FPreprocessError::Gdal(write_result.error())
            };
        }
    }
    return {};
}

template <typename T>
    requires GdalType<T> && Copy<T>
PreprocessResult<void> stitch(
    const TArray<FTileCoordinate>& tiles,
    const FPreprocessContext& context
) {
    using ext::types::usize, ext::types::isize;
    const auto center_size = static_cast<usize>(context.attachment.center_size());
    const auto border_size = static_cast<usize>(context.attachment.border_size);
    const auto offset_size = static_cast<usize>(context.attachment.offset_size());

    const isize iborder_size = static_cast<isize>(border_size);
    const isize icenter_size = static_cast<isize>(center_size);
    auto src_offsets = TStaticArray<isize_c, NUM_NEIGHBORS>{
        isize_c{iborder_size, icenter_size},
        isize_c{iborder_size, iborder_size},
        isize_c{iborder_size, iborder_size},
        isize_c{icenter_size, iborder_size},
        isize_c{icenter_size, icenter_size},
        isize_c{iborder_size, icenter_size},
        isize_c{iborder_size, iborder_size},
        isize_c{icenter_size, iborder_size}
    };

    const isize ioffset_size = static_cast<isize>(offset_size);
    auto dst_offsets = TStaticArray<isize_c, NUM_NEIGHBORS>{
        isize_c{iborder_size, 0},
        isize_c{ioffset_size, iborder_size},
        isize_c{iborder_size, ioffset_size},
        isize_c{0, iborder_size},
        isize_c{0, 0},
        isize_c{ioffset_size, 0},
        isize_c{ioffset_size, ioffset_size},
        isize_c{0, ioffset_size}
    };

    auto sizes = TStaticArray<usize_c, NUM_NEIGHBORS>{
        usize_c{center_size, border_size},
        usize_c{border_size, center_size},
        usize_c{center_size, border_size},
        usize_c{border_size, center_size},
        usize_c{border_size, border_size},
        usize_c{border_size, border_size},
        usize_c{border_size, border_size},
        usize_c{border_size, border_size}
    };

    TSet<FTileCoordinate> existing_tiles{};
    existing_tiles.Reserve(tiles.Num());
    TMap<FTileCoordinate, SharedDatasetRO> read_only_tiles{};
    read_only_tiles.Reserve(tiles.Num());
    for (const FTileCoordinate& tile : tiles) {
        existing_tiles.Add(tile);
        read_only_tiles.Emplace(tile, SharedDatasetRO{tile.path(context.tile_dir)});
    }

    if (const auto iter_result = par_iter_try_for_each<FTileCoordinate, void, FPreprocessError>(
        tiles,
        [&context, &src_offsets, &dst_offsets, &sizes, &existing_tiles, &read_only_tiles](
        const FTileCoordinate tile_coordinate) -> std::expected<void, FPreprocessError> {
            PreprocessResult<GDALDatasetRef> tile_dataset_result =
                update_tile_dataset(tile_coordinate, context);
            if (!tile_dataset_result.has_value()) {
                return std::unexpected{tile_dataset_result.error()};
            }
            const GDALDatasetRef tile_dataset = MoveTemp(tile_dataset_result.value());

            const auto neighbors = tile_coordinate.neighbors();
            for (usize i = 0; i < neighbors.Num(); ++i) {
                const auto [neighbor_coordinate, rotation] = neighbors[i];
                if (existing_tiles.Contains(neighbor_coordinate)) {
                    const SharedDatasetRO* neighbor_dataset = read_only_tiles.Find(neighbor_coordinate);
                    if (neighbor_dataset == nullptr) {
                        return std::unexpected{
                            FPreprocessError::Parse(TEXT("Missing cached stitch neighbor dataset"))};
                    }

                    auto nd_result = neighbor_data<T>(
                        tile_dataset,
                        neighbor_dataset->get(),
                        rotation,
                        i,
                        src_offsets,
                        dst_offsets,
                        sizes
                    );
                    if (!nd_result.has_value()) { return std::unexpected{nd_result.error()}; }
                } else if (i >= 4) {
                    auto sc_result = stitch_corners<T>(
                        tile_dataset,
                        dst_offsets,
                        i,
                        context
                    );
                    if (!sc_result.has_value()) { return std::unexpected{sc_result.error()}; }
                }
            }

            return {};
        }); !iter_result.has_value()) { return std::unexpected{iter_result.error()}; }
    return {};
}
}
