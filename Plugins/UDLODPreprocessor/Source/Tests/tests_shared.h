#pragma once

#include "preprocess_dataset.h"
#include "preprocess_gdal_extended.h"
#include "HAL/FileManager.h"
#include "Misc/AutomationTest.h"
#include "Misc/Guid.h"
#include "Misc/Paths.h"

namespace preprocess::tests {
inline constexpr EAutomationTestFlags TestFlags =
    EAutomationTestFlags::EditorContext |
    EAutomationTestFlags::ProductFilter;

inline FString make_unique_test_root(const FString& suite_name) {
    return FPaths::ConvertRelativePathToFull(
        FPaths::Combine(
            FPaths::ProjectSavedDir(),
            TEXT("Automation"),
            TEXT("UDLODPreprocessor"),
            suite_name,
            FGuid::NewGuid().ToString(EGuidFormats::Digits)));
}

inline void delete_tree(const FString& root) {
    if (!root.IsEmpty()) { IFileManager::Get().DeleteDirectory(*root, false, true); }
}

inline FRasterbandConfig make_band(const EGDALColorInterp interp) {
    FRasterbandConfig band;
    band.color_interp = interp;
    return band;
}

inline FPreprocessContext make_context(
    const FString& root,
    const EAttachmentFormat format = EAttachmentFormat::R32F,
    const int32 texture_size = 64,
    const int32 border_size = 2,
    const EGDALDataType data_type = static_cast<EGDALDataType>(GDT_Float32)
) {
    FPreprocessContext context{};
    context.data_type = data_type;
    context.no_data_value = -9999.0;
    context.rasterbands = {make_band(EGDALColorInterp::GCI_GrayIndex)};
    context.tile_dir = FPaths::Combine(root, TEXT("tiles"));
    context.temp_dir = FPaths::Combine(root, TEXT("temp"));
    context.fill_radius = 0.0f;
    context.overwrite = true;
    context.min_height = 0.0f;
    context.max_height = 1.0f;
    context.create_mask = false;
    context.attachment_label = TEXT("height");
    context.terrain_path = FPaths::Combine(root, TEXT("terrain"));
    context.lod_count = 4;

    context.attachment.texture_size = texture_size;
    context.attachment.border_size = border_size;
    context.attachment.mip_level_count = 1;
    context.attachment.mask = false;
    context.attachment.format = format;

    return context;
}

template <typename T>
GDALDatasetRef create_memory_dataset(
    const int32 width,
    const int32 height,
    const int32 bands = 1
) {
    GDALAllRegister();

    auto* driver = GDALDriver::FromHandle(GDALGetDriverByName("MEM"));
    check(driver != nullptr);

    auto* raw = driver->Create(
        "",
        width,
        height,
        bands,
        static_cast<GDALDataType>(ext::GDALDataType::GDALDataType<T>::gdal_original()),
        nullptr);

    check(raw != nullptr);
    return GDALDatasetRef{raw};
}

template <typename T>
GDALDatasetRef create_tiff_dataset(
    const FString& path,
    const int32 width,
    const int32 height,
    const int32 bands = 1
) {
    GDALAllRegister();

    IFileManager::Get().MakeDirectory(*FPaths::GetPath(path), true);

    auto* driver = GDALDriver::FromHandle(GDALGetDriverByName("GTiff"));
    check(driver != nullptr);

    auto* raw = driver->Create(
        TCHAR_TO_UTF8(*path),
        width,
        height,
        bands,
        static_cast<GDALDataType>(ext::GDALDataType::GDALDataType<T>::gdal_original()),
        nullptr);

    check(raw != nullptr);
    return GDALDatasetRef{raw};
}
} // namespace preprocess::tests
