#pragma once
#include "preprocess_attachment_config.h"
#include "preprocess_tile_coordinate.h"
#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/FileHelper.h"
#include "Serialization/JsonSerializer.h"

struct FTerrainConfig {
    FString path = "";
    uint32 lod_count = 1;
    float min_height = 0.;
    float max_height = 1.;
    TMap<FString, FAttachmentConfig> attachments{};
    TArray<FTileCoordinate> tiles{};
    double side_length = 86400.0;
    uint32 face_count = 1;
    double scale_scalar = side_length / 2.;
    double face_size = 2. * PI / 4. * scale_scalar;

    void refresh_derived_fields() {
        scale_scalar = side_length / 2.;
        face_size = 2. * PI / 4. * scale_scalar;
    }

    FTerrainConfig& add_attachment(
        const FString& label,
        const FAttachmentConfig config
    ) {
        attachments.Add(label, FAttachmentConfig(config));
        return *this;
    }

    FString ToString(const bool truncated = true) const {
        FString attachments_str;
        for (const auto& [label, config] : attachments) {
            attachments_str += FString::Printf(
                TEXT("%s: {texture_size: %d, border_size: %d, mip_level_count: %d, mask: %s, format: %d}, "),
                *label,
                config.texture_size,
                config.border_size,
                config.mip_level_count,
                config.mask ? TEXT("true") : TEXT("false"),
                static_cast<int32>(config.format)
            );
        }

        FString tiles_str;
        if (truncated) {
            int faces = 0;
            int lods = 0;
            int x_range = 0;
            int y_range = 0;
            for (const auto& tile : tiles) {
                faces = FMath::Max(faces, tile.face);
                lods = FMath::Max(lods, tile.lod);
                x_range = FMath::Max(x_range, tile.xy.X);
                y_range = FMath::Max(y_range, tile.xy.Y);
            }
            tiles_str = FString::Printf(
                TEXT("face: [0, %d], lod: [0, %d], range: [0, %d]x[0, %d], count: %d"),
                faces,
                lods,
                x_range,
                y_range,
                tiles.Num()
            );
        } else {
            for (const auto& tile : tiles) {
                tiles_str += FString::Printf(
                    TEXT("{face: %d, lod: %d, xy: (%d, %d)}, "),
                    tile.face,
                    tile.lod,
                    tile.xy.X,
                    tile.xy.Y
                );
            }
        }

        return FString::Printf(
            TEXT("FTerrainConfig{path: %s, lod_count: %d, min_height: %f, max_height: %f, attachments: {%s}, tiles: [%s], side_length: %f, face_count: %d}"),
            *path,
            lod_count,
            min_height,
            max_height,
            *attachments_str,
            *tiles_str,
            side_length,
            face_count
        );
    }

    static TOptional<TSharedPtr<FJsonObject>> load_file(const FString& file_path) {
        IFileManager& file_manager = IFileManager::Get();
        if (!file_manager.FileExists(*file_path)) {
            UE_LOG(
                LogTemp,
                Error,
                TEXT("Terrain config file does not exist: %s"),
                *file_path);
            return NullOpt;
        }

        FString encoded;
        FFileHelper::LoadFileToString(encoded, *file_path);
        TSharedPtr<FJsonObject> json_object;
        const auto json_reader = TJsonReaderFactory<>::Create(encoded);
        FJsonSerializer::Deserialize(json_reader, json_object);
        if (json_object.IsValid()) { return json_object; }

        return NullOpt;
    }

    static TOptional<FTerrainConfig> from_file(const FString& file_path) {
        const auto json_object = load_file(file_path);
        if (json_object == NullOpt) { return NullOpt; }
        FTerrainConfig config = from_json(*json_object).Get(FTerrainConfig{});
        if (config.path.IsEmpty()) { config.path = FPaths::GetPath(file_path); }
        config.refresh_derived_fields();
        return config;
    }

