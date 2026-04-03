#pragma once

#include "preprocess_dataset.h"
#include "preprocess_gdal_extended.h"
#include "terrain_tile_atlas.h"
#include "Misc/Paths.h"

namespace terrain::tile_loader {
namespace detail {
inline FString tile_path(const FAttachment& attachment, const FAttachmentTile& tile) {
    return tile.coordinate.path(FPaths::Combine(attachment.path, tile.label));
}

template <typename T>
FAttachmentTileData make_tile_data_variant(TArray<T> data) {
    FAttachmentTileData result;
    result.Emplace<TArray<T>>(MoveTemp(data));
    return result;
}

inline FAttachmentTileData make_zero_tile_data(const FAttachment& attachment) {
    const int32 pixel_count = static_cast<int32>(attachment.texture_size * attachment.texture_size);

    switch (attachment.attachment_format) {
    case EAttachmentFormat::Rgb8U:
    case EAttachmentFormat::Rgba8U: {
        TArray<TStaticArray<uint8, 4>> data;
        data.Init(TStaticArray<uint8, 4>{0u, 0u, 0u, 255u}, pixel_count);
        return make_tile_data_variant(MoveTemp(data));
    }
    case EAttachmentFormat::R16U: {
        TArray<uint16> data;
        data.Init(0u, pixel_count);
        return make_tile_data_variant(MoveTemp(data));
    }
    case EAttachmentFormat::R16I: {
        TArray<int16> data;
        data.Init(0, pixel_count);
        return make_tile_data_variant(MoveTemp(data));
    }
    case EAttachmentFormat::Rg16U: {
        TArray<TStaticArray<uint16, 2>> data;
        data.Init(TStaticArray<uint16, 2>{0u, 0u}, pixel_count);
        return make_tile_data_variant(MoveTemp(data));
    }
    case EAttachmentFormat::R32F: {
        TArray<float> data;
        data.Init(0.0f, pixel_count);
        return make_tile_data_variant(MoveTemp(data));
    }
    }

    checkNoEntry();
    return make_tile_data_variant(TArray<float>{});
}

template <typename T>
TOptional<TArray<T>> try_read_band(GDALRasterBand* band) {
    if (band == nullptr) { return NullOpt; }

    const auto read_result = preprocess::read_band_as<T>(band);
    if (!read_result.has_value()) { return NullOpt; }

    return read_result->data();
}

inline FAttachmentTileData load_color_tile_data(
    const FAttachment& attachment,
    const TArray<GDALRasterBand*>& bands,
    const int32 pixel_count
) {
    const TOptional<TArray<uint8>> r = bands.Num() > 0
        ? try_read_band<uint8>(bands[0])
        : NullOpt;
    const TOptional<TArray<uint8>> g = bands.Num() > 1
        ? try_read_band<uint8>(bands[1])
        : NullOpt;
    const TOptional<TArray<uint8>> b = bands.Num() > 2
        ? try_read_band<uint8>(bands[2])
        : NullOpt;
    const TOptional<TArray<uint8>> a = bands.Num() > 3
        ? try_read_band<uint8>(bands[3])
        : NullOpt;
    if (!r.IsSet() || !g.IsSet() || !b.IsSet() || r->Num() != pixel_count || g->Num() !=
        pixel_count || b->Num() != pixel_count) { return make_zero_tile_data(attachment); }

    const bool bHasAlpha = a.IsSet() && a->Num() == pixel_count;
    TArray<TStaticArray<uint8, 4>> data;
    data.SetNum(pixel_count);
    for (int32 index = 0; index < pixel_count; ++index) {
        data[index] = TStaticArray<uint8, 4>{
            (*r)[index],
            (*g)[index],
            (*b)[index],
            bHasAlpha ? (*a)[index] : 255u
        };
    }
    return make_tile_data_variant(MoveTemp(data));
}

inline FAttachmentTileData load_tile_data(
    const FAttachment& attachment,
    const FAttachmentTile& tile
) {
    const FString path = tile_path(attachment, tile);
    GDALDataset* dataset_handle = GDALDataset::Open(TCHAR_TO_UTF8(*path), GDAL_OF_RASTER);
    if (dataset_handle == nullptr) {
        UE_LOGFMT(
            LogTemp,
            Warning,
            "[UDLODTerrain] Missing or unreadable tile {Path}; using zeros",
            path
        );
        return make_zero_tile_data(attachment);
    }

    GDALDatasetRef dataset{dataset_handle};
    const auto bands_result = preprocess::try_collect_rasterbands(dataset);
    if (!bands_result.has_value()) {
        UE_LOGFMT(
            LogTemp,
            Warning,
            "[UDLODTerrain] Failed collecting raster bands for {Path}; using zeros",
            path
        );
        return make_zero_tile_data(attachment);
    }

    const TArray<GDALRasterBand*>& bands = bands_result.value();
    const int32 pixel_count = static_cast<int32>(attachment.texture_size * attachment.texture_size);

    switch (attachment.attachment_format) {
    case EAttachmentFormat::Rgb8U:
    case EAttachmentFormat::Rgba8U: return load_color_tile_data(attachment, bands, pixel_count);
    case EAttachmentFormat::R16U: {
        const TOptional<TArray<uint16>> band = bands.Num() > 0
            ? try_read_band<uint16>(bands[0])
            : NullOpt;
        if (!band.IsSet() || band->Num() != pixel_count) { return make_zero_tile_data(attachment); }
        return make_tile_data_variant(*band);
    }
    case EAttachmentFormat::R16I: {
        const TOptional<TArray<int16>> band = bands.Num() > 0
            ? try_read_band<int16>(bands[0])
            : NullOpt;
        if (!band.IsSet() || band->Num() != pixel_count) { return make_zero_tile_data(attachment); }
        return make_tile_data_variant(*band);
    }
    case EAttachmentFormat::Rg16U: {
        const TOptional<TArray<uint16>> r = bands.Num() > 0
            ? try_read_band<uint16>(bands[0])
            : NullOpt;
        const TOptional<TArray<uint16>> g = bands.Num() > 1
            ? try_read_band<uint16>(bands[1])
            : NullOpt;
        if (!r.IsSet() || !g.IsSet() || r->Num() != pixel_count || g->Num() != pixel_count) {
            return make_zero_tile_data(attachment);
        }

        TArray<TStaticArray<uint16, 2>> data;
        data.SetNum(pixel_count);
        for (int32 index = 0; index < pixel_count; ++index) {
            data[index] = TStaticArray<uint16, 2>{(*r)[index], (*g)[index]};
        }
        return make_tile_data_variant(MoveTemp(data));
    }
    case EAttachmentFormat::R32F: {
        const TOptional<TArray<float>> band = bands.Num() > 0
            ? try_read_band<float>(bands[0])
            : NullOpt;
        if (!band.IsSet() || band->Num() != pixel_count) { return make_zero_tile_data(attachment); }
        return make_tile_data_variant(*band);
    }
    }

    checkNoEntry();
    return make_zero_tile_data(attachment);
}
}

void pump_tile_loads(FTileAtlas* tile_atlas);
}
