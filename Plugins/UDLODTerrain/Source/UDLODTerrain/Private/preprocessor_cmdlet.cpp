#include "preprocessor_cmdlet.h"

#include "terrain_preprocess_runner.h"
#include "Logging/StructuredLog.h"
#include "Misc/Parse.h"

namespace {
template <typename T>
bool parse_value(const FString& params, const TCHAR* key, T& out_value);

template <>
bool parse_value<FString>(const FString& params, const TCHAR* key, FString& out_value) {
    return FParse::Value(*params, key, out_value);
}

template <>
bool parse_value<int32>(const FString& params, const TCHAR* key, int32& out_value) {
    return FParse::Value(*params, key, out_value);
}

template <>
bool parse_value<float>(const FString& params, const TCHAR* key, float& out_value) {
    return FParse::Value(*params, key, out_value);
}

template <>
bool parse_value<FDirectoryPath>(
    const FString& params,
    const TCHAR* key,
    FDirectoryPath& out_value) {
    FString path_str;
    if (!FParse::Value(*params, key, path_str)) { return false; }
    out_value.Path = path_str;
    return true;
}

template <>
bool parse_value<FFilePath>(const FString& params, const TCHAR* key, FFilePath& out_value) {
    FString path_str;
    if (!FParse::Value(*params, key, path_str)) { return false; }
    out_value.FilePath = path_str;
    return true;
}

bool parse_bool_value(const FString& params, const TCHAR* key, bool& out_value) {
    FString parsed_value;
    if (!FParse::Value(*params, key, parsed_value)) {
        return false;
    }

    parsed_value = parsed_value.ToLower();
    if (parsed_value == TEXT("true") || parsed_value == TEXT("1") || parsed_value == TEXT("yes")) {
        out_value = true;
        return true;
    }

    if (parsed_value == TEXT("false") || parsed_value == TEXT("0") || parsed_value == TEXT("no")) {
        out_value = false;
        return true;
    }

    return false;
}

TOptional<EGDALDataType> parse_data_type(const FString& value) {
    const FString normalized = value.ToUpper();
    if (normalized == TEXT("BYTE") || normalized == TEXT("GDT_BYTE")) {
        return static_cast<EGDALDataType>(GDT_Byte);
    }
    if (normalized == TEXT("UINT16") || normalized == TEXT("GDT_UINT16")) {
        return static_cast<EGDALDataType>(GDT_UInt16);
    }
    if (normalized == TEXT("INT16") || normalized == TEXT("GDT_INT16")) {
        return static_cast<EGDALDataType>(GDT_Int16);
    }
    if (normalized == TEXT("UINT32") || normalized == TEXT("GDT_UINT32")) {
        return static_cast<EGDALDataType>(GDT_UInt32);
    }
    if (normalized == TEXT("INT32") || normalized == TEXT("GDT_INT32")) {
        return static_cast<EGDALDataType>(GDT_Int32);
    }
    if (normalized == TEXT("FLOAT32") || normalized == TEXT("GDT_FLOAT32")) {
        return static_cast<EGDALDataType>(GDT_Float32);
    }
    if (normalized == TEXT("FLOAT64") || normalized == TEXT("GDT_FLOAT64")) {
        return static_cast<EGDALDataType>(GDT_Float64);
    }
    return NullOpt;
}

TOptional<EAttachmentFormat> parse_attachment_format(const FString& value) {
    const FString normalized = value.ToUpper();
    if (normalized == TEXT("RGB8U")) {
        return EAttachmentFormat::Rgb8U;
    }
    if (normalized == TEXT("R16U")) {
        return EAttachmentFormat::R16U;
    }
    if (normalized == TEXT("R32F")) {
        return EAttachmentFormat::R32F;
    }
    if (normalized == TEXT("RGBA8U")) {
        return EAttachmentFormat::Rgba8U;
    }
    return NullOpt;
}

bool apply_no_data_mode(
    const FString& params,
    const TCHAR* mode_key,
    const TCHAR* value_key,
    FPreprocessNoData& out_no_data
) {
    FString mode_value;
    if (!FParse::Value(*params, mode_key, mode_value)) {
        return true;
    }

    mode_value = mode_value.ToLower();
    if (mode_value == TEXT("source")) {
        out_no_data = FPreprocessNoData::Source();
        return true;
    }

    if (mode_value == TEXT("alpha")) {
        out_no_data = FPreprocessNoData::Alpha();
        return true;
    }

    if (mode_value == TEXT("nodata")) {
        double no_data_value = 0.0;
        if (!FParse::Value(*params, value_key, no_data_value)) {
            return false;
        }
        out_no_data = FPreprocessNoData::NoData(no_data_value);
        return true;
    }

    return false;
}

bool apply_data_type_override(
    const FString& params,
    const TCHAR* key,
    FPreprocessDataType& out_data_type
) {
    FString value;
    if (!FParse::Value(*params, key, value)) {
        return true;
    }

    if (value.Equals(TEXT("Source"), ESearchCase::IgnoreCase)) {
        out_data_type = FPreprocessDataType::Source();
        return true;
    }

    const TOptional<EGDALDataType> parsed_value = parse_data_type(value);
    if (!parsed_value.IsSet()) {
        return false;
    }

    out_data_type = FPreprocessDataType::DataType(parsed_value.GetValue());
    return true;
}

bool apply_attachment_format_override(
    const FString& params,
    const TCHAR* key,
    EAttachmentFormat& out_format
) {
    FString value;
    if (!FParse::Value(*params, key, value)) {
        return true;
    }

    const TOptional<EAttachmentFormat> parsed_value = parse_attachment_format(value);
    if (!parsed_value.IsSet()) {
        return false;
    }

    out_format = parsed_value.GetValue();
    return true;
}

FString usage() {
    return TEXT(
        "Usage: UnrealEditor-Cmd.exe <Project.uproject> -run=UDLODPreprocessor "
        "-HeightmapSrc=<path> [-UseAlbedo=true|false] [-AlbedoSrc=<path>] -TerrainPath=<path> [-TempPath=<path>] "
        "[-HeightmapLodCount=<n>] [-AlbedoLodCount=<n>] [-Overwrite=true|false]");
}
} // namespace

