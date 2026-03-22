#pragma once
#include "preprocess_dataset.h"
#include "preprocess_gdal_extended.h"
#include "preprocess_parallel.h"
#include "preprocess_shared_dataset.h"

namespace preprocess {
inline isize_c component_min(const isize_c& lhs, const isize_c& rhs) {
    return isize_c{
        FMath::Min(lhs.Get<0>(), rhs.Get<0>()),
        FMath::Min(lhs.Get<1>(), rhs.Get<1>())
    };
}

inline isize_c component_max(const isize_c& lhs, const isize_c& rhs) {
    return isize_c{
        FMath::Max(lhs.Get<0>(), rhs.Get<0>()),
        FMath::Max(lhs.Get<1>(), rhs.Get<1>())
    };
}

template <typename T>
    requires GdalType<T> && Copy<T>
PreprocessResult<TArray<FTileCoordinate>> split(
    const TArray<FTileCoordinate>& input_tiles,
    const TMap<uint32, FaceInfo>& faces,
    const TMap<uint32, SharedDatasetRO>& datasets,
    const FPreprocessContext& context
) {
    const auto out_result = par_iter_map_filter_map<FTileCoordinate, FPreprocessError>(
        input_tiles,
        [&faces, &datasets, &context](
        const FTileCoordinate& tile_coordinate
    ) -> PreprocessResult<TOptional<FTileCoordinate>> {
            const auto& src_dataset = datasets[static_cast<uint32>(tile_coordinate.face)].get();
            const auto face = faces[static_cast<uint32>(tile_coordinate.face)];
            const int32 center_size = static_cast<int32>(context.attachment.center_size());

            const auto tile_pixel_start = static_cast<isize_c>(
                tile_coordinate.xy * center_size);

            const auto tile_pixel_end = static_cast<isize_c>(
                (tile_coordinate.xy + 1) * center_size);

            const auto overlap_start = component_max(tile_pixel_start, face.pixel_start);
            const auto overlap_end = component_min(tile_pixel_end, face.pixel_end);
            const auto copy_size = overlap_end - overlap_start;

            if (copy_size.Get<0>() <= 0 || copy_size.Get<1>() <= 0) {
                return {NullOpt};
            }

            const auto src_offset = component_max(
                tile_pixel_start - face.pixel_start,
                isize_c{0, 0});

            const auto tile_offset = component_max(
                face.pixel_start - tile_pixel_start,
                isize_c{0, 0}) + context.attachment.border_size;

            auto has_data = false;
            TArray<ext::Buffer<T>> copy_buffers{};
            const auto src_rasters_result = try_collect_rasterbands(src_dataset);
            if (!src_rasters_result.has_value()) {
                return std::unexpected{FPreprocessError::Gdal(src_rasters_result.error())};
            }

            const TArray<GDALRasterBand*>& src_rasters = src_rasters_result.value();
            copy_buffers.Reserve(src_rasters.Num());
            for (GDALRasterBand* src_raster : src_rasters) {
                ext::BufferResult<T> copy_buffer_result = read_as<T>(
                    src_raster,
                    src_offset,
                    static_cast<usize_c>(copy_size),
                    static_cast<usize_c>(copy_size),
                    NullOpt);

                if (!copy_buffer_result.has_value()) {
                    return std::unexpected{FPreprocessError::Gdal(copy_buffer_result.error())};
                }

                ext::Buffer<T> copy_buffer = copy_buffer_result.value();
                int success = 0;
                const auto no_data_value = src_raster->GetNoDataValue(&success);

                has_data |= std::any_of(
                    copy_buffer.begin(),
                    copy_buffer.end(),
                    [no_data_value, success](T value) {
                        return !success || value != no_data_value;
                    });
                copy_buffers.Add(MoveTemp(copy_buffer));
            }

            if (has_data) {
                PreprocessResult<GDALDatasetRef> tile_dataset_result = create_tile_dataset<T>(
                    tile_coordinate,
                    context);

                if (!tile_dataset_result.has_value()) {
                    return std::unexpected{tile_dataset_result.error()};
                }

                const auto tile_dataset = MoveTemp(tile_dataset_result.value());
                for (uint32 band = 0; band < static_cast<uint32>(copy_buffers.Num()); ++band) {
                    ext::Buffer<T>& copy_buffer = copy_buffers[band];
                    auto* tile_raster = tile_dataset->GetRasterBand(band + 1);

                    std::expected<void, CPLErrorNum> write_result = write<T>(
                        tile_raster,
                        tile_offset,
                        static_cast<usize_c>(copy_size),
                        copy_buffer);

                    if (!write_result.has_value()) {
                        return std::unexpected{FPreprocessError::Gdal(write_result.error())};
                    }
                }
            }

            return {has_data ? TOptional{tile_coordinate} : NullOpt};
        },
        [](
        PreprocessResult<TOptional<FTileCoordinate>> result)
        -> TOptional<PreprocessResult<FTileCoordinate>> {
            if (!result.has_value()) {
                return TOptional{
                    std::expected<FTileCoordinate, FPreprocessError>{
                        std::unexpected{result.error()}}};
            }

            if (!result.value().IsSet()) { return NullOpt; }

            return TOptional{
                std::expected<FTileCoordinate, FPreprocessError>{result.value().GetValue()}};
        });
    if (!out_result.has_value()) { return std::unexpected{out_result.error()}; }
    return out_result.value();
}
}
