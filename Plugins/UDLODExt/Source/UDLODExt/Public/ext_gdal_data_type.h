#pragma once
#include "gdal.h"
#include "UObject/ObjectMacros.h"

#include "ext_gdal_data_type.generated.h"

UENUM(BlueprintType)
enum class EGDALDataType : uint8 {
    /*! Unknown or unspecified type */ GDT_Unknown = 0,
    /*! Eight bit unsigned integer */ GDT_Byte = 1,
    /*! 8-bit signed integer (GDAL >= 3.7) */ GDT_Int8 = 14,
    /*! Sixteen bit unsigned integer */ GDT_UInt16 = 2,
    /*! Sixteen bit signed integer */ GDT_Int16 = 3,
    /*! Thirty-two bit unsigned integer */ GDT_UInt32 = 4,
    /*! Thirty-two bit signed integer */ GDT_Int32 = 5,
    /*! 64 bit unsigned integer (GDAL >= 3.5)*/ GDT_UInt64 = 12,
    /*! 64-bit signed integer  (GDAL >= 3.5)*/ GDT_Int64 = 13,
    /*! Thirty-two bit floating point */ GDT_Float32 = 6,
    /*! Sixty-four bit floating point */ GDT_Float64 = 7,
    /*! Complex Int16 */ GDT_CInt16 = 8,
    /*! Complex Int32 */ GDT_CInt32 = 9,
    /* TODO?(#6879): GDT_CInt64 */
    /*! Complex Float32 */ GDT_CFloat32 = 10,
    /*! Complex Float64 */ GDT_CFloat64 = 11,
    /* maximum type # + 1 */ GDT_TypeCount = 15
};

inline GDALDataType to_gdal_data_type(const EGDALDataType dt) {
    switch (dt) {
    case EGDALDataType::GDT_Unknown: return GDT_Unknown;
    case EGDALDataType::GDT_Byte: return GDT_Byte;
    case EGDALDataType::GDT_Int8: return GDT_Int8;
    case EGDALDataType::GDT_UInt16: return GDT_UInt16;
    case EGDALDataType::GDT_Int16: return GDT_Int16;
    case EGDALDataType::GDT_UInt32: return GDT_UInt32;
    case EGDALDataType::GDT_Int32: return GDT_Int32;
    case EGDALDataType::GDT_UInt64: return GDT_UInt64;
    case EGDALDataType::GDT_Int64: return GDT_Int64;
    case EGDALDataType::GDT_Float32: return GDT_Float32;
    case EGDALDataType::GDT_Float64: return GDT_Float64;
    case EGDALDataType::GDT_CInt16: return GDT_CInt16;
    case EGDALDataType::GDT_CInt32: return GDT_CInt32;
    case EGDALDataType::GDT_CFloat32: return GDT_CFloat32;
    case EGDALDataType::GDT_CFloat64: return GDT_CFloat64;
    default:
        UE_LOG(
            LogTemp,
            Warning,
            TEXT("Unrecognized data type %d, defaulting to Unknown"),
            static_cast<uint8>(dt));
        return GDT_Unknown;
    }
}

namespace ext::GDALDataType {
/** * @brief enum GDALDataTypeEnum
 * @details Enum for GDAL data types, used for type-level metadata in the
 * GDALDataType type trait. This enum provides a mapping between C++ types and
 * their corresponding GDAL data types, allowing for compile-time checks and
 * ensuring that the correct data type is used when interacting with GDAL raster
 * bands.
 */
enum GDALDataTypeEnum {
    GDT_Unknown = GDT_Unknown,
    GDT_Byte = GDT_Byte,
    GDT_Int16 = GDT_Int16,
    GDT_UInt16 = GDT_UInt16,
    GDT_Int32 = GDT_Int32,
    GDT_UInt32 = GDT_UInt32,
    GDT_Float32 = GDT_Float32,
    GDT_Float64 = GDT_Float64
};

/**
 * @brief type trait GDALDataType
 * @details Type-level metadata for mapping C++ types to GDAL data types.
 * This is used to ensure that the correct GDAL data type is used when
 * reading from or writing to raster bands.
 */
template <typename T>
struct GDALDataType;

template <>
struct GDALDataType<void> {
    static constexpr GDALDataTypeEnum gdal_original() { return GDT_Unknown; }
};

template <>
struct GDALDataType<uint8> {
    static constexpr GDALDataTypeEnum gdal_original() { return GDT_Byte; }
};

template <>
struct GDALDataType<int16> {
    static constexpr GDALDataTypeEnum gdal_original() { return GDT_Int16; }
};

template <>
struct GDALDataType<uint16> {
    static constexpr GDALDataTypeEnum gdal_original() { return GDT_UInt16; }
};

template <>
struct GDALDataType<int32> {
    static constexpr GDALDataTypeEnum gdal_original() { return GDT_Int32; }
};

template <>
struct GDALDataType<uint32> {
    static constexpr GDALDataTypeEnum gdal_original() { return GDT_UInt32; }
};

template <>
struct GDALDataType<float> {
    static constexpr GDALDataTypeEnum gdal_original() { return GDT_Float32; }
};

template <>
struct GDALDataType<double> {
    static constexpr GDALDataTypeEnum gdal_original() { return GDT_Float64; }
};
}