UUDLODPreprocessorCommandlet::UUDLODPreprocessorCommandlet() {
    IsClient = false;
    IsEditor = true;
    IsServer = false;
    LogToConsole = true;
    ShowErrorCount = true;
}

int32 UUDLODPreprocessorCommandlet::Main(const FString& params) {
    if (FParse::Param(*params, TEXT("Help"))) {
        UE_LOGFMT(LogTemp, Display, "{usage}", usage());
        return 0;
    }

    FTerrainPreprocessSettings settings{};

    parse_bool_value(params, TEXT("UseAlbedo="), settings.use_albedo);
    parse_value(params, TEXT("HeightmapSrc="), settings.heightmap_src_path);
    parse_value(params, TEXT("AlbedoSrc="), settings.albedo_src_path);
    parse_value(params, TEXT("TerrainPath="), settings.terrain_path);
    parse_value(params, TEXT("TempPath="), settings.temp_path);
    parse_bool_value(params, TEXT("Overwrite="), settings.overwrite);
    parse_value(params, TEXT("FillRadius="), settings.fill_radius);
    parse_bool_value(params, TEXT("HeightmapCreateMask="), settings.heightmap_create_mask);
    parse_bool_value(params, TEXT("AlbedoCreateMask="), settings.albedo_create_mask);
    parse_value(params, TEXT("HeightmapAttachmentLabel="), settings.heightmap_attachment_label);
    parse_value(params, TEXT("AlbedoAttachmentLabel="), settings.albedo_attachment_label);
    parse_value(params, TEXT("TextureSize="), settings.texture_size);
    parse_value(params, TEXT("BorderSize="), settings.border_size);
    parse_value(params, TEXT("MipLevelCount="), settings.mip_level_count);

    int32 LodCount = 0;
    if (parse_value(params, TEXT("HeightmapLodCount="), LodCount)) {
        settings.heightmap_lod_count = LodCount;
    }
    if (parse_value(params, TEXT("AlbedoLodCount="), LodCount)) {
        settings.albedo_lod_count = LodCount;
    }

    if (!apply_no_data_mode(
            params,
            TEXT("HeightmapNoDataMode="),
            TEXT("HeightmapNoDataValue="),
            settings.heightmap_no_data) ||
        !apply_no_data_mode(
            params,
            TEXT("AlbedoNoDataMode="),
            TEXT("AlbedoNoDataValue="),
            settings.albedo_no_data) ||
        !apply_data_type_override(params, TEXT("HeightmapDataType="), settings.heightmap_data_type) ||
        !apply_data_type_override(params, TEXT("AlbedoDataType="), settings.albedo_data_type) ||
        !apply_attachment_format_override(
            params,
            TEXT("HeightmapAttachmentFormat="),
            settings.heightmap_attachment_format) ||
        !apply_attachment_format_override(
            params,
            TEXT("AlbedoAttachmentFormat="),
            settings.albedo_attachment_format)) {
        UE_LOGFMT(LogTemp, Error, "{usage}", usage());
        return 1;
    }

    const auto result = run_preprocess(
        settings,
        preprocess::FPreprocessRunOptions{
            preprocess::EPreprocessProgressMode::Console,
            0.0f
        });
    if (!result.has_value()) {
        UE_LOGFMT(LogTemp, Error, "Terrain preprocessing failed: {error}", result.error().ToString());
        return 1;
    }

    UE_LOGFMT(
        LogTemp,
        Display,
        "Terrain preprocessing completed in {duration}.",
        result.value().duration.ToString());
    return 0;
}
