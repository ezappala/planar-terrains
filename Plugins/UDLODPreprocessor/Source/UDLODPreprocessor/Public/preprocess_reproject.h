#pragma once

#include "preprocess_dataset.h"
#include "preprocess_gdal_extended.h"
#include "preprocess_result.h"
#include "preprocess_util.h"
#include "gdalwarper.h"

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

inline FVector2d apply_geo_transform(
    const GeoTransformRef& transform,
    const double pixel_x,
    const double pixel_y
) {
    return {
        transform[0] + pixel_x * transform[1] + pixel_y * transform[2],
        transform[3] + pixel_x * transform[4] + pixel_y * transform[5]
    };
}

struct FPlanarAnchoredOutput {
    FUint64Point size;
    GeoTransformRef transform;
};

inline FPlanarAnchoredOutput anchor_planar_output_grid(
    GeoTransformRef transform,
    const FUint64Point size
) {
    if (!transform || size.X == 0 || size.Y == 0) {
        return {
            .size = size,
            .transform = MoveTemp(transform)
        };
    }

    if (!FMath::IsNearlyZero(transform[2], UE_DOUBLE_SMALL_NUMBER) ||
        !FMath::IsNearlyZero(transform[4], UE_DOUBLE_SMALL_NUMBER)) {
        return {
            .size = size,
            .transform = MoveTemp(transform)
        };
    }

    const double pixel_width = FMath::Abs(transform[1]);
    const double pixel_height = FMath::Abs(transform[5]);
    if (pixel_width <= UE_DOUBLE_SMALL_NUMBER || pixel_height <= UE_DOUBLE_SMALL_NUMBER) {
        return {
            .size = size,
            .transform = MoveTemp(transform)
        };
    }

    const FVector2d corners[4] = {
        apply_geo_transform(transform, 0.0, 0.0),
        apply_geo_transform(transform, static_cast<double>(size.X), 0.0),
        apply_geo_transform(transform, 0.0, static_cast<double>(size.Y)),
        apply_geo_transform(transform, static_cast<double>(size.X), static_cast<double>(size.Y))
    };

    double min_x = corners[0].X;
    double max_x = corners[0].X;
    double min_y = corners[0].Y;
    double max_y = corners[0].Y;
    for (const FVector2d& corner : corners) {
        min_x = FMath::Min(min_x, corner.X);
        max_x = FMath::Max(max_x, corner.X);
        min_y = FMath::Min(min_y, corner.Y);
        max_y = FMath::Max(max_y, corner.Y);
    }

    const int64 pixel_start_x = static_cast<int64>(FMath::FloorToDouble(min_x / pixel_width));
    const int64 pixel_end_x = static_cast<int64>(FMath::CeilToDouble(max_x / pixel_width));
    const int64 pixel_start_y = static_cast<int64>(FMath::FloorToDouble(min_y / pixel_height));
    const int64 pixel_end_y = static_cast<int64>(FMath::CeilToDouble(max_y / pixel_height));

    FPlanarAnchoredOutput output{
        .size = {
            static_cast<uint64>(FMath::Max<int64>(1, pixel_end_x - pixel_start_x)),
            static_cast<uint64>(FMath::Max<int64>(1, pixel_end_y - pixel_start_y))
        },
        .transform = MoveTemp(transform)
    };

    output.transform[0] = output.transform[1] >= 0.0
        ? static_cast<double>(pixel_start_x) * pixel_width
        : static_cast<double>(pixel_end_x) * pixel_width;
    output.transform[3] = output.transform[5] >= 0.0
        ? static_cast<double>(pixel_start_y) * pixel_height
        : static_cast<double>(pixel_end_y) * pixel_height;

    return output;
}

