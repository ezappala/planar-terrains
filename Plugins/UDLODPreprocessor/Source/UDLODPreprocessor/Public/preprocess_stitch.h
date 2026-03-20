#pragma once

#include "preprocess_dataset.h"
#include "preprocess_gdal_extended.h"
#include "preprocess_neighbors.h"
#include "preprocess_result.h"
#include "Algo/Accumulate.h"
#include "Containers/StaticArray.h"

namespace preprocess {
template <typename T>
PreprocessResult<void> stitch_corners(
    GDALDatasetRef tile_dataset,
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

    const FIntPoint corner_offsets[4][3] = {
        {{0, 0}, {-1, 0}, {0, -1}},
        {{0, 0}, {1, 0}, {0, -1}},
        {{0, 0}, {1, 0}, {0, 1}},
        {{0, 0}, {-1, 0}, {0, 1}}
    };

    for (const auto& raster_result : rasterbands(tile_dataset)) {
        auto* raster = raster_result.has_value()
            ? raster_result.value()
            : nullptr;

        if (raster == nullptr) {
            return std::unexpected{
                FPreprocessError::Gdal(raster_result.error())
            };
        }

        TArray<PreprocessResult<T>> corner_values{};
        for (uint8 j = 0; j < corner_offsets[corner]->Num(); ++j) {
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
            ext::Buffer<T> buffer = buffer_result.value();
            TArray<T>& data = buffer.data();
            corner_values.Emplace(PreprocessResult<T>{data[0]});
        }

        const double avg = [&] {
            TArray<double> as_f64{};
            for (auto val : corner_values) { as_f64.Emplace(static_cast<double>(val.value())); }
            const double sum = Algo::Accumulate<double>(as_f64, 0.0);
            return sum / as_f64.Num();
        }();

        auto buffer = ext::Buffer<double>{{avg}, {1, 1}};
        const auto write_result = write<double>(
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

    par_iter_try_for_each<FTileCoordinate, void, FPreprocessError>(
        tiles,
        [&context, &src_offsets, &dst_offsets, &sizes](
        const FTileCoordinate tile_coordinate) -> std::expected<void, FPreprocessError> {
            PreprocessResult<GDALDatasetRef> tile_dataset_result =
                update_tile_dataset(tile_coordinate, context);
            if (!tile_dataset_result.has_value()) {
                return std::unexpected{tile_dataset_result.error()};
            }

            const auto neighbors = tile_coordinate.neighbors();
            for (usize i = 0; i < neighbors.Num(); ++i) {
                const auto [neighbor_coordinate, rotation] = neighbors[i];
                auto neighbor_dataset_result =
                    load_tile_dataset_if_exists(neighbor_coordinate, context);
                if (!neighbor_dataset_result.has_value()) {
                    return std::unexpected{neighbor_dataset_result.error()};
                }
                if (neighbor_dataset_result.value().IsSet()) {
                    auto nd_result = neighbor_data<T>(
                        MoveTemp(tile_dataset_result.value()),
                        MoveTemp(neighbor_dataset_result.value().GetValue()),
                        rotation,
                        i,
                        src_offsets,
                        dst_offsets,
                        sizes
                    );
                    if (!nd_result.has_value()) { return std::unexpected{nd_result.error()}; }
                } else if (i >= 4) {
                    auto sc_result = stitch_corners<T>(
                        MoveTemp(tile_dataset_result.value()),
                        dst_offsets,
                        i,
                        context
                    );
                    if (sc_result.has_value()) { return std::unexpected{sc_result.error()}; }
                }
            }

            return {};
        });
    return {};
}
}
