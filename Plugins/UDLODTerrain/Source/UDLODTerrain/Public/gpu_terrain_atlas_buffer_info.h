#pragma once
#include "CoreUObject.h"
#include "preprocess_attachment_config.h"
#include "RenderGraphResources.h"
#include "terrain_tile_atlas.h"

#include "gpu_terrain_atlas_buffer_info.generated.h"

constexpr uint32 COPY_BYTES_PER_ROW_ALIGNMENT = 256;

inline uint32 align_byte_size(const uint32 value) {
    return value - 1 - (value - 1) % COPY_BYTES_PER_ROW_ALIGNMENT + COPY_BYTES_PER_ROW_ALIGNMENT;
}

USTRUCT()
struct FAtlasBufferInfo {
    GENERATED_BODY()

    FAtlasBufferInfo() = default;

    FAtlasBufferInfo(const FAttachment& attachment, const uint32 lod_count) : mask{attachment.mask},
        lod_count{lod_count},
        format{attachment.attachment_format},
        texture_size{attachment.texture_size},
        border_size{attachment.border_size},
        center_size{attachment.center_size},
        mip_level_count{attachment.mip_level_count},
        pixels_per_entry{static_cast<uint32>(sizeof(uint32)) / bytes_per_pixel(format)},
        actual_side_size{texture_size * bytes_per_pixel(format)},
        aligned_side_size{align_byte_size(actual_side_size)},
        actual_tile_size{texture_size * actual_side_size},
        aligned_tile_size{texture_size * aligned_side_size},
        entries_per_side{aligned_side_size / static_cast<uint32>(sizeof(uint32))},
        entries_per_tile{texture_size * entries_per_side} {}

    UPROPERTY(VisibleAnywhere)
    bool mask;

    UPROPERTY(VisibleAnywhere)
    uint32 lod_count;

    UPROPERTY(VisibleAnywhere)
    EAttachmentFormat format;

    UPROPERTY(VisibleAnywhere)
    uint32 texture_size;

    UPROPERTY(VisibleAnywhere)
    uint32 border_size;

    UPROPERTY(VisibleAnywhere)
    uint32 center_size;

    UPROPERTY(VisibleAnywhere)
    uint32 mip_level_count;

    UPROPERTY(VisibleAnywhere)
    uint32 pixels_per_entry;

    UPROPERTY(VisibleAnywhere)
    uint32 actual_side_size;

    UPROPERTY(VisibleAnywhere)
    uint32 aligned_side_size;

    UPROPERTY(VisibleAnywhere)
    uint32 actual_tile_size;

    UPROPERTY(VisibleAnywhere)
    uint32 aligned_tile_size;

    UPROPERTY(VisibleAnywhere)
    uint32 entries_per_side;

    UPROPERTY(VisibleAnywhere)
    uint32 entries_per_tile;

    friend bool operator ==(const FAtlasBufferInfo& a, const FAtlasBufferInfo& b) {
        return a.mask == b.mask
            && a.lod_count == b.lod_count
            && a.format == b.format
            && a.texture_size == b.texture_size
            && a.border_size == b.border_size
            && a.center_size == b.center_size
            && a.mip_level_count == b.mip_level_count
            && a.pixels_per_entry == b.pixels_per_entry
            && a.actual_side_size == b.actual_side_size
            && a.aligned_side_size == b.aligned_side_size
            && a.actual_tile_size == b.actual_tile_size
            && a.aligned_tile_size == b.aligned_tile_size
            && a.entries_per_side == b.entries_per_side
            && a.entries_per_tile == b.entries_per_tile;
    }
};

FORCEINLINE uint32 GetTypeHash(const FAtlasBufferInfo& buffer_info) {
    uint32 hash = 0;
    hash = HashCombine(hash, GetTypeHash(buffer_info.mask));
    hash = HashCombine(hash, GetTypeHash(buffer_info.lod_count));
    hash = HashCombine(hash, GetTypeHash(buffer_info.format));
    hash = HashCombine(hash, GetTypeHash(buffer_info.texture_size));
    hash = HashCombine(hash, GetTypeHash(buffer_info.border_size));
    hash = HashCombine(hash, GetTypeHash(buffer_info.center_size));
    hash = HashCombine(hash, GetTypeHash(buffer_info.mip_level_count));
    hash = HashCombine(hash, GetTypeHash(buffer_info.pixels_per_entry));
    hash = HashCombine(hash, GetTypeHash(buffer_info.actual_side_size));
    hash = HashCombine(hash, GetTypeHash(buffer_info.aligned_side_size));
    hash = HashCombine(hash, GetTypeHash(buffer_info.actual_tile_size));
    hash = HashCombine(hash, GetTypeHash(buffer_info.aligned_tile_size));
    hash = HashCombine(hash, GetTypeHash(buffer_info.entries_per_side));
    hash = HashCombine(hash, GetTypeHash(buffer_info.entries_per_tile));
    return hash;
}

USTRUCT()
struct FAtlasTileAttachment {
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere)
    uint32 atlas_index;

    UPROPERTY(VisibleAnywhere)
    FString label;

    friend bool operator==(const FAtlasTileAttachment& a, const FAtlasTileAttachment& b) {
        return a.atlas_index == b.atlas_index
            && a.label == b.label;
    }
};

FORCEINLINE uint32 GetTypeHash(const FAtlasTileAttachment& attachment) {
    uint32 hash = 0;
    hash = HashCombine(hash, GetTypeHash(attachment.atlas_index));
    hash = HashCombine(hash, GetTypeHash(attachment.label));
    return hash;
}
