#pragma once

#include "preprocess_dataset.h"
#include "preprocess_gdal_extended.h"
#include "preprocess_result.h"

namespace preprocess {
inline int32 derive_planar_lod_count(
    const uint64 width,
    const uint64 height,
    const FAttachmentConfig& attachment
) {
    const double max_dimension = static_cast<double>(FMath::Max(width, height));
    const double tile_size = static_cast<double>(attachment.center_size());
    const double lod_ratio = max_dimension / tile_size;
    const uint32 max_lod = lod_ratio <= 1.0
        ? 0u
        : static_cast<uint32>(FMath::CeilToInt(FMath::Log2(lod_ratio)));
    return static_cast<int32>(max_lod + 1u);
}

inline int32 resolve_planar_lod_count(
    const uint64 width,
    const uint64 height,
    const FPreprocessContext& context
) {
    const int32 derived_lod_count = derive_planar_lod_count(width, height, context.attachment);
    if (!context.lod_count.IsSet()) { return derived_lod_count; }

    return FMath::Clamp(context.lod_count.GetValue(), 1, derived_lod_count);
}

inline FUint64Point resolve_planar_output_size(
    const uint64 width,
    const uint64 height,
    const FPreprocessContext& context,
    const int32 lod_count
) {
    check(lod_count > 0);

    const uint32 max_lod = static_cast<uint32>(lod_count - 1);
    const uint64 max_dimension_pixels = static_cast<uint64>(context.attachment.center_size()) <<
        max_lod;
    const uint64 source_max_dimension = FMath::Max(width, height);
    if (source_max_dimension <= max_dimension_pixels) { return FUint64Point{width, height}; }

    const double scale = static_cast<double>(max_dimension_pixels) /
        static_cast<double>(source_max_dimension);
    const auto scale_dimension = [scale, max_dimension_pixels](const uint64 dimension) {
        return FMath::Max<uint64>(
            1ull,
            FMath::Min<uint64>(
                max_dimension_pixels,
                static_cast<uint64>(FMath::CeilToDouble(
                    static_cast<double>(dimension) * scale))));
    };

    return FUint64Point{scale_dimension(width), scale_dimension(height)};
}

inline GeoTransformRef scale_geo_transform(
    GeoTransformRef transform,
    const double scale_x,
    const double scale_y
) {
    if (!transform) { return transform; }

    transform[1] *= scale_x;
    transform[2] *= scale_y;
    transform[4] *= scale_x;
    transform[5] *= scale_y;
    return transform;
}

template <typename T>
    requires GdalType<T> && Copy<T>
PreprocessResult<TMap<uint32, FaceInfo>> reproject_planar(
    const GDALDatasetRef src_dataset,
    FPreprocessContext& context
) {
    constexpr auto face = 0;
    const auto width = static_cast<uint64>(src_dataset->GetRasterXSize());
    const auto height = static_cast<uint64>(src_dataset->GetRasterYSize());
    const int32 lod_count = resolve_planar_lod_count(width, height, context);
    const uint32 max_lod = static_cast<uint32>(lod_count - 1);
    const FUint64Point dst_size = resolve_planar_output_size(width, height, context, lod_count);
    const bool bNeedsResample = dst_size.X != width || dst_size.Y != height;
    context.lod_count = lod_count;

    GeoTransformRef dst_transform = geo_transform(src_dataset);
    if (bNeedsResample) {
        const double scale_x = static_cast<double>(width) / static_cast<double>(dst_size.X);
        const double scale_y = static_cast<double>(height) / static_cast<double>(dst_size.Y);
        dst_transform = scale_geo_transform(MoveTemp(dst_transform), scale_x, scale_y);
    }

    PreprocessResult<GDALDatasetRef> dst_dataset_result =
        create_empty_dataset<T>(
            FTCHARToUTF8(*(context.temp_dir / FString::Printf(TEXT("face%u.tif"), face))).Get(),
            dst_size,
            MoveTemp(dst_transform),
            context);

    if (!dst_dataset_result.has_value()) { return std::unexpected{dst_dataset_result.error()}; }
    const auto dst_dataset = MoveTemp(dst_dataset_result.value());

    const auto band_count = src_dataset->GetRasterCount();
    constexpr uint64 TargetChunkBytes = 64ull * 1024ull * 1024ull;
    constexpr uint64 MaxChunkRows = 1024ull;
    const uint64 bytes_per_row = FMath::Max<uint64>(1ull, dst_size.X * sizeof(T));
    const uint64 chunk_size = FMath::Clamp<uint64>(
        TargetChunkBytes / bytes_per_row,
        1ull,
        FMath::Min<uint64>(dst_size.Y, MaxChunkRows));

    for (ext::types::usize band = 0; band < band_count; ++band) {
        auto* src_band = src_dataset->GetRasterBand(band + 1);
        auto* dst_band = dst_dataset->GetRasterBand(band + 1);

        for (uint64 chunk_start = 0; chunk_start < dst_size.Y; chunk_start += chunk_size) {
            const auto chunk_height = FMath::Min(chunk_size, dst_size.Y - chunk_start);
            const uint64 chunk_end = chunk_start + chunk_height;
            const uint64 src_y_start = bNeedsResample
                ? FMath::Min<uint64>(
                    height - 1,
                    static_cast<uint64>(FMath::FloorToDouble(
                        static_cast<double>(chunk_start) * static_cast<double>(height) /
                        static_cast<double>(dst_size.Y))))
                : chunk_start;
            const uint64 src_y_end = bNeedsResample
                ? FMath::Min<uint64>(
                    height,
                    static_cast<uint64>(FMath::CeilToDouble(
                        static_cast<double>(chunk_end) * static_cast<double>(height) /
                        static_cast<double>(dst_size.Y))))
                : chunk_end;
            const uint64 src_chunk_height = FMath::Max<uint64>(1ull, src_y_end - src_y_start);

            ext::BufferResult<T> src_band_result = read_as<T>(
                src_band,
                {0, static_cast<ext::types::isize>(src_y_start)},
                {width, src_chunk_height},
                {dst_size.X, chunk_height},
                bNeedsResample
                    ? TOptional<GDALRIOResampleAlg>{GRIORA_Bilinear}
                    : NullOpt);

            if (!src_band_result.has_value()) {
                return std::unexpected{FPreprocessError::Gdal(src_band_result.error())};
            }

            auto buffer = src_band_result.value();

            auto dst_band_result = write<T>(
                dst_band,
                {0, static_cast<ext::types::isize>(chunk_start)},
                {static_cast<ext::types::usize>(dst_size.X), chunk_height},
                buffer);

            if (!dst_band_result.has_value()) {
                return std::unexpected{FPreprocessError::From(dst_band_result.error())};
            }
        }
    }

    if (context.attachment_label.ToLower() == "height") {
        TStaticArray<double, 2> min_max{0., 0.};
        dst_dataset->GetRasterBand(1)->ComputeRasterMinMax(true, min_max.GetData());

        context.min_height = FMath::Min(context.min_height, static_cast<float>(min_max[0]));
        context.max_height = FMath::Max(context.max_height, static_cast<float>(min_max[1]));
    }

    const auto face_info = FaceInfo{
        .lod = max_lod,
        .pixel_start = {0, 0},
        .pixel_end = {static_cast<int32>(dst_size.X), static_cast<int32>(dst_size.Y)},
        .path = (context.temp_dir / FString::Printf(TEXT("face%u.tif"), face))
    };

    auto faces = TMap<uint32, FaceInfo>{};
    faces.Add(face, face_info);
    return faces;
}
}
