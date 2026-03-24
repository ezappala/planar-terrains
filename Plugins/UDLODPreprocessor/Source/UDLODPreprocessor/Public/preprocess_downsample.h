#pragma once
#include "ext_buffer.h"
#include "preprocess_dataset.h"
#include "preprocess_gdal_extended.h"
#include "preprocess_parallel.h"
#include "preprocess_result.h"
#include "preprocess_tile_coordinate.h"
#include "Containers/Array.h"

namespace preprocess {
inline TArray<TArray<FTileCoordinate>> compute_tiles_to_downsample(
    const TArray<FTileCoordinate>& input_tiles) {
    auto tiles_to_downsample = ext::iter::filter_map_unique<FTileCoordinate, FTileCoordinate>(
        input_tiles,
        [](const FTileCoordinate tile) -> TOptional<FTileCoordinate> { return tile.parent(); });

    auto new_tiles = tiles_to_downsample;

    while (!new_tiles.IsEmpty()) {
        for (const auto& tile : ext::iter::drain<TSet<FTileCoordinate>,
                 FTileCoordinate>(new_tiles)) {
            const auto parent = tile.parent();
            if (parent.IsSet()) {
                bool is_already_in_set = false;
                tiles_to_downsample.Add(parent.GetValue(), &is_already_in_set);

                if (is_already_in_set) { continue; }

                new_tiles.Add(parent.GetValue());
            }
        }
    }
    const auto tiles = ext::iter::drain<TSet<FTileCoordinate>,
        FTileCoordinate>(tiles_to_downsample);

    const auto max_lod = ext::iter::fold<FTileCoordinate>(
        tiles,
        0,
        [](const int32 m_lod, const FTileCoordinate tile_coordinate) {
            return FMath::Max(m_lod, tile_coordinate.lod);
        });

    return ext::iter::map_range_inclusive_rev(
        0,
        max_lod,
        [&tiles](int32 lod) {
            return ext::iter::filter<FTileCoordinate>(
                tiles,
                [&lod](const FTileCoordinate tile) { return tile.lod == lod; });
        });
}

template <typename T>
    requires GdalType<T> && Copy<T> && PartialEq<T> && NumCast<T>
PreprocessResult<void> downsample(
    const TArray<FTileCoordinate>& input_tiles,
    const FPreprocessContext& context) {
    using namespace ext;
    using namespace ext::iter;
    using types::isize, types::usize;
    using std::expected, std::unexpected;

    const auto child_center_size = context.attachment.center_size() / 2;
    const auto border_offset = isize_c{
        static_cast<isize>(context.attachment.border_size),
        static_cast<isize>(context.attachment.border_size)};

    const auto tile_size = usize_c{
        static_cast<usize>(context.attachment.center_size()),
        static_cast<usize>(context.attachment.center_size())};

    const auto child_size = usize_c{
        static_cast<usize>(child_center_size),
        static_cast<usize>(child_center_size)};

    if (const auto iter_result = par_iter_try_for_each<FTileCoordinate, void, FPreprocessError>(
        input_tiles,
        [&context, &border_offset, &child_size, &tile_size, &child_center_size](
        FTileCoordinate tile_coordinate) -> PreprocessResult<void> {
            PreprocessResult<GDALDatasetRef> tile_dataset_result = create_tile_dataset<T>(
                tile_coordinate,
                context);

            if (!tile_dataset_result.has_value()) {
                return unexpected{tile_dataset_result.error()};
            }

            const GDALDatasetRef tile_dataset = MoveTemp(tile_dataset_result.value());
            auto tile_rasters_result = try_collect_rasterbands(tile_dataset);
            if (!tile_rasters_result.has_value()) {
                return unexpected{FPreprocessError::Gdal(tile_rasters_result.error())};
            }

            auto tile_rasters = MoveTemp(tile_rasters_result.value());
            TArray<PreprocessResult<Buffer<T>>> tile_buffers = map<GDALRasterBand*>(
                tile_rasters,
                [&border_offset, &child_size, &tile_size](
                GDALRasterBand* tile_raster) -> PreprocessResult<Buffer<T>> {
                    BufferResult<T> tile_buffer_result = read_as<T>(
                        tile_raster,
                        border_offset,
                        tile_size,
                        tile_size);

                    if (!tile_buffer_result.has_value()) {
                        return unexpected{FPreprocessError::From(tile_buffer_result.error())};
                    }

                    return tile_buffer_result.value();
                });

            for (const auto& child_coordinate : tile_coordinate.children()) {
                auto child_dataset_result = load_tile_dataset_if_exists(child_coordinate, context);
                if (!child_dataset_result.has_value()) {
                    return unexpected{child_dataset_result.error()};
                }

                if (!child_dataset_result.value().IsSet()) { continue; }
                const auto child_dataset = MoveTemp(child_dataset_result.value().GetValue());
                const auto child_rasters = rasterbands(child_dataset);
                for (int32 band_index = 0; band_index < child_rasters.Num(); ++band_index) {
                    const auto& child_raster_result = child_rasters[band_index];
                    if (!child_raster_result.has_value()) {
                        return unexpected{FPreprocessError::Gdal(child_raster_result.error())};
                    }

                    GDALRasterBand* child_raster = child_raster_result.value();
                    int has_no_data_value = 0;
                    const auto raw_no_data_value = child_raster->GetNoDataValue(
                        &has_no_data_value);
                    const TOptional<T> no_data_value = has_no_data_value
                        ? TOptional<T>{T(raw_no_data_value)}
                        : NullOpt;
                    const BufferResult<T> child_buffer_result = read_as<T>(
                        child_raster,
                        border_offset,
                        tile_size,
                        child_size,
                        GRIORA_Bilinear);

                    if (!child_buffer_result.has_value()) {
                        return unexpected{FPreprocessError::Gdal(child_buffer_result.error())};
                    }

                    const Buffer<T> child_buffer = child_buffer_result.value();
                    auto& tile_buffer_result = tile_buffers[band_index];
                    if (!tile_buffer_result.has_value()) {
                        return unexpected{tile_buffer_result.error()};
                    }

                    Buffer<T>& tile_buffer = tile_buffer_result.value();
                    TArray<TTuple<usize, usize, T>> child_iter = indexed_iter<T>(child_buffer);
                    for (const auto& [child_x, child_y, child_value] : child_iter) {
                        const auto tile_xy = isize_c{
                            static_cast<isize>(child_x),
                            static_cast<isize>(child_y)} + isize_c{
                            child_coordinate.xy.X % 2,
                            child_coordinate.xy.Y % 2} * static_cast<isize>(child_center_size);

                        if (no_data_value.IsSet() && child_value == no_data_value.GetValue()) {
                            continue;
                        }
                        tile_buffer[tile_xy.transpose()] = child_value;
                    }
                }
            }

            for (int32 band_index = 0; band_index < tile_rasters.Num(); ++band_index) {
                GDALRasterBand* tile_raster = tile_rasters[band_index];
                auto& tile_buffer_result = tile_buffers[band_index];
                if (!tile_buffer_result.has_value()) {
                    return unexpected{tile_buffer_result.error()};
                }

                Buffer<T>& tile_buffer = tile_buffer_result.value();
                const auto write_result = write<T>(
                    tile_raster,
                    border_offset,
                    tile_size,
                    tile_buffer);

                if (!write_result.has_value()) {
                    return unexpected{FPreprocessError::Gdal(write_result.error())};
                }
            }

            return {};
        }); !iter_result.has_value()) {
        return unexpected{iter_result.error()};
    }

    return {};
}
}
