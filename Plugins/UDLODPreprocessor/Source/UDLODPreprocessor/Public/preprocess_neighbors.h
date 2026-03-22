#pragma once

#include "preprocess_gdal_extended.h"
#include "preprocess_result.h"
#include "preprocess_tile_coordinate.h"
#include "Containers/StaticArray.h"

namespace preprocess {
inline constexpr uint32 NUM_NEIGHBORS = 8;

template <typename T>
    requires GdalType<T> && Copy<T>
PreprocessResult<void> neighbor_data(
    const GDALDatasetRef& tile_dataset,
    const GDALDatasetRef& neighbor_dataset,
    const FaceRotation rotation,
    const uint32 i,
    const TStaticArray<isize_c, NUM_NEIGHBORS>& src_offsets,
    const TStaticArray<isize_c, NUM_NEIGHBORS>& dst_offsets,
    const TStaticArray<usize_c, NUM_NEIGHBORS>& sizes
) {
    const auto size = sizes[i];
    const auto dst_offset = dst_offsets[i];
    const auto dst_size = size;
    const auto tile_rasters_result = try_collect_rasterbands(tile_dataset);
    if (!tile_rasters_result.has_value()) {
        return std::unexpected{FPreprocessError::Gdal(tile_rasters_result.error())};
    }

    const auto neighbor_rasters_result = try_collect_rasterbands(neighbor_dataset);
    if (!neighbor_rasters_result.has_value()) {
        return std::unexpected{FPreprocessError::Gdal(neighbor_rasters_result.error())};
    }

    TTuple<isize_c, usize_c> rr = [&]() -> TTuple<isize_c, usize_c> {
        switch (rotation) {
        case FaceRotation::Identical:
        case FaceRotation::ShiftU:
        case FaceRotation::ShiftV: { return {src_offsets[i], size}; }
        case FaceRotation::RotateCW: {
            if (i < 4) {
                return {
                    src_offsets[(i + 3) % 4],
                    usize_c{size.Get<1>(), size.Get<0>()}
                };
            }
            return {
                src_offsets[4 + (i + 3) % 4],
                usize_c{size.Get<1>(), size.Get<0>()}
            };
        }
        case FaceRotation::RotateCCW: {
            if (i < 4) {
                return {
                    src_offsets[(i + 1) % 4],
                    usize_c{size.Get<1>(), size.Get<0>()}
                };
            }
            return {
                src_offsets[4 + (i + 1) % 4],
                usize_c{size.Get<1>(), size.Get<0>()}
            };
        }
        case FaceRotation::Backside: { std::unreachable(); }
        }
        checkf(false, TEXT("Invalid rotation"));
        return {isize_c{0, 0}, usize_c{0, 0}};
    }();

    const auto& [src_offset, src_size] = rr;

    for (const auto& [tile_raster, neighbor_raster] :
         ext::iter::zip<GDALRasterBand*>(
             tile_rasters_result.value(),
             neighbor_rasters_result.value())) {
        ext::BufferResult<T> buffer_result =
            read_as<T>(neighbor_raster, src_offset, src_size, src_size);

        if (!buffer_result.has_value()) {
            return std::unexpected{FPreprocessError::Gdal(buffer_result.error())};
        }

        auto buffer = [&buffer_result, &rotation] {
            switch (rotation) {
            case FaceRotation::Identical:
            case FaceRotation::ShiftU:
            case FaceRotation::ShiftV: { return buffer_result.value(); }
            case FaceRotation::RotateCW: {
                TArray<T>& arr = buffer_result.value().data();
                usize_c shape = buffer_result.value().shape();
                auto [width, height] = shape;
                TArray<T> rotated_data = TArray<T>{};
                rotated_data.SetNum(arr.Num());
                for (ext::types::usize x = 0; x < width; ++x) {
                    for (ext::types::usize y = 0; y < height; ++y) {
                        auto* data = &rotated_data[y * width + (width - 1 - x)];
                        *data = arr[x * height + y];
                    }
                }
                return ext::Buffer<T>{
                    MoveTemp(rotated_data),
                    usize_c{
                        shape.Get<1>(),
                        shape.Get<0>()}
                };
            }
            case FaceRotation::RotateCCW: {
                TArray<T>& arr = buffer_result.value().data();
                usize_c shape = buffer_result.value().shape();
                auto [width, height] = shape;
                TArray<T> rotated_data = TArray<T>{};
                rotated_data.SetNum(arr.Num());
                for (ext::types::usize x = 0; x < width; ++x) {
                    for (ext::types::usize y = 0; y < height; ++y) {
                        T* data = &rotated_data[(height - 1 - y) * width + x];
                        *data = arr[x * height + y];
                    }
                }
                return ext::Buffer<T>{
                    MoveTemp(rotated_data),
                    usize_c{
                        shape.Get<1>(),
                        shape.Get<0>()}
                };
            }
            case FaceRotation::Backside:
            default: std::unreachable();
            }
        }();

        const auto write_result = write<T>(tile_raster, dst_offset, dst_size, buffer);

        if (!write_result.has_value()) {
            return std::unexpected{FPreprocessError::Gdal(write_result.error())};
        }
    }

    return {};
}
}
