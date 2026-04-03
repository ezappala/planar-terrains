#pragma once
#include "gpu_terrain_atlas_buffer_info.h"
#include "preprocess_attachment_config.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "terrain_settings.h"
#include "terrain_tile_atlas.h"
#include "geos/namespaces.h"

struct FGpuAttachment {
    FGpuAttachment(
        const FString& label,
        const FAttachment& attachment,
        const FTileAtlas& tile_atlas,
        const FTerrainSettings& settings
    ) : index{
            static_cast<uint32>(settings.attachments.IndexOfByPredicate(
                [&label](const FString& name) { return name == label; }
            ))
        },
        buffer_info{attachment, tile_atlas.lod_count} {
        if (label.Equals(TEXT("height"), ESearchCase::IgnoreCase)) {
            checkf(
                buffer_info.format == EAttachmentFormat::R32F,
                TEXT("Height attachment must be R32F for the current terrain shader path.")
            );
        }

        if (label.Equals(TEXT("albedo"), ESearchCase::IgnoreCase)) {
            checkf(
                buffer_info.format == EAttachmentFormat::Rgba8U || buffer_info.format ==
                EAttachmentFormat::Rgb8U,
                TEXT(
                    "Albedo attachment must be an 8-bit color format "
                    "for the current terrain_shader path."
                )
            );
        }

        constexpr uint16 mip_count = 1;
        const FRDGTextureDesc texture_desc = FRDGTextureDesc::Create2DArray(
            FIntPoint{
                static_cast<int32>(buffer_info.texture_size),
                static_cast<int32>(buffer_info.texture_size)
            },
            to_pixel_format(buffer_info.format),
            FClearValueBinding::Transparent,
            ETextureCreateFlags::ShaderResource | ETextureCreateFlags::UAV,
            static_cast<uint16>(settings.atlas_size),
            mip_count
        );
        AllocatePooledTexture(texture_desc, atlas_texture_pooled, *label);
    }

    void prepare(FRDGBuilder& gb, const FString& label) {
        if (!atlas_texture_pooled.IsValid()) {
            atlas_texture_resource = nullptr;
            atlas_texture_uav = nullptr;
            atlas_texture = nullptr;
            atlas_view = {};
            return;
        }

        atlas_texture_resource = gb.RegisterExternalTexture(atlas_texture_pooled, *label);
        atlas_texture_uav = gb.CreateUAV(atlas_texture_resource);
        atlas_texture = gb.CreateSRV(FRDGTextureSRVDesc::Create(atlas_texture_resource));
        atlas_view = atlas_texture->GetHandle();
    }

    uint32 index;
    FAtlasBufferInfo buffer_info;

    TRefCountPtr<IPooledRenderTarget> atlas_texture_pooled;
    FRDGTextureRef atlas_texture_resource;
    FRDGTextureUAVRef atlas_texture_uav;
    FRDGTextureSRVRef atlas_texture;
    FRDGViewHandle atlas_view;

    // FShaderPipelineCache* mip_pipeline;
    // TArray<FRDGTextureSRVRef> mip_views;
    // TArray<TArray<uint32>> mips_to_generate;

    friend bool operator ==(const FGpuAttachment& a, const FGpuAttachment& b) {
        return a.index == b.index
            && a.buffer_info == b.buffer_info
            && a.atlas_texture_pooled == b.atlas_texture_pooled
            && a.atlas_texture_resource == b.atlas_texture_resource
            && a.atlas_texture_uav == b.atlas_texture_uav
            && a.atlas_texture == b.atlas_texture
            && a.atlas_view == b.atlas_view;
    }
};

FORCEINLINE uint32 GetTypeHash(const FGpuAttachment& attachment) {
    uint32 hash = 0;
    hash = HashCombine(hash, GetTypeHash(attachment.index));
    hash = HashCombine(hash, GetTypeHash(attachment.buffer_info));
    hash = HashCombine(
        hash,
        GetTypeHash(reinterpret_cast<UPTRINT>(attachment.atlas_texture_pooled.GetReference()))
    );
    hash = HashCombine(hash, GetTypeHash(attachment.atlas_texture_resource));
    hash = HashCombine(hash, GetTypeHash(attachment.atlas_texture_uav));
    hash = HashCombine(hash, GetTypeHash(attachment.atlas_texture));
    hash = HashCombine(hash, GetTypeHash(attachment.atlas_view));
    return hash;
}
