#pragma once
#include "gpu_terrain_attachment.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "terrain.h"
#include "Logging/StructuredLog.h"

BEGIN_SHADER_PARAMETER_STRUCT(FUploadTilePassParameters, UDLODTERRAIN_API)
    RDG_TEXTURE_ACCESS(Texture, ERHIAccess::CopyDest)
END_SHADER_PARAMETER_STRUCT()

struct FGpuTileAtlas {
    FGpuTileAtlas(
        const FTileAtlas& tile_atlas,
        const FTerrainSettings& settings
    ) : attachments{
        ext::iter::map(
            tile_atlas.attachments,
            [&tile_atlas, &settings](const FString& label, const FAttachment& attachment) {
                return MakeTuple(
                    label,
                    FGpuAttachment(label, attachment, tile_atlas, settings));
            })
    } {}

    TMap<FString, FGpuAttachment> attachments;
    TArray<FAttachmentTileWithData> upload_tiles;
    TArray<FAttachmentTileWithData> download_tiles;
    bool bNeedsFullResync = true;

    friend bool operator ==(const FGpuTileAtlas& a, const FGpuTileAtlas& b) {
        return a.attachments.OrderIndependentCompareEqual(b.attachments);
    }

    static void initialize(
        FRDGBuilder& gb,
        TOptional<FGpuTileAtlas>& gpu_tile_atlas,
        const FTileAtlas& tile_atlas,
        const FTerrainSettings& settings
    ) {
        (void)gb;

        auto needs_rebuild = [&tile_atlas, &settings](const FGpuTileAtlas& atlas) {
            if (atlas.attachments.Num() != tile_atlas.attachments.Num()) { return true; }

            for (const auto& [label, attachment] : tile_atlas.attachments) {
                const FGpuAttachment* gpu_attachment = atlas.attachments.Find(label);
                if (gpu_attachment == nullptr) { return true; }

                const uint32 expected_index = static_cast<uint32>(settings.attachments.
                    IndexOfByPredicate([&label](const FString& name) { return name == label; }));
                const FAtlasBufferInfo expected_buffer_info{attachment, tile_atlas.lod_count};
                if (gpu_attachment->index != expected_index ||
                    !(gpu_attachment->buffer_info == expected_buffer_info)) {
                    return true;
                }
            }

            return false;
        };

        const bool bRebuildAtlas = !gpu_tile_atlas.IsSet() || needs_rebuild(gpu_tile_atlas.GetValue());
        if (bRebuildAtlas) {
            gpu_tile_atlas.Reset();
            gpu_tile_atlas.Emplace(tile_atlas, settings);
            gpu_tile_atlas->bNeedsFullResync = true;
        }
    }

    static void extract(
        FTileAtlas& tile_atlas,
        TOptional<FGpuTileAtlas>& gpu_tile_atlas
    ) {
        if (!gpu_tile_atlas.IsSet()) {
            UE_LOGFMT(
                LogTemp,
                Warning,
                "[FGpuTileAtlas::Extract] GPU tile atlas is not set, skipping extraction");
            return;
        }

        if (gpu_tile_atlas->bNeedsFullResync) {
            gpu_tile_atlas->upload_tiles.Empty();
            for (const auto& [loaded_tile, loaded_data] : tile_atlas.loaded_tile_data) {
                const FTileLoadingState* tile_state = tile_atlas.tile_states.Find(
                    loaded_tile.coordinate);
                if (tile_state == nullptr || !tile_state->state.is_loaded) { continue; }

                gpu_tile_atlas->upload_tiles.Add(
                    FAttachmentTileWithData{
                        tile_state->atlas_index,
                        loaded_tile.label,
                        loaded_data
                    }
                );
            }
            gpu_tile_atlas->bNeedsFullResync = false;
        } else {
            gpu_tile_atlas->upload_tiles.Append(MoveTemp(tile_atlas.uploading_tiles));
        }
        tile_atlas.uploading_tiles.Reset();

        tile_atlas.downloading_tiles.Append(gpu_tile_atlas->download_tiles);
        gpu_tile_atlas->download_tiles.Empty();
    }

