#pragma once
#include "gpu_terrain_atlas_buffer_info.h"
#include "preprocess_attachment_config.h"
#include "RenderGraphBuilder.h"
#include "terrain_settings.h"
#include "terrain_tile_atlas.h"

#include "gpu_terrain_attachment.generated.h"

USTRUCT()
struct FGpuAttachment {
    GENERATED_BODY()

    FGpuAttachment() = default;

    FGpuAttachment(
        FRDGBuilder& gb,
        const FString& label,
        const FAttachment& attachment,
        const FTileAtlas& tile_atlas,
        const FTerrainSettings& settings
    ) : index{
            static_cast<uint32>(settings.attachments.IndexOfByPredicate(
                [&label](const FString& name) { return name == label; }
            ))
        },
        buffer_info{attachment, tile_atlas.lod_count},
        atlas_texture{
            gb.CreateSRV(
                FRDGTextureSRVDesc::Create(
                    gb.CreateTexture(
                        FRDGTextureDesc::Create2DArray(
                            FIntPoint{
                                static_cast<int>(buffer_info.texture_size),
                                static_cast<int>(buffer_info.texture_size),
                            },
                            attachment_format_as_pixel_format(buffer_info.format),
                            FClearValueBinding::Transparent,
                            ETextureCreateFlags::Dynamic | ETextureCreateFlags::ShaderResource,
                            static_cast<uint16>(settings.atlas_size),
                            attachment.mip_level_count
                        ),
                        *label
                    )))
        },
        atlas_view{atlas_texture->GetHandle()} {}

    UPROPERTY(VisibleAnywhere)
    uint32 index;

    UPROPERTY(VisibleAnywhere)
    FAtlasBufferInfo buffer_info;

    FRDGTextureSRVRef atlas_texture;
    FRDGViewHandle atlas_view;

    // FShaderPipelineCache* mip_pipeline;
    // TArray<FRDGTextureSRVRef> mip_views;
    // TArray<TArray<uint32>> mips_to_generate;

    friend bool operator ==(const FGpuAttachment& a, const FGpuAttachment& b) {
        return a.index == b.index
            && a.buffer_info == b.buffer_info
            && a.atlas_texture == b.atlas_texture
            && a.atlas_view == b.atlas_view;
    }
};

FORCEINLINE uint32 GetTypeHash(const FGpuAttachment& attachment) {
    uint32 hash = 0;
    hash = HashCombine(hash, GetTypeHash(attachment.index));
    hash = HashCombine(hash, GetTypeHash(attachment.buffer_info));
    hash = HashCombine(hash, GetTypeHash(attachment.atlas_texture));
    hash = HashCombine(hash, GetTypeHash(attachment.atlas_view));
    return hash;
}
