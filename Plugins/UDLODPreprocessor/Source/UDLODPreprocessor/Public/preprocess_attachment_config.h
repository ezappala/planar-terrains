#pragma once
#include <utility>

#include "PixelFormat.h"
#include "preprocess_tile_coordinate.h"
#include "Containers/StaticArray.h"
#include "Misc/TVariant.h"

#include "preprocess_attachment_config.generated.h"

UENUM(BlueprintType)
enum class EAttachmentFormat : uint8 {
    Rgb8U,
    Rgba8U,
    R16U,
    R16I,
    Rg16U,
    R32F
};

inline uint32 bytes_per_pixel(const EAttachmentFormat format) {
    switch (format) {
    case EAttachmentFormat::Rgb8U: return 4;
    case EAttachmentFormat::Rgba8U: return 4;
    case EAttachmentFormat::R16U: return 2;
    case EAttachmentFormat::R16I: return 2;
    case EAttachmentFormat::Rg16U: return 4;
    case EAttachmentFormat::R32F: return 4;
    }
    std::unreachable();
}

inline EPixelFormat attachment_format_as_pixel_format(const EAttachmentFormat f) {
    switch (f) {
    case EAttachmentFormat::Rgb8U: return PF_R8G8B8A8_UINT;
    case EAttachmentFormat::Rgba8U: return PF_R8G8B8A8_UINT;
    case EAttachmentFormat::R16U: return PF_R16_UINT;
    case EAttachmentFormat::R16I: return PF_R16_SINT;
    case EAttachmentFormat::Rg16U: return PF_R16G16_UINT;
    case EAttachmentFormat::R32F: return PF_R32_FLOAT;
    }
    std::unreachable();
}

USTRUCT(BlueprintType)
struct FAttachmentConfig {
    GENERATED_BODY()

    FAttachmentConfig() : texture_size(0),
        border_size(0),
        mip_level_count(1),
        mask(false),
        format(EAttachmentFormat::Rgba8U) {}

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 texture_size;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 border_size;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    int32 mip_level_count;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    bool mask;

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    EAttachmentFormat format;

    uint32 center_size() const {
        return static_cast<uint32>(texture_size) - 2u *
            static_cast<uint32>(border_size);
    }

    uint32 offset_size() const {
        return static_cast<uint32>(texture_size) -
            static_cast<uint32>(border_size);
    }
};

USTRUCT()
struct FAttachmentTile {
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere)
    FTileCoordinate coordinate;

    UPROPERTY(VisibleAnywhere)
    FString label;
};

USTRUCT()
struct FAttachmentTileWithData {
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere)
    uint32 atlas_index;

    UPROPERTY(VisibleAnywhere)
    FString label;

    TVariant<
        TArray<TStaticArray<uint8, 4>>,
        TArray<uint16>,
        TArray<int16>,
        TArray<TStaticArray<uint16, 2>>,
        TArray<float>
    > data;
};

USTRUCT()
struct FAttachment {
    GENERATED_BODY()

    FAttachment() = default;

    FAttachment(const FAttachmentConfig& config, const FString& in_path) : path{in_path},
        texture_size{static_cast<uint32>(config.texture_size)},
        center_size{config.center_size()},
        border_size{static_cast<uint32>(config.border_size)},
        mip_level_count{static_cast<uint32>(config.mip_level_count)},
        attachment_format{config.format},
        mask{config.mask} {}

    UPROPERTY(VisibleAnywhere)
    FString path;

    UPROPERTY(VisibleAnywhere)
    uint32 texture_size;

    UPROPERTY(VisibleAnywhere)
    uint32 center_size;

    UPROPERTY(VisibleAnywhere)
    uint32 border_size;

    UPROPERTY(VisibleAnywhere)
    uint32 mip_level_count;

    UPROPERTY(VisibleAnywhere)
    EAttachmentFormat attachment_format;

    UPROPERTY(VisibleAnywhere)
    bool mask;

    friend bool operator==(const FAttachment& a, const FAttachment& b) {
        return a.path == b.path && a.texture_size == b.texture_size &&
            a.center_size == b.center_size && a.border_size == b.border_size &&
            a.mip_level_count == b.mip_level_count &&
            a.attachment_format == b.attachment_format && a.mask == b.mask;
    }

    friend bool operator!=(const FAttachment& a, const FAttachment& b) { return !(a == b); }
};

FORCEINLINE uint32 GetTypeHash(const FAttachment& attachment) {
    uint32 hash = 0;
    hash = HashCombine(hash, GetTypeHash(attachment.path));
    hash = HashCombine(hash, GetTypeHash(attachment.texture_size));
    hash = HashCombine(hash, GetTypeHash(attachment.center_size));
    hash = HashCombine(hash, GetTypeHash(attachment.border_size));
    hash = HashCombine(hash, GetTypeHash(attachment.mip_level_count));
    hash = HashCombine(hash, GetTypeHash(attachment.attachment_format));
    hash = HashCombine(hash, GetTypeHash(attachment.mask));
    return hash;
}