    static void prepare(
        FRDGBuilder& gb,
        TOptional<FGpuTileAtlas>& gpu_tile_atlas
    ) {
        if (!gpu_tile_atlas.IsSet()) { return; }

        for (TPair<FString, FGpuAttachment>& attachment_entry : gpu_tile_atlas->attachments) {
            attachment_entry.Value.prepare(gb, attachment_entry.Key);
        }

        gpu_tile_atlas->exec_upload_tiles(gb);
    }

    static void queue(
        TOptional<FGpuTileAtlas>&
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
            const FGpuAttachment* attachment = attachments.Find(tile.label);
            if (attachment == nullptr ||
                attachment->atlas_texture_resource == nullptr) { continue; }

            const EPixelFormat pixel_format = to_pixel_format(attachment->buffer_info.format);
            const uint32 source_stride = attachment->buffer_info.actual_side_size;
            const uint32 texture_size = attachment->buffer_info.texture_size;
            FRDGTextureRef atlas_texture = attachment->atlas_texture_resource;
            const uint32 atlas_index = tile.atlas_index;

            auto add_upload_pass = [&gb, atlas_texture, atlas_index, pixel_format, source_stride,
                    texture_size, &tile]<typename PixelType>(const TArray<PixelType>& source_data) {
                FRDGTextureRef staging_texture = gb.CreateTexture(
                    FRDGTextureDesc::Create2D(
                        FIntPoint{
                            static_cast<int32>(texture_size),
                            static_cast<int32>(texture_size)
                        },
                        pixel_format,
                        FClearValueBinding::None,
                        ETextureCreateFlags::CPUWritable
                    ),
                    *FString::Printf(TEXT("UDLOD.StageTile.%s.%u"), *tile.label, atlas_index)
                );

                FUploadTilePassParameters* upload_parameters =
                    gb.AllocParameters<FUploadTilePassParameters>();
                upload_parameters->Texture = staging_texture;

                gb.AddPass(
                    RDG_EVENT_NAME("UDLOD.StageTile.%s.%u", *tile.label, atlas_index),
                    upload_parameters,
                    ERDGPassFlags::Copy | ERDGPassFlags::NeverCull,
                    [staging_texture, source_data, source_stride, texture_size](
                    FRHICommandListImmediate& cmd
                ) {
                        uint32 destination_stride = 0;
                        void* destination_data = cmd.LockTexture2D(
                            staging_texture->GetRHI(),
                            0,
                            RLM_WriteOnly,
                            destination_stride,
                            false
                        );

                        if (destination_data == nullptr) {
                            UE_LOG(
                                LogTemp,
                                Warning,
                                TEXT("Failed to lock staging terrain texture for upload"));
                            return;
                        }

                        const uint8* source_bytes = reinterpret_cast<const uint8*>(
                            source_data.GetData());
                        uint8* destination_bytes = static_cast<uint8*>(destination_data);
                        for (uint32 y = 0; y < texture_size; ++y) {
                            FMemory::Memcpy(
                                destination_bytes + y * destination_stride,
                                source_bytes + y * source_stride,
                                source_stride);
                        }

                        cmd.UnlockTexture2D(staging_texture->GetRHI(), 0, false);
                    }
                );

                FRHICopyTextureInfo copy_info;
                copy_info.DestSliceIndex = atlas_index;
                copy_info.Size = FIntVector(texture_size, texture_size, 1);
                AddCopyTexturePass(gb, staging_texture, atlas_texture, copy_info);
            };

            // @formatter:off
            if (const auto* rgba_data = tile.data.TryGet<TArray<TStaticArray<uint8, 4>>>()) { add_upload_pass(*rgba_data); }
            else if (const auto* r16u_data = tile.data.TryGet<TArray<uint16>>()) { add_upload_pass(*r16u_data); }
            else if (const auto* r16i_data = tile.data.TryGet<TArray<int16>>()) { add_upload_pass(*r16i_data); }
            else if (const auto* rg16u_data = tile.data.TryGet<TArray<TStaticArray<uint16, 2>>>()) { add_upload_pass(*rg16u_data); }
            else if (const auto* r32f_data = tile.data.TryGet<TArray<float>>()) { add_upload_pass(*r32f_data); }
            else { UE_LOGFMT( LogTemp, Error, "[FGpuTileAtlas::ExecUploadTiles] Unsupported tile data type for tile {Label}, skipping", tile.label ); }
            // @formatter:on
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
