#pragma once

#include "preprocess_attachment_config.h"
#include "Misc/Paths.h"
#include "UObject/SoftObjectPath.h"

#include "terrain_settings.generated.h"

UENUM(BlueprintType)
enum class EPreprocessNoData : uint8 {
    Source,
    NoData,
    Alpha,
};

USTRUCT(BlueprintType)
struct FPreprocessNoData {
    GENERATED_BODY()

    FPreprocessNoData() = default;

    FPreprocessNoData(
        const EPreprocessNoData in_type,
        const double in_no_data_value
    ) : Type{in_type},
        NoDataValue{in_no_data_value} {}

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EPreprocessNoData Type = EPreprocessNoData::Source;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        meta=(EditCondition="Type == EPreprocessNoData::NoData", EditConditionHides))
    double NoDataValue = 0.0;

    static FPreprocessNoData Source() { return {EPreprocessNoData::Source, 0.0}; }
    static FPreprocessNoData Alpha() { return {EPreprocessNoData::Alpha, 0.0}; }
    static FPreprocessNoData NoData(const double v) { return {EPreprocessNoData::NoData, v}; }
};

UENUM(BlueprintType)
enum class EPreprocessDataType : uint8 {
    Source,
    DataType
};

USTRUCT(BlueprintType)
struct FPreprocessDataType {
    GENERATED_BODY()

    FPreprocessDataType() = default;

    FPreprocessDataType(
        const EPreprocessDataType in_type,
        const EGDALDataType in_data_type_value
    ) : Type{in_type},
        DataTypeValue{in_data_type_value} {}

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EPreprocessDataType Type = EPreprocessDataType::Source;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        meta=(EditCondition="Type == EPreprocessDataType::DataType",
            EditConditionHides))
    EGDALDataType DataTypeValue = EGDALDataType::GDT_Unknown;

    static FPreprocessDataType Source() {
        return {EPreprocessDataType::Source, EGDALDataType::GDT_Unknown};
    }

    static FPreprocessDataType DataType(const EGDALDataType dt) {
        return {EPreprocessDataType::DataType, dt};
    }
};

USTRUCT(BlueprintType)
struct FTerrainPreprocessSettings {
    GENERATED_BODY()

