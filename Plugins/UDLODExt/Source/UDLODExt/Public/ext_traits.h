#pragma once

#include <type_traits>
#include "ext_gdal_data_type.h"

namespace ext::traits {
/**
 * Type-level constraint for bouding primitive numeric values for
 * generic functions requiring a [`GDALDataType`].
 * See [`GDALDataType`] for access to metadata describing each type.
 */
template <typename T>
concept GdalType = requires {
    typename GDALDataType::GDALDataType<T>;
    {
        GDALDataType::GDALDataType<T>::gdal_original()
    } -> std::convertible_to<GDALDataType::GDALDataTypeEnum>;
};

/**
 * Type-level constraint for bounding trivially copyable types for
 * generic functions requiring copy semantics.
 */
template <typename T>
concept Copy = std::is_trivially_copyable_v<T>;

template <typename T>
concept PartialEq = std::equality_comparable<T>;

template <typename T>
concept ToPrimitive =
    std::is_convertible_v<T, double> || std::is_convertible_v<T, float> ||
    std::is_convertible_v<T, int64> || std::is_convertible_v<T, uint64> ||
    std::is_convertible_v<T, int32> || std::is_convertible_v<T, uint32> ||
    std::is_convertible_v<T, int16> || std::is_convertible_v<T, uint16> ||
    std::is_convertible_v<T, int8> || std::is_convertible_v<T, uint8>;

template <typename T>
concept NumCast = std::is_arithmetic_v<T> && ToPrimitive<T>;
}
