#pragma once
#include "preprocess_dataset.h"
#include "preprocess_gdal_extended.h"
#include "preprocess_parallel.h"
#include "preprocess_shared_dataset.h"

namespace preprocess {
template <typename T>
    requires GdalType<T> && Copy<T>
PreprocessResult<TArray<FTileCoordinate>> split(
    const TArray<FTileCoordinate>& input_tiles,
    const TMap<uint32, FaceInfo>& faces,
    const TMap<uint32, SharedDatasetRO>& datasets,
    const FPreprocessContext& context
) {
    TArray<FTileCoordinate> out = par_iter_map_filter_map<FTileCoordinate, FPreprocessError>(
        input_tiles,
        [&faces, &datasets, &context](
        const FTileCoordinate& tile_coordinate
    ) -> PreprocessResult<TOptional<FTileCoordinate>> {
            const auto src_dataset = datasets[static_cast<uint32>(tile_coordinate.face)].get();
            const auto face = faces[static_cast<uint32>(tile_coordinate.face)];
            const int32 center_size = static_cast<int32>(context.attachment.center_size());

            const auto tile_pixel_start = static_cast<isize_c>(
                tile_coordinate.xy * center_size);

            const auto tile_pixel_end = static_cast<isize_c>(
                (tile_coordinate.xy + 1) * center_size);

            const auto copy_size = FMath::Min(tile_pixel_end, face.pixel_end) -
                FMath::Max(tile_pixel_start, face.pixel_start);

            const auto src_offset = FMath::Max(tile_pixel_start - face.pixel_start, isize_c{0, 0});

            const auto tile_offset = FMath::Max(face.pixel_start - tile_pixel_start, isize_c{0, 0})
                + context.attachment.border_size;

            auto has_data = false;
            TArray<ext::Buffer<T>> copy_buffers{};
            for (auto band_result : rasterbands(src_dataset)) {
                if (!band_result.has_value()) {
                    return std::unexpected{FPreprocessError::Gdal(band_result.error())};
                }

                auto* src_raster = band_result.value();
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
                    ext::Buffer<T> copy_buffer = copy_buffers[band];
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

    return out;
}
}
