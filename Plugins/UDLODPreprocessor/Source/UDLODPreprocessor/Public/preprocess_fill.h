#pragma once
#include "gdal_alg.h"
#include "preprocess_dataset.h"
#include "preprocess_gdal_extended.h"
#include "preprocess_parallel.h"
#include "preprocess_result.h"
#include "preprocess_tile_coordinate.h"
#include "preprocess_util.h"

namespace preprocess {
inline PreprocessResult<void> fill_no_data(
    const GDALDatasetRef& src,
    const double fill_radius
) {
    using std::unexpected;
    for (const auto& rasterband_result : rasterbands(src)) {
        if (!rasterband_result.has_value()) {
            return unexpected{FPreprocessError::Gdal(rasterband_result.error())};
        }
        auto* rasterband = rasterband_result.value();
        const auto rv = GDALFillNodata(
            rasterband,
            nullptr,
            fill_radius,
            0,
            0,
            nullptr,
            GDALDummyProgress,
            nullptr);
        if (rv != CE_None) { return unexpected{FPreprocessError::Gdal(rv)}; }
    }
    return {};
}

template <typename T>
// TODO: requires BITMASK???
    requires GdalType<T>
PreprocessResult<void> create_mask_and_fill_no_data_gen(
    const TArray<FTileCoordinate>& tiles,
    const FPreprocessContext& context
) {
    using ext::types::usize;
    using namespace ext;
    using namespace ext::iter;
    using std::unexpected, std::expected;

    const auto iter_result = par_iter_try_for_each<FTileCoordinate, void, FPreprocessError>(
        tiles,
        [&context](FTileCoordinate tile) -> PreprocessResult<void> {
            auto src_dataset_result = update_tile_dataset(tile, context);
            if (!src_dataset_result.has_value()) {
                return unexpected{src_dataset_result.error()};
            }
            const auto src_dataset = MoveTemp(src_dataset_result.value());

            using FExpectedBand = expected<GDALRasterBand*, CPLErrorNum>;
            using FExpectedBuff = expected<Buffer<uint8>, FPreprocessError>;
            using MasksFunc = FExpectedBuff(*)(FExpectedBand);
            PreprocessResult<TArray<Buffer<uint8>>> masks_result =
                map_try_collect<FExpectedBand, MasksFunc>(
                    rasterbands(src_dataset),
                    [](FExpectedBand src_raster) -> FExpectedBuff {
                        if (!src_raster.has_value()) {
                            return unexpected{FPreprocessError::Gdal(src_raster.error())};
                        }
                        auto* raster = src_raster.value();
                        auto mask_result = open_mask_band(raster);
                        if (!mask_result.has_value()) {
                            return unexpected{FPreprocessError::Gdal(mask_result.error())};
                        }

                        const auto mask = MoveTemp(mask_result.value());

                        const auto mask_data = read_band_as<uint8>(mask);
                        if (!mask_data.has_value()) {
                            return unexpected{FPreprocessError::Gdal(mask_data.error())};
                        }

                        Buffer<uint8> mask_buffer = mask_data.value();
                        return mask_buffer;
                    });

            if (!masks_result.has_value()) { return std::unexpected{masks_result.error()}; }
            const TArray<Buffer<uint8>> masks = MoveTemp(masks_result.value());
            const auto fill_Result = fill_no_data(
                src_dataset,
                static_cast<double>(context.fill_radius));
            if (!fill_Result.has_value()) { return std::unexpected{fill_Result.error()}; }

            using FPreprocExpectedBand = PreprocessResult<GDALRasterBand*>;
            using FBandsFunc = FPreprocExpectedBand(*)(FExpectedBand);
            PreprocessResult<TArray<GDALRasterBand*>> bands_result =
                map_try_collect<FExpectedBand, FBandsFunc>(
                    rasterbands(src_dataset),
                    [](FExpectedBand band_result) -> FPreprocExpectedBand {
                        if (band_result.has_value()) { return band_result.value(); }

                        return unexpected{FPreprocessError::Gdal(band_result.error())};
                    });

            if (!bands_result.has_value()) { return unexpected{bands_result.error()}; }

            auto bands = MoveTemp(bands_result.value());

            auto zipped_masks_and_bands = zip<Buffer<uint8>, GDALRasterBand*>(
                masks,
                bands);

            for (auto& [mask_buffer, band] : zipped_masks_and_bands) {
                BufferResult<T> band_data_result = read_band_as<T>(band);
                if (!band_data_result.has_value()) {
                    return unexpected{FPreprocessError::Gdal(band_data_result.error())};
                }
                Buffer<T>& band_data = band_data_result.value();

                auto zipped_mask_values = zip<uint8, T>(mask_buffer.data(), band_data.data());
                for (auto& [mask, value] : zipped_mask_values) {
                    value = apply_bitmask<T>(value, mask);
                }

                usize texture_size = static_cast<usize>(context.attachment.texture_size);
                expected<void, CPLErrorNum> write_result =
                    write<T>(
                        band,
                        {0, 0},
                        {texture_size, texture_size},
                        band_data);

                if (!write_result.has_value()) {
                    return unexpected{FPreprocessError::Gdal(write_result.error())};
                }
            }
            return {};
        });
    if (!iter_result.has_value()) { return unexpected{iter_result.error()}; }
    return {};
}

inline PreprocessResult<void> only_fill_no_data_gen(
    const TArray<FTileCoordinate>& tiles,
    const FPreprocessContext& context
) {
    using std::unexpected, std::expected;
    const auto iter_result = par_iter_try_for_each<FTileCoordinate, void, FPreprocessError>(
        tiles,
        [&context](const FTileCoordinate tile) -> expected<void, FPreprocessError> {
            auto src_dataset_result = update_tile_dataset(tile, context);
            if (!src_dataset_result.has_value()) {
                return unexpected{src_dataset_result.error()};
            }
            const auto src_dataset = MoveTemp(src_dataset_result.value());
            const auto fill_Result = fill_no_data(
                src_dataset,
                static_cast<double>(context.fill_radius));
            if (!fill_Result.has_value()) { return unexpected{fill_Result.error()}; }

            return {};
        });
    if (!iter_result.has_value()) { return unexpected{iter_result.error()}; }

    return {};
}

#define CREATE_MASK_CASE(type)                                                   \
    {                                                                            \
        const auto res = create_mask_and_fill_no_data_gen<type>(tiles, context); \
        if (!res.has_value()) { return std::unexpected{res.error()}; }       \
        return {};                                                               \
    }

inline PreprocessResult<void> create_mask_and_fill_no_data(
    const TArray<FTileCoordinate>& tiles,
    const FPreprocessContext& context
) {
    const auto data_type = util::transpose_gdal_datatype(context);
    if (context.create_mask) {
        switch (data_type) {
        case ext::GDALDataType::GDT_Unknown: return std::unexpected{
                FPreprocessError::UnknownRasterbandDataType()
            };
        case ext::GDALDataType::GDT_Byte: CREATE_MASK_CASE(uint8);
        case ext::GDALDataType::GDT_Int16: CREATE_MASK_CASE(int16);
        case ext::GDALDataType::GDT_UInt16: CREATE_MASK_CASE(uint16);
        case ext::GDALDataType::GDT_Int32: CREATE_MASK_CASE(int32);
        case ext::GDALDataType::GDT_UInt32: CREATE_MASK_CASE(uint32);
        case ext::GDALDataType::GDT_Float32: CREATE_MASK_CASE(float);
        case ext::GDALDataType::GDT_Float64: CREATE_MASK_CASE(double);
        }
    } else if (context.fill_radius > 0.0) {
        const auto res = only_fill_no_data_gen(tiles, context);
        if (!res.has_value()) { return std::unexpected{res.error()}; }
    }

    return {};
}
}
