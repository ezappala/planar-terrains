#pragma once
#include "preprocess_downsample.h"
#include "preprocess_fill.h"
#include "preprocess_progress.h"
#include "preprocess_reproject.h"
#include "preprocess_shared_dataset.h"
#include "preprocess_split.h"
#include "preprocess_stitch.h"
#include "preprocess_util.h"
#include "terrain_settings.h"

namespace preprocess {
inline PreprocessResult<void> check_canceled(const IPreprocessProgress* progress) {
    if (progress != nullptr && progress->should_cancel()) {
        return std::unexpected{FPreprocessError::Canceled()};
    }
    return {};
}

template <typename T>
    requires GdalType<T> && Copy<T> && PartialEq<T> && NumCast<T>
PreprocessResult<TArray<FTileCoordinate>> split_and_stitch(
    const TMap<uint32, FaceInfo>& faces,
    const FPreprocessContext& context,
    IPreprocessProgress* progress = nullptr
) {
    FScopedPreprocessProgress _(
        progress,
        2.0f,
        FText::Format(
            NSLOCTEXT("UDLODPreprocessor", "SplitAndStitch", "Splitting {0} tiles"),
            FText::FromString(context.attachment_label)));

    using ext::types::isize, ext::types::usize;
    auto input_tiles = TArray<FTileCoordinate>{};
    auto datasets = TMap<uint32, SharedDatasetRO>{};
    for (const auto& [face, info] : faces) {
        datasets.Emplace(face, SharedDatasetRO{info.path});
        const auto xy_start = info.pixel_start / static_cast<int32>(context.
            attachment.center_size());
        const auto xy_end = (info.pixel_end - 1) / static_cast<int32>(context.
            attachment.center_size());

        for (isize x = xy_start.Get<0>(); x <= xy_end.Get<0>(); ++x) {
            for (isize y = xy_start.Get<1>(); y <= xy_end.Get<1>(); ++y) {
                input_tiles.Emplace(
                    FTileCoordinate{
                        face,
                        info.lod,
                        {
                            static_cast<int32>(x),
                            static_cast<int32>(y)
                        }
                    });
            }
        }
    }

    if (progress != nullptr) {
        progress->enter_progress_frame(
            1.0f,
            NSLOCTEXT("UDLODPreprocessor", "SplitTiles", "Splitting tiles"));
    }
    const PreprocessResult<TArray<FTileCoordinate>> output_tiles_result =
        split<T>(input_tiles, faces, datasets, context);

    if (!output_tiles_result.has_value()) { return std::unexpected{output_tiles_result.error()}; }
    if (const auto canceled = check_canceled(progress); !canceled.has_value()) {
        return std::unexpected{canceled.error()};
    }

    auto output_tiles = output_tiles_result.value();
    if (progress != nullptr) {
        progress->enter_progress_frame(
            1.0f,
            NSLOCTEXT("UDLODPreprocessor", "StitchTiles", "Stitching tile borders"));
    }
    const auto stitch_result = stitch<T>(output_tiles, context);
    if (!stitch_result.has_value()) { return std::unexpected{stitch_result.error()}; }
    return output_tiles;
}

template <typename T>
    requires GdalType<T> && Copy<T> && PartialEq<T> && NumCast<T>
PreprocessResult<TArray<FTileCoordinate>> downsample_and_stitch(
    const TArray<FTileCoordinate>& input_tiles,
    const FPreprocessContext& context,
    IPreprocessProgress* progress = nullptr
) {
    const auto tiles_to_downsample = compute_tiles_to_downsample(input_tiles);
    auto output_tiles = input_tiles;
    output_tiles.Append(ext::iter::flatten(tiles_to_downsample));

    FScopedPreprocessProgress _(
        progress,
        static_cast<float>(FMath::Max(1, tiles_to_downsample.Num() * 2)),
        FText::Format(
            NSLOCTEXT("UDLODPreprocessor", "DownsampleAndStitch", "Downsampling {0}"),
            FText::FromString(context.attachment_label)));

    if (tiles_to_downsample.IsEmpty() && progress != nullptr) {
        progress->enter_progress_frame(
            1.0f,
            NSLOCTEXT("UDLODPreprocessor", "NoDownsampleWork", "No downsample work required"));
    }

    int32 lod_group_index = 0;
    for (const auto& tiles : tiles_to_downsample) {
        if (progress != nullptr) {
            progress->enter_progress_frame(
                1.0f,
                FText::Format(
                    NSLOCTEXT(
                        "UDLODPreprocessor",
                        "DownsampleLodGroup",
                        "Downsampling LOD group {0}"),
                    FText::AsNumber(lod_group_index)));
        }
        auto downsample_result = downsample<T>(tiles, context);
        if (!downsample_result.has_value()) { return std::unexpected{downsample_result.error()}; }
        if (const auto canceled = check_canceled(progress); !canceled.has_value()) {
            return std::unexpected{canceled.error()};
        }

        if (progress != nullptr) {
            progress->enter_progress_frame(
                1.0f,
                FText::Format(
                    NSLOCTEXT("UDLODPreprocessor", "StitchLodGroup", "Stitching LOD group {0}"),
                    FText::AsNumber(lod_group_index)));
        }
        auto stitch_result = stitch<T>(tiles, context);
        if (!stitch_result.has_value()) { return std::unexpected{stitch_result.error()}; }
        ++lod_group_index;
    }

    return output_tiles;
}

template <typename T>
    requires GdalType<T> && Copy<T> && PartialEq<T> && NumCast<T>
PreprocessResult<void> preprocess_gen(
    GDALDatasetRef src_dataset,
    FPreprocessContext& context,
    IPreprocessProgress* progress = nullptr
) {
    FScopedPreprocessProgress _(
        progress,
        5.0f,
        FText::Format(
            NSLOCTEXT("UDLODPreprocessor", "PreprocessAttachment", "Preprocessing {0}"),
            FText::FromString(context.attachment_label)));

    if (progress != nullptr) {
        progress->enter_progress_frame(
            1.0f,
            NSLOCTEXT("UDLODPreprocessor", "PrepareDirectories", "Preparing output directories"));
    }
    if (context.overwrite) { util::clear_directory(context.tile_dir); }
    util::clear_directory(context.temp_dir);
    if (const auto canceled = check_canceled(progress); !canceled.has_value()) {
        return std::unexpected{canceled.error()};
    }

    if (progress != nullptr) {
        progress->enter_progress_frame(
            1.0f,
            NSLOCTEXT("UDLODPreprocessor", "ReprojectPlanar", "Reprojecting source imagery"));
    }
    PreprocessResult<TMap<uint32, FaceInfo>> faces_result =
        reproject_planar<T>(MoveTemp(src_dataset), context);
    if (!faces_result.has_value()) { return std::unexpected{faces_result.error()}; }
    if (const auto canceled = check_canceled(progress); !canceled.has_value()) {
        return std::unexpected{canceled.error()};
    }

    const TMap<uint32, FaceInfo> faces = MoveTemp(faces_result.value());
    if (progress != nullptr) {
        progress->enter_progress_frame(
            1.0f,
            NSLOCTEXT("UDLODPreprocessor", "SplitAndStitchFrame", "Splitting and stitching"));
    }
    const PreprocessResult<TArray<FTileCoordinate>> split_tiles_result = split_and_stitch<T>(
        faces,
        context,
        progress);
    if (!split_tiles_result.has_value()) { return std::unexpected{split_tiles_result.error()}; }
    const TArray<FTileCoordinate> split_tiles = split_tiles_result.value();

    if (progress != nullptr) {
        progress->enter_progress_frame(
            1.0f,
            NSLOCTEXT("UDLODPreprocessor", "DownsampleFrame", "Downsampling terrain tiles"));
    }
    PreprocessResult<TArray<FTileCoordinate>> tiles_result = downsample_and_stitch<T>(
        split_tiles,
        context,
        progress);
    if (!tiles_result.has_value()) { return std::unexpected{tiles_result.error()}; }
    const auto tiles = MoveTemp(tiles_result.value());
    if (const auto canceled = check_canceled(progress); !canceled.has_value()) {
        return std::unexpected{canceled.error()};
    }

    if (progress != nullptr) {
        progress->enter_progress_frame(
            1.0f,
            NSLOCTEXT("UDLODPreprocessor", "FinalizeTiles", "Finalizing terrain tiles"));
    }
    auto create_mask_result = create_mask_and_fill_no_data(tiles, context);
    if (!create_mask_result.has_value()) { return std::unexpected{create_mask_result.error()}; }

    util::delete_directory(context.temp_dir);
    util::save_terrain_config(tiles, context);
    return {};
}

#define PREPROCESS_CASE(type) return preprocess_gen<type>(MoveTemp(src_dataset), context, progress);

inline PreprocessResult<void> preprocess(
    GDALDatasetRef src_dataset,
    FPreprocessContext& context,
    IPreprocessProgress* progress = nullptr
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
    default: return std::unexpected{FPreprocessError::UnknownRasterbandDataType()};
    }
}