inline PreprocessResult<void> warp_planar_dataset(
    const GDALDatasetRef& src_dataset,
    const GDALDatasetRef& dst_dataset,
    const FPreprocessContext& context
) {
    const int32 band_count = src_dataset->GetRasterCount();
    TArray<int> bands;
    bands.Reserve(band_count);
    for (int32 band_index = 0; band_index < band_count; ++band_index) {
        bands.Add(band_index + 1);
    }

    TArray<double> src_no_data;
    if (const TOptional<double> src_no_data_value = util::get_no_data_value(src_dataset);
        src_no_data_value.IsSet()) {
        src_no_data.Init(src_no_data_value.GetValue(), band_count);
    }

    TArray<double> dst_no_data;
    if (context.no_data_value.IsSet()) {
        dst_no_data.Init(context.no_data_value.GetValue(), band_count);
    }

    GDALWarpOptions* options = GDALCreateWarpOptions();
    if (options == nullptr) { return std::unexpected{FPreprocessError::Gdal(CPLE_OutOfMemory)}; }

    options->hSrcDS = src_dataset.Get();
    options->hDstDS = dst_dataset.Get();
    options->eResampleAlg = GRA_Bilinear;
    options->dfWarpMemoryLimit = 1024.0 * 1024.0 * 8.0;
    options->eWorkingDataType = static_cast<GDALDataType>(context.data_type);
    options->nBandCount = band_count;
    options->panSrcBands = bands.GetData();
    options->panDstBands = bands.GetData();
    options->padfSrcNoDataReal = src_no_data.IsEmpty() ? nullptr : src_no_data.GetData();
    options->padfDstNoDataReal = dst_no_data.IsEmpty() ? nullptr : dst_no_data.GetData();

    const CPLErr rv = GDALReprojectImage(
        src_dataset.Get(),
        nullptr,
        dst_dataset.Get(),
        nullptr,
        GRA_Bilinear,
        options->dfWarpMemoryLimit,
        0.0,
        GDALDummyProgress,
        nullptr,
        options);

    options->panSrcBands = nullptr;
    options->panDstBands = nullptr;
    options->padfSrcNoDataReal = nullptr;
    options->padfDstNoDataReal = nullptr;
    GDALDestroyWarpOptions(options);

    if (rv != CE_None) { return std::unexpected{FPreprocessError::Gdal(rv)}; }
    return {};
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
    context.lod_count = lod_count;

    const FUint64Point requested_size = resolve_planar_output_size(width, height, context, lod_count);
    GeoTransformRef dst_transform = geo_transform(src_dataset);
    if (requested_size.X != width || requested_size.Y != height) {
        const double scale_x = static_cast<double>(width) / static_cast<double>(requested_size.X);
        const double scale_y = static_cast<double>(height) / static_cast<double>(requested_size.Y);
        dst_transform = scale_geo_transform(MoveTemp(dst_transform), scale_x, scale_y);
    }
    auto anchored_output = anchor_planar_output_grid(MoveTemp(dst_transform), requested_size);
    const FUint64Point dst_size = anchored_output.size;
    dst_transform = MoveTemp(anchored_output.transform);

    PreprocessResult<GDALDatasetRef> dst_dataset_result =
        create_empty_dataset<T>(
            FTCHARToUTF8(*(context.temp_dir / FString::Printf(TEXT("face%u.tif"), face))).Get(),
            dst_size,
            MoveTemp(dst_transform),
            context);

    if (!dst_dataset_result.has_value()) { return std::unexpected{dst_dataset_result.error()}; }
    const auto dst_dataset = MoveTemp(dst_dataset_result.value());

    const char* projection = src_dataset->GetProjectionRef();
    if (projection != nullptr && projection[0] != '\0') {
        const CPLErr projection_result = dst_dataset->SetProjection(projection);
        if (projection_result != CE_None) {
            return std::unexpected{FPreprocessError::Gdal(projection_result)};
        }
    }

    const auto warp_result = warp_planar_dataset(src_dataset, dst_dataset, context);
    if (!warp_result.has_value()) { return std::unexpected{warp_result.error()}; }

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
