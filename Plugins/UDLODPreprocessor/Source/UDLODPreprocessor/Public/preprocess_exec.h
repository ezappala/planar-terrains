#pragma once
#include "preprocess_downsample.h"
#include "preprocess_fill.h"
#include "preprocess_reproject.h"
#include "preprocess_shared_dataset.h"
#include "preprocess_split.h"
#include "preprocess_stitch.h"
#include "preprocess_util.h"

namespace preprocess {
template <typename T>
    requires GdalType<T> && Copy<T> && PartialEq<T> && NumCast<T>
PreprocessResult<TArray<FTileCoordinate>> split_and_stitch(
    const TMap<uint32, FaceInfo>& faces,
    const FPreprocessContext& context
) {
    using ext::types::isize, ext::types::usize;
    auto input_tiles = TArray<FTileCoordinate>{};
    auto datasets = TMap<uint32, SharedDatasetRO>{};
    for (const auto& [face, info] : faces) {
        datasets.Emplace(face, SharedDatasetRO{info.path});
        const auto xy_start = info.pixel_start / static_cast<int32>(context.
            attachment.center_size());
        const auto xy_end = (info.pixel_end - 1) / static_cast<int32>(context.
            attachment.center_size());

        input_tiles.Append(
            ext::iter::map<TTuple<isize, isize>, FTileCoordinate>(
                ext::iter::product<isize>(
                    TArray{xy_start.Get<0>(), xy_end.Get<0>()},
                    TArray{xy_start.Get<1>(), xy_end.Get<1>()}
                ),
                [&face, &info](TTuple<isize, isize> xy) {
                    return FTileCoordinate{
                        face,
                        info.lod,
                        {
                            static_cast<int32>(xy.Get<0>()),
                            static_cast<int32>(xy.Get<1>())
                        }
                    };
                }));
    }

    const PreprocessResult<TArray<FTileCoordinate>> output_tiles_result =
        split<T>(input_tiles, faces, datasets, context);

    if (!output_tiles_result.has_value()) { return std::unexpected{output_tiles_result.error()}; }

    auto output_tiles = output_tiles_result.value();
    const auto stitch_result = stitch<T>(output_tiles, context);
    if (!stitch_result.has_value()) { return std::unexpected{stitch_result.error()}; }
    return output_tiles;
}

template <typename T>
    requires GdalType<T> && Copy<T> && PartialEq<T> && NumCast<T>
PreprocessResult<TArray<FTileCoordinate>> downsample_and_stitch(
    const TArray<FTileCoordinate>& input_tiles,
    const FPreprocessContext& context
) {
    const auto tiles_to_downsample = compute_tiles_to_downsample(input_tiles);
    auto output_tiles = input_tiles;
    output_tiles.Append(ext::iter::flatten(tiles_to_downsample));
    for (const auto& tiles : tiles_to_downsample) {
        auto downsample_result = downsample<T>(tiles, context);
        if (!downsample_result.has_value()) { return std::unexpected{downsample_result.error()}; }
        auto stitch_result = stitch<T>(tiles, context);
        if (!stitch_result.has_value()) { return std::unexpected{stitch_result.error()}; }
    }

    return output_tiles;
}

template <typename T>
    requires GdalType<T> && Copy<T> && PartialEq<T> && NumCast<T>
void preprocess_gen(GDALDatasetRef src_dataset, FPreprocessContext& context) {
    if (context.overwrite) { util::clear_directory(context.tile_dir); }
    util::clear_directory(context.temp_dir);

    // const auto is_planar = true;
    PreprocessResult<TMap<uint32, FaceInfo>> faces_result =
        reproject_planar<T>(MoveTemp(src_dataset), context);
    if (!faces_result.has_value()) {
        checkf(
            false,
            TEXT("Failed to reproject planar: %s"),
            *faces_result.error().ToString());
        return;
    }

    const TMap<uint32, FaceInfo> faces = MoveTemp(faces_result.value());
    const PreprocessResult<TArray<FTileCoordinate>> split_tiles_result =
        split_and_stitch<T>(faces, context);
    if (!split_tiles_result.has_value()) {
        checkf(
            false,
            TEXT("Failed to split and stitch: %s"),
            *split_tiles_result.error().ToString());
        return;
    }
    const TArray<FTileCoordinate> split_tiles = split_tiles_result.value();
    PreprocessResult<TArray<FTileCoordinate>> tiles_result =
        downsample_and_stitch<T>(split_tiles, context);
    if (!tiles_result.has_value()) {
        checkf(
            false,
            TEXT("Failed to downsample and stitch: %s"),
            *tiles_result.error().ToString());
        return;
    }
    const auto tiles = MoveTemp(tiles_result.value());
    auto create_mask_result = create_mask_and_fill_no_data(tiles, context);
    if (!create_mask_result.has_value()) {
        checkf(
            false,
            TEXT("Failed to create mask and fill no data: %s"),
            *create_mask_result.error().ToString());
        return;
    }

    util::delete_directory(context.temp_dir);
    util::save_terrain_config(tiles, context);
}

#define PREPROCESS_CASE(type) return preprocess_gen<type>(MoveTemp(src_dataset), context);

inline void preprocess(
    GDALDatasetRef src_dataset,
    FPreprocessContext& context
) {
    const auto data_type = util::transpose_gdal_datatype(context);

    switch (data_type) {
    case GDT_Byte: PREPROCESS_CASE(uint8)
    case GDT_UInt16: PREPROCESS_CASE(uint16)
    case GDT_Int16: PREPROCESS_CASE(int16)
    case GDT_UInt32: PREPROCESS_CASE(uint32)
    case GDT_Int32: PREPROCESS_CASE(int32)
    case GDT_Float32: PREPROCESS_CASE(float)
    case GDT_Float64: PREPROCESS_CASE(double)
    default: checkf(false, TEXT("Unsupported data type: %d"), data_type);
    }
}

inline TOptional<double> resolve_no_data_value(
    const GDALDatasetRef& dataset,
    TArray<FRasterbandConfig>& rasterbands,
    const FPreprocessNoData no_data
) {
    switch (no_data.Type) {
    case EPreprocessNoData::Source: {
        return util::get_no_data_value(dataset).Get(0.0);
    }
    case EPreprocessNoData::NoData: return no_data.NoDataValue;
    case EPreprocessNoData::Alpha: {
        rasterbands.Push(FRasterbandConfig{.color_interp = EGDALColorInterp::GCI_AlphaBand});
        return NullOpt;
    }
    }

    std::unreachable();
}

inline TTuple<
    TTuple<GDALDatasetRef, FPreprocessContext>,
    TTuple<GDALDatasetRef, FPreprocessContext>
> initialize(const FTerrainPreprocessSettings& settings) {
    const auto heightmap_attachment = FAttachmentConfig{
        .texture_size = settings.texture_size,
        .border_size = settings.border_size,
        .mip_level_count = settings.mip_level_count,
        .mask = settings.heightmap_create_mask,
        .format = settings.heightmap_attachment_format
    };

    const auto albedo_attachment = FAttachmentConfig{
        .texture_size = settings.texture_size,
        .border_size = settings.border_size,
        .mip_level_count = settings.mip_level_count,
        .mask = settings.albedo_create_mask,
        .format = settings.albedo_attachment_format
    };

    const auto heightmap_data_type = settings.heightmap_data_type.DataTypeValue;
    const auto albedo_data_type = settings.albedo_data_type.DataTypeValue;

    GDALDatasetRef heightmap_src_dataset{
        GDALDataset::Open(TCHAR_TO_UTF8(*settings.heightmap_src_path), GA_Update)
    };
    GDALDatasetRef albedo_src_dataset{
        GDALDataset::Open(TCHAR_TO_UTF8(*settings.albedo_src_path), GA_Update)
    };

    auto heightmap_rasterbands = ext::iter::map<std::expected<
        GDALRasterBand*, CPLErrorNum>, FRasterbandConfig>(
        rasterbands(heightmap_src_dataset),
        [](const std::expected<GDALRasterBand*, CPLErrorNum>& rb) -> FRasterbandConfig {
            return FRasterbandConfig{
                .color_interp = static_cast<EGDALColorInterp>(rb.value()->GetColorInterpretation())
            };
        });

    auto albedo_rasterbands = ext::iter::map<
        std::expected<GDALRasterBand*, CPLErrorNum>,
        FRasterbandConfig
    >(
        rasterbands(albedo_src_dataset),
        [](const std::expected<GDALRasterBand*, CPLErrorNum>& rb) -> FRasterbandConfig {
            return FRasterbandConfig{
                .color_interp = static_cast<EGDALColorInterp>(rb.value()->GetColorInterpretation())
            };
        });

    TOptional<double> heightmap_no_data_value = resolve_no_data_value(
        heightmap_src_dataset,
        heightmap_rasterbands,
        settings.heightmap_no_data);

    TOptional<double> albedo_no_data_value = resolve_no_data_value(
        albedo_src_dataset,
        albedo_rasterbands,
        settings.albedo_no_data);

    const auto heightmap_tile_dir = settings.terrain_path / settings.heightmap_attachment_label;
    const auto albedo_tile_dir = settings.terrain_path / settings.albedo_attachment_label;

    const auto temp_dir = settings.temp_path;

    const auto heightmap_context = FPreprocessContext{
        .data_type = heightmap_data_type,
        .no_data_value = heightmap_no_data_value,
        .rasterbands = heightmap_rasterbands,
        .tile_dir = heightmap_tile_dir,
        .temp_dir = temp_dir,
        .fill_radius = settings.fill_radius,
        .overwrite = settings.overwrite,
        .min_height = TNumericLimits<float>::Max(),
        .max_height = TNumericLimits<float>::Lowest(),
        .create_mask = settings.heightmap_create_mask,
        .attachment_label = settings.heightmap_attachment_label,
        .attachment = heightmap_attachment,
        .terrain_path = settings.terrain_path,
        .lod_count = settings.heightmap_lod_count
    };

    const auto albedo_context = FPreprocessContext{
        .data_type = albedo_data_type,
        .no_data_value = albedo_no_data_value,
        .rasterbands = albedo_rasterbands,
        .tile_dir = albedo_tile_dir,
        .temp_dir = temp_dir,
        .fill_radius = settings.fill_radius,
        .overwrite = settings.overwrite,
        .min_height = 0.0f,
        .max_height = 0.0f,
        .create_mask = settings.albedo_create_mask,
        .attachment_label = settings.albedo_attachment_label,
        .attachment = albedo_attachment,
        .terrain_path = settings.terrain_path,
        .lod_count = settings.albedo_lod_count,
    };

    return {
        TTuple<GDALDatasetRef, FPreprocessContext>{
            MoveTemp(heightmap_src_dataset),
            heightmap_context
        },
        TTuple<GDALDatasetRef, FPreprocessContext>{
            MoveTemp(albedo_src_dataset),
            albedo_context
        }
    };
}
} // namespace preprocess