    static TOptional<FTerrainConfig> from_json(const TSharedPtr<FJsonObject>& json_object) {
        if (!json_object.IsValid()) { return NullOpt; }
        FTerrainConfig config{};
        json_object->TryGetStringField(FString{"path"}, config.path);
        config.lod_count = json_object->GetIntegerField(FString{"lod_count"});
        config.min_height = json_object->GetNumberField(FString{"min_height"});
        config.max_height = json_object->GetNumberField(FString{"max_height"});
        json_object->TryGetNumberField(FString{"side_length"}, config.side_length);

        double face_count_value = config.face_count;
        if (json_object->TryGetNumberField(FString{"face_count"}, face_count_value)) {
            config.face_count = static_cast<uint32>(face_count_value);
        }

        const auto attachments_json = json_object->GetObjectField(
            FString{"attachments"});
        for (const auto& [label, value] : attachments_json->Values) {
            const auto config_json = value->AsObject();
            if (config_json == nullptr) { continue; }
            auto attachment_config = FAttachmentConfig{};
            attachment_config.texture_size = config_json->GetNumberField(FString{"texture_size"});
            attachment_config.border_size = config_json->GetNumberField(FString{"border_size"});
            attachment_config.mip_level_count = config_json->GetNumberField(
                FString{"mip_level_count"});
            attachment_config.mask = config_json->GetBoolField(FString{"mask"});
            attachment_config.format = static_cast<EAttachmentFormat>(
                config_json->GetIntegerField(FString{"format"}));
            config.attachments.Add(label, attachment_config);
        }

        const auto tiles_json = json_object->GetArrayField(FString{"tiles"});
        for (const auto& value : tiles_json) {
            const auto tile_object = value->AsObject();
            if (tile_object == nullptr) { continue; }
            auto tile = FTileCoordinate{};
            tile.face = tile_object->GetIntegerField(FString{"face"});
            tile.lod = tile_object->GetIntegerField(FString{"lod"});
            tile.xy.X = tile_object->GetIntegerField(FString{"xy_x"});
            tile.xy.Y = tile_object->GetIntegerField(FString{"xy_y"});
            config.tiles.Add(tile);
        }

        config.refresh_derived_fields();
        return config;
    }

    void save_file(const FString& file_path) const {
        const TSharedPtr<FJsonObject> json_object = MakeShared<FJsonObject>();
        json_object->SetStringField(FString{"path"}, path);
        json_object->SetNumberField(FString{"lod_count"}, lod_count);
        json_object->SetNumberField(FString{"min_height"}, min_height);
        json_object->SetNumberField(FString{"max_height"}, max_height);
        json_object->SetNumberField(FString{"side_length"}, side_length);
        json_object->SetNumberField(FString{"face_count"}, face_count);

        TSharedPtr<FJsonObject> attachments_json = MakeShared<FJsonObject>();
        for (const auto& [label, config] : attachments) {
            TSharedPtr<FJsonObject> config_json = MakeShared<FJsonObject>();
            config_json->SetNumberField(FString{"texture_size"}, config.texture_size);
            config_json->SetNumberField(FString{"border_size"}, config.border_size);
            config_json->SetNumberField(FString{"mip_level_count"}, config.mip_level_count);
            config_json->SetBoolField(FString{"mask"}, config.mask);
            config_json->SetNumberField(FString{"format"}, static_cast<int32>(config.format));
            attachments_json->Values.Add(label, MakeShared<FJsonValueObject>(config_json));
        }
        json_object->Values.Add(
            FString{"attachments"},
            MakeShared<FJsonValueObject>(attachments_json));

        TArray<TSharedPtr<FJsonValue>> tiles_json;
        for (const auto& tile : tiles) {
            TSharedPtr<FJsonObject> tile_object = MakeShared<FJsonObject>();
            tile_object->SetNumberField(FString{"face"}, tile.face);
            tile_object->SetNumberField(FString{"lod"}, tile.lod);
            tile_object->SetNumberField(FString{"xy_x"}, tile.xy.X);
            tile_object->SetNumberField(FString{"xy_y"}, tile.xy.Y);
            tiles_json.Add(MakeShared<FJsonValueObject>(tile_object));
        }
        json_object->Values.Add(FString{"tiles"}, MakeShared<FJsonValueArray>(tiles_json));

        FString encoded;
        const TSharedRef<TJsonWriter<>> json_writer = TJsonWriterFactory<>::Create(&encoded);
        if (FJsonSerializer::Serialize(json_object.ToSharedRef(), json_writer)) {
            FFileHelper::SaveStringToFile(encoded, *file_path);
        } else {
            UE_LOG(
                LogTemp,
                Error,
                TEXT("Failed to serialize terrain config to file: %s"),
                *file_path);
        }
    }
};
