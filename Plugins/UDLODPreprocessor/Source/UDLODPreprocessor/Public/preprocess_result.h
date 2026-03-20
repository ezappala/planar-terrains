#pragma once

#include <expected>

#include "cpl_error.h"
#include "SmartPointers.h"

namespace preprocess {
enum class EPreprocessError : uint8 {
    UnknownRasterbandDataType,
    TransformOperationFailed,
    NoDataOutOfRange,
    Gdal,
    Parse,
    Canceled,
};

struct FPreprocessError {
    EPreprocessError Type = EPreprocessError::Gdal;

    struct FVariant {
        enum class EType : uint8 {
            Empty,
            Gdal,
            Parse,
        } Type = EType::Empty;

        CPLErrorNum GdalErrorCode = CPLE_None;
        FString GdalMessage;
        FString ParseMessage;

        FVariant() = default;

        static FVariant Empty() { return {}; }

        static FVariant Gdal(const CPLErrorNum InCode) {
            FVariant V;
            V.Type = EType::Gdal;
            V.GdalErrorCode = InCode;
            V.GdalMessage = UTF8_TO_TCHAR(CPLGetLastErrorMsg());
            return V;
        }

        static FVariant Gdal(const CPLErrorNum InCode, FString InMessage) {
            FVariant V;
            V.Type = EType::Gdal;
            V.GdalErrorCode = InCode;
            V.GdalMessage = MoveTemp(InMessage);
            return V;
        }

        static FVariant Parse(FString InMessage) {
            FVariant V;
            V.Type = EType::Parse;
            V.ParseMessage = MoveTemp(InMessage);
            return V;
        }
    } Variant;

    FPreprocessError() = default;

    static FPreprocessError Gdal(const CPLErrorNum ErrorCode) {
        FPreprocessError Err;
        Err.Type = EPreprocessError::Gdal;
        Err.Variant = FVariant::Gdal(ErrorCode);
        return Err;
    }

    static FPreprocessError Gdal(const CPLErrorNum ErrorCode, FString Message) {
        FPreprocessError Err;
        Err.Type = EPreprocessError::Gdal;
        Err.Variant = FVariant::Gdal(ErrorCode, MoveTemp(Message));
        return Err;
    }

    static FPreprocessError UnknownRasterbandDataType() {
        FPreprocessError Err;
        Err.Type = EPreprocessError::UnknownRasterbandDataType;
        Err.Variant = FVariant::Empty();
        return Err;
    }

    static FPreprocessError TransformOperationFailed() {
        FPreprocessError Err;
        Err.Type = EPreprocessError::TransformOperationFailed;
        Err.Variant = FVariant::Empty();
        return Err;
    }

    static FPreprocessError NoDataOutOfRange() {
        FPreprocessError Err;
        Err.Type = EPreprocessError::NoDataOutOfRange;
        Err.Variant = FVariant::Empty();
        return Err;
    }

    static FPreprocessError Parse(FString Message = FString()) {
        FPreprocessError Err;
        Err.Type = EPreprocessError::Parse;
        Err.Variant = FVariant::Parse(MoveTemp(Message));
        return Err;
    }

    static FPreprocessError Canceled() {
        FPreprocessError Err;
        Err.Type = EPreprocessError::Canceled;
        Err.Variant = FVariant::Empty();
        return Err;
    }

    FString ToString() const {
        switch (Type) {
        case EPreprocessError::UnknownRasterbandDataType: return
                "Unknown rasterband data type";
        case EPreprocessError::TransformOperationFailed: return
                "Transform operation failed";
        case EPreprocessError::NoDataOutOfRange: return
                "The no data value is outside of the datatypes range.";
        case EPreprocessError::Gdal: {
            if (Variant.Type == FVariant::EType::Gdal) {
                if (!Variant.GdalMessage.IsEmpty()) {
                    return FString::Printf(
                        TEXT("GDAL error (%d): %s"),
                        Variant.GdalErrorCode,
                        *Variant.GdalMessage);
                }
                return FString::Printf(
                    TEXT("GDAL error (%d)"),
                    Variant.GdalErrorCode);
            }
            return "GDAL error";
        }
        case EPreprocessError::Parse: {
            if (Variant.Type == FVariant::EType::Parse) {
                if (!Variant.ParseMessage.IsEmpty()) {
                    return FString::Printf(
                        TEXT("Parse error: %s"),
                        *Variant.ParseMessage);
                }
            }
            return "Parse error";
        }
        case EPreprocessError::Canceled: return "Preprocessing canceled";
        }
        return "Unknown error";
    }

    const TCHAR* what() const { return *ToString(); }

    static FPreprocessError From(const CPLErrorNum code) { return Gdal(code); }
};

template <typename T>
using PreprocessResult = std::expected<T, FPreprocessError>;

template <typename T>
    requires std::is_same_v<T, GDALDatasetRef> || std::is_same_v<T, TOptional<GDALDatasetRef>>
PreprocessResult<T> handle_gdal_err() {
    return std::unexpected{FPreprocessError::Gdal(CPLGetLastErrorNo())};
}

inline FPreprocessError make_gdal_error() {
    return FPreprocessError::Gdal(
        CPLGetLastErrorNo(),
        UTF8_TO_TCHAR(CPLGetLastErrorMsg()));
}

template <typename T>
PreprocessResult<T> make_gdal_error_result() { return std::unexpected{make_gdal_error()}; }

inline FPreprocessError make_parse_error(FString Message = FString()) {
    return FPreprocessError::Parse(MoveTemp(Message));
}

template <typename T>
PreprocessResult<T> make_parse_error_result(FString Message = FString()) {
    return std::unexpected{FPreprocessError::Parse(MoveTemp(Message))};
}
} // namespace preprocess
