#pragma once
#include <expected>

#include "ext_traits.h"
#include "gdal.h"
#include "preprocess_attachment_config.h"
#include "preprocess_gdal_extended.h"
#include "preprocess_result.h"
#include "preprocess_tile_coordinate.h"
#include "SmartPointers.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"

#include "preprocess_dataset.generated.h"

UENUM(BlueprintType)
enum class EGDALColorInterp : uint8 {
    /*! Undefined */ GCI_Undefined = 0,
    /*! Greyscale */ GCI_GrayIndex = 1,
    /*! Paletted (see associated color table) */ GCI_PaletteIndex = 2,
    /*! Red band of RGBA image */ GCI_RedBand = 3,
    /*! Green band of RGBA image */ GCI_GreenBand = 4,
    /*! Blue band of RGBA image */ GCI_BlueBand = 5,
    /*! Alpha (0=transparent, 255=opaque) */ GCI_AlphaBand = 6,
    /*! Hue band of HLS image */ GCI_HueBand = 7,
    /*! Saturation band of HLS image */ GCI_SaturationBand = 8,
    /*! Lightness band of HLS image */ GCI_LightnessBand = 9,
    /*! Cyan band of CMYK image */ GCI_CyanBand = 10,
    /*! Magenta band of CMYK image */ GCI_MagentaBand = 11,
    /*! Yellow band of CMYK image */ GCI_YellowBand = 12,
    /*! Black band of CMYK image */ GCI_BlackBand = 13,
    /*! Y Luminance */ GCI_YCbCr_YBand = 14,
    /*! Cb Chroma */ GCI_YCbCr_CbBand = 15,
    /*! Cr Chroma */ GCI_YCbCr_CrBand = 16,
    /*! Max current value (equals to GCI_YCbCr_CrBand currently) */ GCI_Max = 16
};

USTRUCT(BlueprintType)
struct FRasterbandConfig {
    GENERATED_BODY()

    FRasterbandConfig() : color_interp(EGDALColorInterp::GCI_Undefined) {}

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EGDALColorInterp color_interp;
};

USTRUCT(BlueprintType)
struct FPreprocessContext {
    GENERATED_BODY()

    FPreprocessContext() : data_type(static_cast<EGDALDataType>(GDT_Unknown)),
        no_data_value(NullOpt),
        fill_radius(0.0f),
        overwrite(false),
        min_height(0.0f),
        max_height(0.0f),
        create_mask(false),
        lod_count(NullOpt) {}

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EGDALDataType data_type;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TOptional<double> no_data_value;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TArray<FRasterbandConfig> rasterbands;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString tile_dir;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString temp_dir;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float fill_radius;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool overwrite;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float min_height;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    float max_height;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool create_mask;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString attachment_label;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FAttachmentConfig attachment;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FString terrain_path;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    TOptional<int32> lod_count;
};

namespace preprocess {
struct FaceInfo {
    uint32 lod;
    isize_c pixel_start;
    isize_c pixel_end;
    FString path;
};

template <typename T>
    requires GdalType<T> && Copy<T>
PreprocessResult<GDALDatasetRef> create_empty_dataset(
    const FString& dst_path,
    const FUint64Point size,
    TOptional<GeoTransformRef> transform,
    const FPreprocessContext& context
) {
    const auto driver = GDALDriver::FromHandle(GDALGetDriverByName("GTiff"));

    const char* fmt = [&context] {
        switch (context.attachment.format) {
        case EAttachmentFormat::R16U:
        case EAttachmentFormat::R32F: return "PHOTOMETRIC=MINISBLACK";
        case EAttachmentFormat::Rgba8U: return "PHOTOMETRIC=RGB";
        default: return "";
        }
    }();
    TArray options{
        "TILED=YES",
        "BLOCKXSIZE=512",
        "BLOCKYSIZE=512",
        "INTERLEAVE=PIXEL"
    };
    if (fmt[0] != '\0') { options.Add(fmt); }
    options.Add(nullptr);

    std::expected<GDALDatasetRef, CPLErrorNum> dst_result = create_with_band_type_with_options<T>(
        driver,
        dst_path,
        size.X,
        size.Y,
        context.rasterbands.Num(),
        options.GetData()
    );

    if (!dst_result.has_value()) {
        return std::unexpected{FPreprocessError::Gdal(dst_result.error())};
    }

    GDALDatasetRef dst = MoveTemp(dst_result.value());

    if (transform.IsSet()) {
        const auto err = dst->SetGeoTransform(transform->Get());
        if (err != CE_None) { return handle_gdal_err<GDALDatasetRef>(); }
    }

    for (uint32 i = 0; i < static_cast<uint32>(context.rasterbands.Num()); ++i) {
        auto [color_interp] = context.rasterbands[i];
        const auto dst_band = dst->GetRasterBand(i + 1);
        if (dst_band == nullptr) { return handle_gdal_err<GDALDatasetRef>(); }
        if (context.no_data_value.IsSet()) {
            const auto err = dst_band->SetNoDataValue(*context.no_data_value);
            if (err != CE_None) { return handle_gdal_err<GDALDatasetRef>(); }
        }

        const auto err2 = dst_band->SetColorInterpretation(
            static_cast<GDALColorInterp>(color_interp));
        if (err2 != CE_None) { return handle_gdal_err<GDALDatasetRef>(); }
    }

    return dst;
}

template <typename T>
    requires ext::traits::GdalType<T> && ext::traits::Copy<T>
PreprocessResult<GDALDatasetRef> create_tile_dataset(
    const FTileCoordinate tile_coordinate,
    const FPreprocessContext& context
) {
    const auto tile_path = tile_coordinate.path(context.tile_dir);
    const auto parent_dir = FPaths::GetPath(tile_path);
    IFileManager::Get().MakeDirectory(*parent_dir, true);

    const uint64 texture_size = static_cast<uint64>(context.attachment.texture_size);

    return create_empty_dataset<T>(
        tile_path,
        FUint64Point{texture_size, texture_size},
        NullOpt,
        context
    );
}

inline GeoTransformRef geo_transform(const GDALDatasetRef& dataset) {
    auto transformation = GeoTransformRef{new double[6]};
    const auto rv = dataset->GetGeoTransform(transformation.Get());

    checkf(
        rv == CE_None,
        TEXT("Failed to get geotransform from dataset with error (%d): %hs"),
        CPLGetLastErrorNo(),
        CPLGetLastErrorMsg());

    return transformation;
}

inline PreprocessResult<GDALDatasetRef> update_tile_dataset(
    const FTileCoordinate tile_coordinate,
    const FPreprocessContext& context
) {
    const auto p = tile_coordinate.path(context.tile_dir);
    GDALDataset* dataset = GDALDataset::Open(
        TCHAR_TO_UTF8(*p),
        GDAL_OF_UPDATE | GDAL_OF_RASTER);

    if (!dataset) { return handle_gdal_err<GDALDatasetRef>(); }

    return PreprocessResult<GDALDatasetRef>{dataset};
}

inline PreprocessResult<TOptional<GDALDatasetRef>> load_tile_dataset_if_exists(
    const FTileCoordinate tile_coordinate,
    const FPreprocessContext& context
) {
    const auto tile_path = tile_coordinate.path(context.tile_dir);
    if (!FPaths::FileExists(tile_path)) { return NullOpt; }

    GDALDataset* dataset = GDALDataset::Open(
        TCHAR_TO_UTF8(*tile_path),
        GDAL_OF_RASTER);

    if (!dataset) { return handle_gdal_err<TOptional<GDALDatasetRef>>(); }

    return GDALDatasetRef{dataset};
}
}
