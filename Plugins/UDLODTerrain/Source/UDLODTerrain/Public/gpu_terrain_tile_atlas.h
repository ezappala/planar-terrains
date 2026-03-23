#pragma once
#include "gpu_terrain_attachment.h"
#include "RenderGraphBuilder.h"
#include "terrain.h"
#include "Logging/StructuredLog.h"

#include "gpu_terrain_tile_atlas.generated.h"

USTRUCT()
struct FGpuTileAtlas {
    GENERATED_BODY()

    FGpuTileAtlas() = default;

    FGpuTileAtlas(
        FRDGBuilder& gb,
        const FTileAtlas& tile_atlas,
        const FTerrainSettings& settings
    ) : attachments{
        ext::iter::map<FString, FAttachment, FString, FGpuAttachment>(
            tile_atlas.attachments,
            [&gb, &tile_atlas, &settings](
            const FString& label,
            const FAttachment& attachment
        ) {
                return MakeTuple(
                    label,
                    FGpuAttachment(
                        gb,
                        label,
                        attachment,
                        tile_atlas,
                        settings
                    ));
            })
    } {}

    UPROPERTY(VisibleAnywhere)
    TMap<FString, FGpuAttachment> attachments;

    UPROPERTY(VisibleAnywhere)
    TArray<FAttachmentTileWithData> upload_tiles;

    UPROPERTY(VisibleAnywhere)
    TArray<FAttachmentTileWithData> download_tiles;

    friend bool operator ==(const FGpuTileAtlas& a, const FGpuTileAtlas& b) {
        return a.attachments.OrderIndependentCompareEqual(b.attachments);
    }

    static void initialize(
        FRDGBuilder& gb,
        TMap<UTerrain*, FGpuTileAtlas>& gpu_tile_atlases,
        TMap<UTerrain*, FTileAtlas>& tile_atlases,
        const FTerrainSettings& settings
    ) {
        gpu_tile_atlases.Reset();
        for (const auto& [terrain, tile_atlas] : tile_atlases) {
            gpu_tile_atlases.Add(
                terrain,
                FGpuTileAtlas{gb, tile_atlas, settings}
            );
        }
    }

    static void extract(
        TMap<UTerrain*, FTileAtlas>& tile_atlases,
        TMap<UTerrain*, FGpuTileAtlas>& gpu_tile_atlases
    ) {
        for (auto& [terrain, tile_atlas] : tile_atlases) {
            FGpuTileAtlas* gpu_tile_atlas = gpu_tile_atlases.Find(terrain);
            if (gpu_tile_atlas == nullptr) {
                UE_LOGFMT(
                    LogTemp,
                    Warning,
                    "[FGpuTileAtlas::Extract] No GPU tile atlas found for terrain {n}, skipping",
                    terrain->GetName()
                );
                continue;
            }

            Swap(tile_atlas.uploading_tiles, gpu_tile_atlas->upload_tiles);
            // TODO: decide if we wanna deal with mips

            tile_atlas.downloading_tiles.Append(gpu_tile_atlas->download_tiles);
            gpu_tile_atlas->download_tiles.Empty();
        }
    }

    static void prepare(
        FRDGBuilder& gb,
        TMap<UTerrain*, FGpuTileAtlas>& gpu_tile_atlases
    ) {
        // TODO: double check rust source
        for (auto& [terrain, gpu_tile_atlas] : gpu_tile_atlases) {
            // for (const auto& [label, attachment] : gpu_tile_atlas->attachments) {
            // }
            gpu_tile_atlas.exec_upload_tiles(gb);
        }
    }

    static void queue(
        TMap<UTerrain*, FGpuTileAtlas>& gpu_tile_atlases
    ) {
        // noop
        // TODO: if dealing with mips, generate pipeline specializations here.
    }

    void exec_upload_tiles(
        FRDGBuilder& gb
    ) {
        for (
            const auto& tile :
            ext::iter::drain<TArray<FAttachmentTileWithData>, FAttachmentTileWithData>(upload_tiles)
        ) {
            const auto attachment = attachments[tile.label];

            auto input_tex = gb.CreateTexture(
                FRDGTextureDesc::Create2D(
                    FIntPoint{
                        static_cast<int>(attachment.buffer_info.texture_size),
                        static_cast<int>(attachment.buffer_info.texture_size)
                    },
                    attachment_format_as_pixel_format(attachment.buffer_info.format),
                    FClearValueBinding::Transparent,
                    ETextureCreateFlags::Dynamic | ETextureCreateFlags::ShaderResource,
                    attachment.buffer_info.mip_level_count
                ),
                *FString::Printf(TEXT("UDLOD.TileUploadTexture.%s"), *tile.label)
            );

            if (tile.data.IsType<TArray<TStaticArray<uint8, 4>>>()) {
                CopyTextureData2D(
                    tile.data.TryGet<TArray<TStaticArray<uint8, 4>>>(),
                    attachment.atlas_texture,
                    attachment.buffer_info.texture_size,
                    attachment_format_as_pixel_format(attachment.buffer_info.format),
                    attachment.buffer_info.actual_side_size,
                    attachment.buffer_info.actual_side_size
                );
            } else if (tile.data.IsType<TArray<uint16>>()) {
                CopyTextureData2D(
                    tile.data.TryGet<TArray<uint16>>(),
                    attachment.atlas_texture,
                    attachment.buffer_info.texture_size,
                    attachment_format_as_pixel_format(attachment.buffer_info.format),
                    attachment.buffer_info.actual_side_size,
                    attachment.buffer_info.actual_side_size
                );
            } else if (tile.data.IsType<TArray<int16>>()) {
                CopyTextureData2D(
                    tile.data.TryGet<TArray<int16>>(),
                    attachment.atlas_texture,
                    attachment.buffer_info.texture_size,
                    attachment_format_as_pixel_format(attachment.buffer_info.format),
                    attachment.buffer_info.actual_side_size,
                    attachment.buffer_info.actual_side_size
                );
            } else if (tile.data.IsType<TArray<TStaticArray<uint16, 2>>>()) {
                CopyTextureData2D(
                    tile.data.TryGet<TArray<TStaticArray<uint16, 2>>>(),
                    attachment.atlas_texture,
                    attachment.buffer_info.texture_size,
                    attachment_format_as_pixel_format(attachment.buffer_info.format),
                    attachment.buffer_info.actual_side_size,
                    attachment.buffer_info.actual_side_size
                );
            } else if (tile.data.IsType<TArray<float>>()) {
                CopyTextureData2D(
                    tile.data.TryGet<TArray<float>>(),
                    attachment.atlas_texture,
                    attachment.buffer_info.texture_size,
                    attachment_format_as_pixel_format(attachment.buffer_info.format),
                    attachment.buffer_info.actual_side_size,
                    attachment.buffer_info.actual_side_size
                );
            } else {
                UE_LOGFMT(
                    LogTemp,
                    Error,
                    "[FGpuTileAtlas::ExecUploadTiles] Unsupported tile data type for tile {l}, skipping",
                    tile.label
                );
            }
        }
    }
};

FORCEINLINE uint32 GetTypeHash(const FGpuTileAtlas& gpu_tile_atlas) {
    uint32 hash = 0;
    for (const auto& [label, attachment] : gpu_tile_atlas.attachments) {
        hash = HashCombine(hash, GetTypeHash(label));
        hash = HashCombine(hash, GetTypeHash(attachment));
    }
    return hash;
}
