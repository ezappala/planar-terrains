#pragma once

#include "preprocess_dataset.h"
#include "preprocess_tile_coordinate.h"
#include "terrain_config.h"
#include "HAL/FileManager.h"

namespace preprocess::util {
inline void clear_directory(const FString& string) {
    IFileManager& file_manager = IFileManager::Get();
    file_manager.DeleteDirectory(*string, false, true);
    file_manager.MakeDirectory(*string, true);
}

inline void delete_directory(const FString& string) {
    IFileManager& file_manager = IFileManager::Get();
    file_manager.DeleteDirectory(*string, false, true);
}

inline void save_terrain_config(
    const TArray<FTileCoordinate>& tiles,
    const FPreprocessContext& context
) {
    const auto file_path = FPaths::Combine(context.terrain_path, TEXT("config.json"));
    FTerrainConfig config{};
    if (IFileManager::Get().FileExists(*file_path)) {
        config = FTerrainConfig::from_file(file_path).Get(FTerrainConfig{});
    }
    config.side_length = 86400;
    config.path = context.terrain_path;
    config.add_attachment(context.attachment_label, context.attachment);
    if (context.attachment_label.ToLower() == "height") {
        config.min_height = context.min_height;
        config.max_height = context.max_height;
        config.tiles = tiles;
        config.lod_count = context.lod_count.GetValue();
    }

    config.save_file(file_path);
}

inline ext::GDALDataType::GDALDataTypeEnum transpose_gdal_datatype(
    const FPreprocessContext& context
) { return static_cast<ext::GDALDataType::GDALDataTypeEnum>(context.data_type); }

inline TOptional<double> get_no_data_value(const GDALDatasetRef& dataset) {
    const auto band = dataset->GetRasterBand(1);
    int has_no_data_value = 0;
    const auto no_data_value = band->GetNoDataValue(&has_no_data_value);
    if (!has_no_data_value) { return NullOpt; }

    return no_data_value;
}
}
