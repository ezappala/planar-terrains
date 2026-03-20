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
    for (const auto& rasterband_result : rasterbands(src)) {
        if (!rasterband_result.has_value()) {
            return std::unexpected{
                FPreprocessError::Gdal(rasterband_result.error())
            };
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
        if (rv != CE_None) { return std::unexpected{FPreprocessError::Gdal(rv)}; }
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
    par_iter_try_for_each<FTileCoordinate, void, FPreprocessError>(
        tiles,
        [&context](FTileCoordinate tile) -> PreprocessResult<void> {
            auto src_dataset_result = update_tile_dataset(tile, context);
            if (!src_dataset_result.has_value()) {
                return std::unexpected{src_dataset_result.error()};
            }
            const auto src_dataset = MoveTemp(src_dataset_result.value());

            PreprocessResult<TArray<ext::Buffer<uint8>>> masks_result =
                ext::iter::map_try_collect<
                    GDALRasterBand*,
                    CPLErrorNum,
                    ext::Buffer<uint8>,
                    FPreprocessError
                >(
                    rasterbands(src_dataset),
                    [](
                    std::expected<GDALRasterBand*, CPLErrorNum> src_raster
                )
                    -> std::expected<ext::Buffer<uint8>, FPreprocessError> {
                        if (!src_raster.has_value()) {
                            return std::unexpected{
                                FPreprocessError::Gdal(src_raster.error())
                            };
                        }
                        auto* raster = src_raster.value();
                        auto mask_result = open_mask_band(raster);
                        if (!mask_result.has_value()) {
                            return std::unexpected{
                                FPreprocessError::Gdal(mask_result.error())
                            };
                        }

                        const auto mask = MoveTemp(mask_result.value());

                        const auto mask_data = read_band_as<uint8>(mask);
                        if (!mask_data.has_value()) {
                            return std::unexpected{
                                FPreprocessError::Gdal(mask_data.error())
                            };
                        }
                        ext::Buffer<uint8> mask_buffer = mask_data.value();
                        return mask_buffer;
                    });

            if (!masks_result.has_value()) { return std::unexpected{masks_result.error()}; }
            const TArray<ext::Buffer<uint8>> masks = MoveTemp(masks_result.value());
            const auto fill_Result = fill_no_data(
                src_dataset,
                static_cast<double>(context.fill_radius));
            if (!fill_Result.has_value()) { return std::unexpected{fill_Result.error()}; }

            PreprocessResult<TArray<GDALRasterBand*>> bands_result =
                ext::iter::map_try_collect<
                    GDALRasterBand*,
                    CPLErrorNum,
                    GDALRasterBand*,
                    FPreprocessError
                >(
                    rasterbands(src_dataset),
                    [](
                    std::expected<GDALRasterBand*, CPLErrorNum> band_result
                ) -> PreprocessResult<GDALRasterBand*> {
                        if (band_result.has_value()) { return band_result.value(); }

                        return std::unexpected{
                            FPreprocessError::Gdal(band_result.error())
                        };
                    });

            if (!bands_result.has_value()) { return std::unexpected{bands_result.error()}; }

            auto bands = MoveTemp(bands_result.value());

            for (auto& [mask_buffer, band]
                 : ext::iter::zip(masks, bands)) {
                ext::BufferResult<float> band_data_result =
                    read_band_as<float>(band);
                if (!band_data_result.has_value()) {
                    return std::unexpected{
                        FPreprocessError::Gdal(band_data_result.error())
                    };
                }
                ext::Buffer<float>& band_data = band_data_result.value();

                for (auto& [mask, value] : ext::iter::zip(
                         mask_buffer.data(),
                         band_data.data())) { value = apply_bitmask<float>(value, mask); }

                std::expected<void, CPLErrorNum> write_result =
                    write<float>(
                        band,
                        {0, 0},
                        {
                            static_cast<usize>(context.attachment.
                                texture_size),
                            static_cast<usize>(context.attachment.
                                texture_size)
                        },
                        band_data);
                if (!write_result.has_value()) {
                    return std::unexpected{
                        FPreprocessError::Gdal(write_result.error())
                    };
                }
            }
            return {};
        });
    return {};
}

inline PreprocessResult<void> only_fill_no_data_gen(
    const TArray<FTileCoordinate>& tiles,
    const FPreprocessContext& context
) {
    par_iter_try_for_each<FTileCoordinate, void, FPreprocessError>(
        tiles,
        [&context](const FTileCoordinate tile) -> std::expected<void, FPreprocessError> {
            auto src_dataset_result = update_tile_dataset(tile, context);
            if (!src_dataset_result.has_value()) {
                return std::unexpected{src_dataset_result.error()};
            }
            const auto src_dataset = MoveTemp(src_dataset_result.value());
            const auto fill_Result = fill_no_data(
                src_dataset,
                static_cast<double>(context.fill_radius));
            if (!fill_Result.has_value()) { return std::unexpected{fill_Result.error()}; }

            return {};
        });

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