inline TOptional<double> resolve_no_data_value(
    const GDALDatasetRef& dataset,
    TArray<FRasterbandConfig>& rasterbands,
    const FPreprocessNoData no_data
) {
    switch (no_data.Type) {
    case EPreprocessNoData::Source: return util::get_no_data_value(dataset);
    case EPreprocessNoData::NoData: return no_data.NoDataValue;
    case EPreprocessNoData::Alpha: {
        FRasterbandConfig alpha_band;
        alpha_band.color_interp = EGDALColorInterp::GCI_AlphaBand;
        rasterbands.Push(alpha_band);
        return NullOpt;
    }
    }

    std::unreachable();
}

inline EGDALDataType resolve_data_type(
    const GDALDatasetRef& dataset,
    const FPreprocessDataType& data_type
) {
    switch (data_type.Type) {
    case EPreprocessDataType::Source: {
        GDALRasterBand* band = dataset->GetRasterBand(1);
        if (band == nullptr) { return static_cast<EGDALDataType>(GDT_Unknown); }
        return static_cast<EGDALDataType>(band->GetRasterDataType());
    }
    case EPreprocessDataType::DataType: return data_type.DataTypeValue;
    }

    std::unreachable();
}

inline PreprocessResult<TTuple<
    TTuple<GDALDatasetRef, FPreprocessContext>,
    TTuple<GDALDatasetRef, FPreprocessContext>
