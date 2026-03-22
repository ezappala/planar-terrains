#pragma once

#include "preprocess_dataset.h"
#include "preprocess_gdal_extended.h"
#include "preprocess_result.h"

namespace preprocess {
template <typename T>
    requires GdalType<T> && Copy<T>
PreprocessResult<TMap<uint32, FaceInfo>> reproject_planar(
    const GDALDatasetRef src_dataset,
    FPreprocessContext& context
) {
    constexpr auto face = 0;
    const auto width = static_cast<uint64>(src_dataset->GetRasterXSize());
    const auto height = static_cast<uint64>(src_dataset->GetRasterYSize());

    const auto max_dimension = static_cast<double>(FMath::Max(width, height));
    const auto tile_size = static_cast<double>(context.attachment.
        center_size());
    const auto max_lod = FMath::CeilLogTwo(max_dimension / tile_size);

    const FString stub = FString::Printf(TEXT("face%u.tif"), face);
    const ANSICHAR* dst_path = TCHAR_TO_UTF8(*(context.temp_dir / stub));

    PreprocessResult<GDALDatasetRef> dst_dataset_result =
        create_empty_dataset<T>(dst_path, {width, height}, geo_transform(src_dataset), context);

    if (!dst_dataset_result.has_value()) { return std::unexpected{dst_dataset_result.error()}; }
    const auto dst_dataset = MoveTemp(dst_dataset_result.value());

    const auto band_count = src_dataset->GetRasterCount();
    constexpr uint64 TargetChunkBytes = 64ull * 1024ull * 1024ull;
    constexpr uint64 MaxChunkRows = 1024ull;
    const uint64 bytes_per_row = FMath::Max<uint64>(
        1ull,
        width * sizeof(T));
    const uint64 chunk_size = FMath::Clamp<uint64>(
        TargetChunkBytes / bytes_per_row,
        1ull,
        FMath::Min<uint64>(height, MaxChunkRows));

    for (ext::types::usize band = 0; band < band_count; ++band) {
        auto* src_band = src_dataset->GetRasterBand(band + 1);
        auto* dst_band = dst_dataset->GetRasterBand(band + 1);

        for (const auto& chunk_start :
             ext::iter::step_by(0ull, height, chunk_size)) {
            const auto chunk_height = FMath::Min(
                chunk_size,
                height - chunk_start);

            ext::BufferResult<T> src_band_result = read_as<T>(
                src_band,
                {0, static_cast<ext::types::isize>(chunk_start)},
                {width, chunk_height},
                {width, chunk_height});

            if (!src_band_result.has_value()) {
                return std::unexpected{
                    FPreprocessError::Gdal(src_band_result.error())
                };
            }

            auto buffer = src_band_result.value();

            auto dst_band_result = write<T>(
                dst_band,
                {0, static_cast<ext::types::isize>(chunk_start)},
                {static_cast<ext::types::usize>(width), chunk_height},
                buffer);

            if (!dst_band_result.has_value()) {
                return std::unexpected{
                    FPreprocessError::From(dst_band_result.error())
                };
            }
        }
    }

    if (context.attachment_label.ToLower() == "height") {
        TStaticArray<double, 2> min_max{0., 0.};
        dst_dataset->GetRasterBand(1)->ComputeRasterMinMax(
            true,
            min_max.GetData());

        context.min_height = FMath::Min(
            context.min_height,
            static_cast<float>(min_max[0]));
        context.max_height = FMath::Max(
            context.max_height,
            static_cast<float>(min_max[1]));
    }

    const auto face_info = FaceInfo{
        .lod = max_lod,
        .pixel_start = {0, 0},
        .pixel_end = {static_cast<int32>(width), static_cast<int32>(height)},
        .path = dst_path
    };

    auto faces = TMap<uint32, FaceInfo>{};
    faces.Add(face, face_info);
    return faces;
}
}