    UPROPERTY(
        EditAnywhere,
        Category="Terrain Preprocess",
        DisplayName="Use Albedo")
    bool use_albedo = true;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category="Terrain Preprocess",
        DisplayName="Base Heightmap Source",
        meta=(FilePathFilter = "tif")
    )
    FFilePath heightmap_src_path{FPaths::ProjectContentDir() / "source_data/gebco-sq.tif"};

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category="Terrain Preprocess",
        DisplayName="Base Albedo Source",
        meta=(EditCondition="use_albedo", EditConditionHides, FilePathFilter = "tif|tiff")
    )
    FFilePath albedo_src_path{FPaths::ProjectContentDir() / "source_data/earth-sq.tif"};

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category="Terrain Preprocess",
        DisplayName="Terrain Output Path"
    )
    FDirectoryPath terrain_path{FPaths::ProjectContentDir() / "terrains/earth"};

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category="Terrain Preprocess",
        DisplayName="Temp Output Path")
    FDirectoryPath temp_path{FPaths::ProjectUserDir() / "tmp"};

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category="Terrain Preprocess",
        DisplayName="Should Overwrite Existing Files")
    bool overwrite = true;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category="Terrain Preprocess",
        DisplayName="Heightmap 'NoData' Value")
    FPreprocessNoData heightmap_no_data = FPreprocessNoData::Source();

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category="Terrain Preprocess",
        DisplayName="Albedo 'NoData' Value",
        meta=(EditCondition="use_albedo", EditConditionHides)
    )
    FPreprocessNoData albedo_no_data = FPreprocessNoData::NoData(0.0);

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category="Terrain Preprocess",
        DisplayName="Heightmap Geographic Data Type")
    FPreprocessDataType heightmap_data_type = FPreprocessDataType::DataType(
        static_cast<EGDALDataType>(GDT_Float32));

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category="Terrain Preprocess",
        DisplayName="Albedo Geographic Data Type",
        meta=(EditCondition="use_albedo", EditConditionHides)
    )
    FPreprocessDataType albedo_data_type = FPreprocessDataType::DataType(
        static_cast<EGDALDataType>(GDT_Byte));

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category="Terrain Preprocess",
        DisplayName="Fill Radius")
    float fill_radius = 16.0;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category="Terrain Preprocess",
        DisplayName="Heightmap Mask")
    bool heightmap_create_mask = true;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category="Terrain Preprocess",
        DisplayName="Albedo Mask",
        meta=(EditCondition="use_albedo", EditConditionHides)
    )
    bool albedo_create_mask = false;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category="Terrain Preprocess",
        DisplayName="Heightmap LOD Count",
        meta=(ClampMin="0"))
    TOptional<int32> heightmap_lod_count = NullOpt;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category="Terrain Preprocess",
        DisplayName="Albedo LOD Count",
        meta=(ClampMin="0"),
        meta=(EditCondition="use_albedo", EditConditionHides)
    )
    TOptional<int32> albedo_lod_count{7};

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category="Terrain Preprocess",
        DisplayName="Heightmap Attachment Label")
    FString heightmap_attachment_label = "height";

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category="Terrain Preprocess",
        DisplayName="Albedo Attachment Label",
        meta=(EditCondition="use_albedo", EditConditionHides)
    )
    FString albedo_attachment_label = "albedo";

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category="Terrain Preprocess",
        DisplayName="Texture Size",
        meta=(ClampMin="1"))
    int32 texture_size = 512;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category="Terrain Preprocess",
        DisplayName="Border Size",
        meta=(ClampMin="0"))
    int32 border_size = 2;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category="Terrain Preprocess",
        DisplayName="Mip Level Count",
        meta=(ClampMin="1"))
    int32 mip_level_count = 1;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category="Terrain Preprocess",
        DisplayName="Heightmap Attachemnt Format")
    EAttachmentFormat heightmap_attachment_format = EAttachmentFormat::R32F;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category="Terrain Preprocess",
        DisplayName="Heightmap Attachemnt Format",
        meta=(EditCondition="use_albedo", EditConditionHides)
    )
    EAttachmentFormat albedo_attachment_format = EAttachmentFormat::Rgba8U;

    FString to_string() {
        return FString::Printf(
            TEXT(
                "use_albedo=%s, heightmap_src_path=%s, albedo_src_path=%s, terrain_path=%s, "
                "temp_path=%s, overwrite=%s, heightmap_no_data={type: %ls, value: %f}, "
                "albedo_no_data={type: %ls, value: %f}, "
                "heightmap_data_type={type: %ls, data_type_value: %ls}, "
                "albedo_data_type={type: %ls, data_type_value: %ls}, "
                "fill_radius=%f, heightmap_create_mask=%s, albedo_create_mask=%s, "
                "heightmap_lod_count=%s, albedo_lod_count=%s, heightmap_attachment_label=%s, "
                "albedo_attachment_label=%s, texture_size=%d, border_size=%d, "
                "mip_level_count=%d, heightmap_attachment_format=%ls, albedo_attachment_format=%ls"),
            use_albedo ? TEXT("true") : TEXT("false"),
            *heightmap_src_path.FilePath,
            *albedo_src_path.FilePath,
            *terrain_path.Path,
            *temp_path.Path,
            overwrite ? TEXT("true") : TEXT("false"),
            *UEnum::GetValueAsString(heightmap_no_data.Type),
            heightmap_no_data.NoDataValue,
            *UEnum::GetValueAsString(albedo_no_data.Type),
            albedo_no_data.NoDataValue,
            *UEnum::GetValueAsString(heightmap_data_type.Type),
            *UEnum::GetValueAsString(heightmap_data_type.DataTypeValue),
            *UEnum::GetValueAsString(albedo_data_type.Type),
            *UEnum::GetValueAsString(albedo_data_type.DataTypeValue),
            fill_radius,
            heightmap_create_mask ? TEXT("true") : TEXT("false"),
            albedo_create_mask ? TEXT("true") : TEXT("false"),
            heightmap_lod_count.IsSet()
            ? *FString::FromInt(heightmap_lod_count.GetValue())
            : TEXT("null"),
            albedo_lod_count.IsSet()
            ? *FString::FromInt(albedo_lod_count.GetValue())
            : TEXT("null"),
            *heightmap_attachment_label,
            *albedo_attachment_label,
            texture_size,
            border_size,
            mip_level_count,
            *UEnum::GetValueAsString(heightmap_attachment_format),
            *UEnum::GetValueAsString(albedo_attachment_format)
        );
    }
};

USTRUCT(BlueprintType)
struct FTerrainSettings {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="Terrain", DisplayName="Attachments")
    TArray<FString> attachments = {"height", "albedo"};

    uint32 atlas_size = 1028;

    FString ToString() const {
        return FString::Printf(
            TEXT("attachments=[%s], atlas_size=%u"),
            *FString::Join(attachments, TEXT(", ")),
            atlas_size);
    }
};