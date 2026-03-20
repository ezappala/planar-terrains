#pragma once
#include <bit>

#include "ext_buffer.h"
#include "GDALHelpers.h"
#include "ext_gdal_data_type.h"
#include "gdal_priv.h"
#include "ext_traits.h"
#include "Logging/StructuredLog.h"
#include "Misc/Paths.h"

namespace preprocess {
using ext::traits::GdalType, ext::traits::Copy, ext::traits::PartialEq, ext::traits::NumCast;
using ext::types::isize_c, ext::types::usize_c;

/**
 * Reads data from a specified window of a GDAL raster band into a buffer.
 */
template <typename T>
    requires GdalType<T> && Copy<T>
ext::BufferResult<T> read_as(
    GDALRasterBand* rasterband,
    isize_c window,
    usize_c window_size,
    usize_c shape,
    const TOptional<GDALRIOResampleAlg>& e_resample_alg = NullOpt
) {
    const auto pixels = shape.Get<0>() * shape.Get<1>();
    auto data = TArray<T>{};
    data.Reserve(pixels);

    const auto resample_alg = e_resample_alg.Get(GRIORA_NearestNeighbour);
    auto options = GDALRasterIOExtraArg{.eResampleAlg = resample_alg};

    GDALRasterIOExtraArg* options_ptr = &options;
    CPLErr rv = rasterband->RasterIO(
        GF_Read,
        window.Get<0>(),
        window.Get<1>(),
        window_size.Get<0>(),
        window_size.Get<1>(),
        data.GetData(),
        shape.Get<0>(),
        shape.Get<1>(),
        static_cast<GDALDataType>
        (ext::GDALDataType::GDALDataType<T>::gdal_original()),
        0,
        0,
        options_ptr
    );
    if (rv != CE_None) { return std::unexpected{rv}; }

    data.SetNum(pixels);
    return ext::Buffer<T>(data, shape);
}

/**
 * Convenience function for reading an entire raster band into a buffer,
 * without needing to specify the window and shape explicitly.
 */
template <typename T>
    requires GdalType<T> && Copy<T>
ext::BufferResult<T> read_band_as(
    GDALRasterBand* rasterband
) {
    const auto size = usize_c{
        static_cast<ext::types::usize>(rasterband->GetXSize()),
        static_cast<ext::types::usize>(rasterband->GetYSize())
    };

    return read_as<T>(MoveTemp(rasterband), {0, 0}, size, size);
}

/**
 * Writes data from a buffer to a specified window of a GDAL raster band.
 */
template <typename T>
    requires GdalType<T> && Copy<T>
std::expected<void, CPLErrorNum> write(
    GDALRasterBand* rasterband,
    isize_c window,
    usize_c window_size,
    ext::Buffer<T>& buffer
) {
    const usize_c shape = buffer.shape();
    if (buffer.len() != shape.Get<0>() * shape.Get<1>()) {
        return std::unexpected{CPLE_IllegalArg};
    }

    const auto rv = rasterband->RasterIO(
        GF_Write,
        window.Get<0>(),
        window.Get<1>(),
        window_size.Get<0>(),
        window_size.Get<1>(),
        &buffer.data(),
        shape.Get<0>(),
        shape.Get<1>(),
        static_cast<GDALDataType>
        (ext::GDALDataType::GDALDataType<T>::gdal_original()),
        0,
        0
    );

    if (rv != CE_None) { return std::unexpected{rv}; }

    return {};
}

inline std::expected<GDALRasterBand*, CPLErrorNum> open_mask_band(
    GDALRasterBand* raster
) {
    const auto mask_band_ptr = GDALGetMaskBand(raster);
    if (mask_band_ptr == nullptr) { return std::unexpected{CPLE_IllegalArg}; }
    return GDALRasterBand::FromHandle(mask_band_ptr);
}

/**
 * Convenience function to retrieve all the rasterbands of a GDALDataset.
 */
inline TArray<std::expected<GDALRasterBand*, CPLErrorNum>> rasterbands(
    const GDALDatasetRef& dataset
) {
    TArray<std::expected<GDALRasterBand*, CPLErrorNum>> bands;
    // BUG: find out of GetRasterCount is 0-indexed or 1-indexed and
    // adjust accordingly
    for (int i = 0; i < dataset->GetRasterCount(); ++i) {
        // Remember that GDAL raster bands are 1-indexed
        auto rasterband = dataset->GetRasterBand(i + 1);
        if (rasterband == nullptr) { bands.Add(std::unexpected{CPLE_IllegalArg}); }
        bands.Emplace(rasterband);
    }
    return bands;
}

/**
 * Convenience function to retrieve all the rasterbands of a GDALDataset, for
 * owning pointers. Special case for bubbling up the underlying result type.
 */
inline std::expected<
    TArray<GDALRasterBand*>, CPLErrorNum
> try_collect_rasterbands(const GDALDatasetRef& dataset) {
    TArray<GDALRasterBand*> bands;
    for (int i = 0; i < dataset->GetRasterCount(); ++i) {
        auto rasterband = dataset->GetRasterBand(i + 1);
        if (rasterband == nullptr) { return std::unexpected{CPLE_IllegalArg}; }
        bands.Emplace(rasterband);
    }

    return bands;
}

/**
 * Convenience function to retrieve all the rasterbands of a GDALDataset, for
 * non-owning pointers.
 */
inline TArray<std::expected<GDALRasterBand*, CPLErrorNum>> rasterbands(
    const TSharedPtr<GDALDataset>& dataset
) {
    TArray<std::expected<GDALRasterBand*, CPLErrorNum>> bands;
    // BUG: find out of GetRasterCount is 0-indexed or 1-indexed and
    // adjust accordingly
    for (int32 i = 0; i < dataset->GetRasterCount(); ++i) {
        // Remember that GDAL raster bands are 1-indexed
        auto rasterband = dataset->GetRasterBand(i + 1);
        if (rasterband == nullptr) { bands.Add(std::unexpected{CPLE_IllegalArg}); }
        bands.Emplace(rasterband);
    }
    return bands;
}

/**
 * Creates a new GDAL dataset with the specified dimensions, number of bands,
 * and data type, using the provided GDAL driver.
 */
template <typename T>
    requires GdalType<T> && Copy<T>
std::expected<GDALDatasetRef, CPLErrorNum> create_with_band_type_with_options(
    GDALDriver* driver,
    const FString& filename,
    const ext::types::usize size_x,
    const ext::types::usize size_y,
    const ext::types::usize bands,
    const CSLConstList options
) {
    const GDALDataType data_type = static_cast<GDALDataType>(
        ext::GDALDataType::GDALDataType<T>::gdal_original()
    );

    UE_LOGFMT(
        LogTemp,
        Log,
        "Creating dataset with filename: {n}, size: ({x}, {y}), bands: {bands}, data type: {data_type}",
        filename,
        size_x,
        size_y,
        bands,
        data_type);

    auto* dataset = driver->Create(
        TCHAR_TO_UTF8(*FPaths::GetCleanFilename(filename)),
        static_cast<int>(size_x),
        static_cast<int>(size_y),
        static_cast<int>(bands),
        data_type,
        options
    );

    if (dataset == nullptr) { return std::unexpected{CPLE_OpenFailed}; }

    return GDALDatasetRef{dataset};
}

template <typename T>
T apply_bitmask(const T& value, const uint8 mask) {
    if constexpr (std::is_same_v<T, float>) {
        return std::bit_cast<float>(
            std::bit_cast<uint32>(value) & ~1 | (mask != 0 ? 1 : 0));
    } else { return value & ~1 | (mask != 0 ? 1 : 0); }
}
} // namespace preprocess
