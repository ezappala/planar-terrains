#pragma once
#include "preprocess_attachment_config.h"
#include "Math/Vector.h"
#include "Misc/Paths.h"
#include "UObject/ObjectMacros.h"

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
        BlueprintReadWrite,
        Category="Terrain Preprocess",
        DisplayName="Base Heightmap Source")
    FString heightmap_src_path = FPaths::ProjectContentDir() / "source_data/gebco-sq.tif";

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category="Terrain Preprocess",
        DisplayName="Base Albedo Source")
    FString albedo_src_path = FPaths::ProjectContentDir() / "source_data/earth-sq.tif";

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category="Terrain Preprocess",
        DisplayName="Terrain Output Path")
    FString terrain_path = FPaths::ProjectContentDir() / "terrains/earth";

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category="Terrain Preprocess",
        DisplayName="Temp Output Path")
    FString temp_path = FPaths::ProjectUserDir() / "tmp";

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
        DisplayName="Albedo 'NoData' Value")
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
        DisplayName="Albedo Geographic Data Type")
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
        DisplayName="Albedo Mask")
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
        meta=(ClampMin="0"))
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
        DisplayName="Albedo Attachment Label")
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
        DisplayName="Heightmap Attachemnt Format")
    EAttachmentFormat albedo_attachment_format = EAttachmentFormat::Rgba8U;
};

struct FTerrainSettings {
    FTerrainSettings() : attachments{"Height", "Albedo"},
        atlas_size{1028} {}

    TArray<FString> attachments;
    uint32 atlas_size;
};

USTRUCT(BlueprintType)
struct FPrimaryTerrainSettings {
    GENERATED_BODY()

    // UPROPERTY(EditAnywhere, AdvancedDisplay,
    //     DisplayName="Enable Advanced Mode")
    // bool advanced_mode_enabled = false;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category="Terrain",
        DisplayName="LOD Count")
    int32 lod_count = 7;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category="Terrain",
        DisplayName="Scale")
    FVector3f scale = FVector3f{1.0f, 1.0f, 1.0f};

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category="Terrain",
        DisplayName="Min Height")
    /**
     * Comes from the heightmap texutre
     */
    float min_height = 0.;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category="Terrain",
        DisplayName="Max Height")
    /**
     * Comes from the heightmap texutre
     */
    float max_height = 0.;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category="Terrain",
        DisplayName="Height Scale")
    float height_scale = 1.0;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category="Terrain",
        DisplayName="Heightmap Texture Size")
    /**
     * Comes from the heightmap texutre, but should be 512 for best results
     */
    float heightmap_texture_size = 512.0f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category="Terrain",
        DisplayName="Heightmap Border Size")
    float heightmap_border_size = 2.0f;

    float heightmap_center_size() const {
        return heightmap_texture_size - 2 * heightmap_border_size;
    }

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category="Terrain",
        DisplayName="Heightmap Scale")
    float heightmap_scale = 1.0f;

    float heightmap_offset() const { return heightmap_texture_size - heightmap_border_size; }

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadWrite,
        Category="Terrain",
        DisplayName="Heightmap Mask")
    /**
     * Heightmaps should be a mask
     */
    bool heightmap_mask = true;

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadOnly,
        Category="Terrain",
        DisplayName="Albedo Texture Size")
    /**
     * Comes from the heightmap texutre
     */
    float albedo_texture_size = 512.0f;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadOnly,
        Category="Terrain",
        DisplayName="Albedo Border Size")
    float albedo_border_size = 2.0f;

    float albedo_center_size() const { return albedo_texture_size - 2 * albedo_border_size; }

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category="Terrain",
        DisplayName="Albedo Scale")
    float albedo_scale = 1.0f;

    float albedo_offset() const { return albedo_texture_size - albedo_border_size; }

    UPROPERTY(
        VisibleAnywhere,
        BlueprintReadWrite,
        Category="Terrain",
        DisplayName="Albedo Mask")
    /**
     * Albedo maps should not be masks
     */
    bool albedo_mask = false;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category="Terrain",
        DisplayName="Tree size")
    int32 tree_size = 16;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category="Terrain",
        DisplayName="Geometry tile count")
    int32 geometry_tile_count = 1000000;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category="Terrain",
        DisplayName="Grid Size")
    int32 grid_size = 16;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category="Terrain",
        DisplayName="Tile Size")
    int32 tile_size;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category="Terrain",
        DisplayName="Morph Distance")
    float morph_distance = 40.0;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category="Terrain",
        DisplayName="Blend Distance")
    float blend_distance = 5.0;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category="Terrain",
        DisplayName="Load Distance")
    float load_distance = 0.2;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category="Terrain",
        DisplayName="Subdivision Distance")
    float subdivision_distance = 0.1;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category="Terrain",
        DisplayName="Morph Range")
    float morph_range = 0.2;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category="Terrain",
        DisplayName="Blend Range")
    float blend_range = 0.2;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category="Terrain",
        DisplayName="Precision Distance")
    float precision_distance = 0.001;

    uint32 vertices_per_row() const { return tile_size + 1; }
    uint32 vertices_per_tile() const { return (tile_size + 1) * (tile_size + 1); }
    FPrimaryTerrainSettings() : tile_size{0} {}
};