>> initialize(const FTerrainPreprocessSettings& settings) {
    FAttachmentConfig heightmap_attachment{};
    heightmap_attachment.texture_size = settings.texture_size;
    heightmap_attachment.border_size = settings.border_size;
    heightmap_attachment.mip_level_count = settings.mip_level_count;
    heightmap_attachment.mask = settings.heightmap_create_mask;
    heightmap_attachment.format = settings.heightmap_attachment_format;

    FAttachmentConfig albedo_attachment{};
    albedo_attachment.texture_size = settings.texture_size;
    albedo_attachment.border_size = settings.border_size;
    albedo_attachment.mip_level_count = settings.mip_level_count;
    albedo_attachment.mask = settings.albedo_create_mask;
    albedo_attachment.format = settings.albedo_attachment_format;

    GDALDatasetRef heightmap_src_dataset{
        GDALDataset::Open(TCHAR_TO_UTF8(*settings.heightmap_src_path), GA_ReadOnly)
    };
    GDALDatasetRef albedo_src_dataset{
        GDALDataset::Open(TCHAR_TO_UTF8(*settings.albedo_src_path), GA_ReadOnly)
    };
    if (!heightmap_src_dataset) {
        return make_parse_error_result<TTuple<
            TTuple<GDALDatasetRef, FPreprocessContext>,
            TTuple<GDALDatasetRef, FPreprocessContext>
        >>(
            FString::Printf(
                TEXT("Could not open heightmap source: %s"),
                *settings.heightmap_src_path));
    }
    if (!albedo_src_dataset) {
        return make_parse_error_result<TTuple<
            TTuple<GDALDatasetRef, FPreprocessContext>,
            TTuple<GDALDatasetRef, FPreprocessContext>
        >>(
            FString::Printf(TEXT("Could not open albedo source: %s"), *settings.albedo_src_path));
    }

    const auto heightmap_data_type = resolve_data_type(
        heightmap_src_dataset,
        settings.heightmap_data_type);
    const auto albedo_data_type = resolve_data_type(
        albedo_src_dataset,
        settings.albedo_data_type);

    auto heightmap_rasterbands_arr = rasterbands(heightmap_src_dataset);
    auto heightmap_rasterbands = ext::iter::map<std::expected<GDALRasterBand*, CPLErrorNum>>(
        heightmap_rasterbands_arr,
        [](const std::expected<GDALRasterBand*, CPLErrorNum>& rb) -> FRasterbandConfig {
            FRasterbandConfig band;
            band.color_interp = static_cast<EGDALColorInterp>(
                rb.value()->GetColorInterpretation());
            return band;
        });

    auto albedo_rasterbands_arr = rasterbands(albedo_src_dataset);
    auto albedo_rasterbands = ext::iter::map<std::expected<GDALRasterBand*, CPLErrorNum>>(
        albedo_rasterbands_arr,
        [](const std::expected<GDALRasterBand*, CPLErrorNum>& rb) -> FRasterbandConfig {
            FRasterbandConfig band;
            band.color_interp = static_cast<EGDALColorInterp>(
                rb.value()->GetColorInterpretation());
            return band;
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

    const auto heightmap_temp_dir = settings.temp_path / settings.heightmap_attachment_label;
    const auto albedo_temp_dir = settings.temp_path / settings.albedo_attachment_label;

    FPreprocessContext heightmap_context{};
    heightmap_context.data_type = heightmap_data_type;
    heightmap_context.no_data_value = heightmap_no_data_value;
    heightmap_context.rasterbands = heightmap_rasterbands;
    heightmap_context.tile_dir = heightmap_tile_dir;
    heightmap_context.temp_dir = heightmap_temp_dir;
    heightmap_context.fill_radius = settings.fill_radius;
    heightmap_context.overwrite = settings.overwrite;
    heightmap_context.min_height = TNumericLimits<float>::Max();
    heightmap_context.max_height = TNumericLimits<float>::Lowest();
    heightmap_context.create_mask = settings.heightmap_create_mask;
    heightmap_context.attachment_label = settings.heightmap_attachment_label;
    heightmap_context.attachment = heightmap_attachment;
    heightmap_context.terrain_path = settings.terrain_path;
    heightmap_context.lod_count = settings.heightmap_lod_count;

    FPreprocessContext albedo_context{};
    albedo_context.data_type = albedo_data_type;
    albedo_context.no_data_value = albedo_no_data_value;
    albedo_context.rasterbands = albedo_rasterbands;
    albedo_context.tile_dir = albedo_tile_dir;
    albedo_context.temp_dir = albedo_temp_dir;
    albedo_context.fill_radius = settings.fill_radius;
    albedo_context.overwrite = settings.overwrite;
    albedo_context.min_height = 0.0f;
    albedo_context.max_height = 0.0f;
    albedo_context.create_mask = settings.albedo_create_mask;
    albedo_context.attachment_label = settings.albedo_attachment_label;
    albedo_context.attachment = albedo_attachment;
    albedo_context.terrain_path = settings.terrain_path;
    albedo_context.lod_count = settings.albedo_lod_count;

    return MakeTuple(
        MakeTuple(MoveTemp(heightmap_src_dataset), heightmap_context),
        MakeTuple(MoveTemp(albedo_src_dataset), albedo_context));
}
} // namespace preprocess
