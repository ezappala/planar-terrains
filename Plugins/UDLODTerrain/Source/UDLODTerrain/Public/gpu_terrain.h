#pragma once

#include "gpu_terrain_funcs.h"
#include "RenderGraphFwd.h"
#include "RenderGraphUtils.h"
#include "RHIStaticStates.h"
#include "terrain_shaders.h"
#include "terrain_tile_tree.h"
#include "Logging/StructuredLog.h"

struct FGpuTerrain {
    explicit FGpuTerrain(TOptional<FGpuTileAtlas>& gpu_tile_atlas) : atlas_sampler{
            TStaticSamplerState<
                /*Filter=*/SF_AnisotropicLinear,
                /*Address Modes*/
                /*AddressU=*/AM_Clamp,
                /*AddressV=*/AM_Clamp,
                /*AddressW=*/AM_Clamp,
                /*MipBias=*/0,
                /*MaxAnisotropy=*/16
            >().CreateRHI()
        },
        attachment_configs{
            attachment_config_from_gpu_tile_atlas(gpu_tile_atlas, TEXT("height")),
            attachment_config_from_gpu_tile_atlas(gpu_tile_atlas, TEXT("albedo"))
        } {
        const FRDGBufferDesc buffer_desc = FRDGBufferDesc::CreateStructuredDesc(
            sizeof(AttachmentConfig),
            attachment_configs.Num()
        );
        AllocatePooledBuffer(
            buffer_desc,
            attachments_buffer_pooled,
            TEXT("UDLOD.AttachmentsBuffer")
        );
    }

    TUniformBufferRef<Terrain> terrain_buffer;
    FSamplerStateRHIRef atlas_sampler;
    FRDGTextureSRVRef height_attachment_texture;
    FRDGTextureSRVRef albedo_attachment_texture;
    TStaticArray<AttachmentConfig, 2> attachment_configs;
    TRefCountPtr<FRDGPooledBuffer> attachments_buffer_pooled;
    FRDGBufferRef attachments_buffer;
    FRDGBufferSRVRef attachments_buffer_srv;

    friend bool operator ==(const FGpuTerrain& a, const FGpuTerrain& b) {
        return a.terrain_buffer == b.terrain_buffer
            && a.atlas_sampler == b.atlas_sampler
            && a.height_attachment_texture == b.height_attachment_texture
            && a.albedo_attachment_texture == b.albedo_attachment_texture
            && a.attachments_buffer_pooled == b.attachments_buffer_pooled
            && a.attachments_buffer == b.attachments_buffer
            && a.attachments_buffer_srv == b.attachments_buffer_srv;
    }

    friend bool operator !=(const FGpuTerrain& a, const FGpuTerrain& b) { return !(a == b); }

    static void initialize(
        FRDGBuilder& gb,
        const FRDGTextureSRVRef fallback_texture,
        TOptional<FGpuTerrain>& gpu_terrains,
        TOptional<FGpuTileAtlas>& gpu_tile_atlas
    ) {
        if (!gpu_terrains.IsSet()) { gpu_terrains.Emplace(FGpuTerrain(gpu_tile_atlas)); }

        gpu_terrains->attachment_configs = {
            attachment_config_from_gpu_tile_atlas(gpu_tile_atlas, TEXT("height")),
            attachment_config_from_gpu_tile_atlas(gpu_tile_atlas, TEXT("albedo"))
        };

        const FRDGBufferDesc buffer_desc = FRDGBufferDesc::CreateStructuredDesc(
            sizeof(AttachmentConfig),
            gpu_terrains->attachment_configs.Num()
        );
        AllocatePooledBuffer(
            buffer_desc,
            gpu_terrains->attachments_buffer_pooled,
            TEXT("UDLOD.AttachmentsBuffer")
        );

        gpu_terrains->attachments_buffer = gb.RegisterExternalBuffer(
            gpu_terrains->attachments_buffer_pooled,
            TEXT("UDLOD.AttachmentsBuffer")
        );

        AttachmentConfig* uploaded_configs = gb.AllocPODArray<AttachmentConfig>(
            gpu_terrains->attachment_configs.Num()
        );
        for (int32 index = 0; index < gpu_terrains->attachment_configs.Num(); ++index) {
            uploaded_configs[index] = gpu_terrains->attachment_configs[index];
        }
        gb.QueueBufferUpload(
            gpu_terrains->attachments_buffer,
            uploaded_configs,
            sizeof(AttachmentConfig) * gpu_terrains->attachment_configs.Num()
        );
        gpu_terrains->attachments_buffer_srv = gb.CreateSRV(gpu_terrains->attachments_buffer);

        const FGpuAttachment* height_attachment = gpu_tile_atlas.IsSet()
            ? find_attachment_by_label(gpu_tile_atlas->attachments, TEXT("height"))
            : nullptr;
        const FGpuAttachment* albedo_attachment = gpu_tile_atlas.IsSet()
            ? find_attachment_by_label(gpu_tile_atlas->attachments, TEXT("albedo"))
            : nullptr;

        gpu_terrains->height_attachment_texture = height_attachment != nullptr
            ? height_attachment->atlas_texture
            : fallback_texture;
        gpu_terrains->albedo_attachment_texture = albedo_attachment != nullptr
            ? albedo_attachment->atlas_texture
            : fallback_texture;
    }
};

struct FGpuTerrainView {
    explicit FGpuTerrainView(const FTileTree* tile_tree) : order{tile_tree->order},
        refinement_count{tile_tree->refinement_count},
        prepass_parameters{nullptr},
        refine_tiles_parameters{nullptr} {
        const FRDGBufferDesc indirect_desc = FRDGBufferDesc::CreateIndirectDesc(4);
        AllocatePooledBuffer(
            indirect_desc,
            indirect_args_buffer_pooled,
            TEXT("UDLOD.PrepassIndirectArgsBuffer")
        );

        const FRDGBufferDesc prepass_state_desc = FRDGBufferDesc::CreateStructuredDesc<
            PrepassState>(1);
        AllocatePooledBuffer(
            prepass_state_desc,
            prepass_state_storage_pooled,
            TEXT("UDLOD.PrepassStateBuffer")
        );

        const FRDGBufferDesc temporary_tiles_desc = FRDGBufferDesc::CreateStructuredDesc<
            TileCoordinate>(tile_tree->geometry_tile_count);
        AllocatePooledBuffer(
            temporary_tiles_desc,
            temporary_tiles_buffer_pooled,
            TEXT("UDLOD.PrepassTemporaryTilesBuffer")
        );

        const FRDGBufferDesc final_tiles_desc = FRDGBufferDesc::CreateStructuredDesc<
            GeometryTile>(tile_tree->geometry_tile_count);
        AllocatePooledBuffer(
            final_tiles_desc,
            final_tiles_buffer_pooled,
            TEXT("UDLOD.FinalTilesBuffer")
        );

        const FRDGBufferDesc picking_data_desc = FRDGBufferDesc::CreateStructuredDesc(
            sizeof(FPickingDataUpload),
            1u
        );
        AllocatePooledBuffer(
            picking_data_desc,
            picking_data_buffer_pooled,
            TEXT("UDLOD.PickingDataBuffer")
        );
    }

    uint32 order;
    uint32 refinement_count;

    FRDGBufferSRVRef terrain_view_buffer;
    TRefCountPtr<FRDGPooledBuffer> indirect_args_buffer_pooled;
    FRDGBufferRef indirect_args_buffer;
    FRDGBufferUAVRef indirect_args_uav;
    TRefCountPtr<FRDGPooledBuffer> prepass_state_storage_pooled;
    FRDGBufferRef prepass_state_storage;
    FRDGBufferUAVRef prepass_state_buffer;
    TRefCountPtr<FRDGPooledBuffer> temporary_tiles_buffer_pooled;
    FRDGBufferRef temporary_tiles_storage;
    FRDGBufferUAVRef temporary_tiles_buffer;
    TRefCountPtr<FRDGPooledBuffer> final_tiles_buffer_pooled;
    FRDGBufferRef final_tiles_buffer;
    FRDGBufferUAVRef final_tiles_uav;
    FRDGBufferSRVRef final_tiles_srv;
    TRefCountPtr<FRDGPooledBuffer> picking_data_buffer_pooled;
    FRDGBufferRef picking_data_buffer;
    FRDGBufferUAVRef picking_data_uav;
    Prepass* prepass_parameters;
    RefineTiles* refine_tiles_parameters;
    Picking* picking_parameters;

    friend bool operator==(const FGpuTerrainView& a, const FGpuTerrainView& b) {
        return a.order == b.order
            && a.refinement_count == b.refinement_count
            && a.indirect_args_buffer_pooled == b.indirect_args_buffer_pooled
            && a.indirect_args_buffer == b.indirect_args_buffer
            && a.indirect_args_uav == b.indirect_args_uav
            && a.prepass_state_storage_pooled == b.prepass_state_storage_pooled
            && a.prepass_state_storage == b.prepass_state_storage
            && a.prepass_state_buffer == b.prepass_state_buffer
            && a.temporary_tiles_buffer_pooled == b.temporary_tiles_buffer_pooled
            && a.temporary_tiles_storage == b.temporary_tiles_storage
            && a.temporary_tiles_buffer == b.temporary_tiles_buffer
            && a.final_tiles_buffer_pooled == b.final_tiles_buffer_pooled
            && a.final_tiles_buffer == b.final_tiles_buffer
            && a.final_tiles_uav == b.final_tiles_uav
            && a.final_tiles_srv == b.final_tiles_srv
            && a.picking_data_buffer_pooled == b.picking_data_buffer_pooled
            && a.picking_data_buffer == b.picking_data_buffer
            && a.picking_data_uav == b.picking_data_uav;
    }

    static void initialize(
        [[maybe_unused]] FRDGBuilder& gb,
        TOptional<FGpuTerrainView>& gpu_terrain_views,
        const FTileTree* tile_tree
    ) {
        const FRDGBufferDesc temporary_tiles_desc = FRDGBufferDesc::CreateStructuredDesc<
            TileCoordinate>(tile_tree->geometry_tile_count);
        const FRDGBufferDesc final_tiles_desc = FRDGBufferDesc::CreateStructuredDesc<
            GeometryTile>(tile_tree->geometry_tile_count);

        const bool bNeedsRebuild = !gpu_terrain_views.IsSet() ||
            gpu_terrain_views->order != tile_tree->order ||
            gpu_terrain_views->refinement_count != tile_tree->refinement_count ||
            !gpu_terrain_views->temporary_tiles_buffer_pooled.IsValid() ||
            gpu_terrain_views->temporary_tiles_buffer_pooled->Desc != temporary_tiles_desc ||
            !gpu_terrain_views->final_tiles_buffer_pooled.IsValid() ||
            gpu_terrain_views->final_tiles_buffer_pooled->Desc != final_tiles_desc;

        if (bNeedsRebuild) {
            gpu_terrain_views.Reset();
            gpu_terrain_views.Emplace(FGpuTerrainView(tile_tree));
        }
    }

    static void prepare(
        FRDGBuilder& gb,
        TOptional<FGpuTerrainView>& gtv,
        const FTileTree* tile_tree
    ) {
        if (!gtv.IsSet()) {
            UE_LOGFMT(LogTemp, Error, "Attempted to prepare a GPU terrain view that was not set");
            return;
        }

        gtv->terrain_view_buffer = gb.CreateSRV(tile_tree->terrain_view_buffer);
        gtv->indirect_args_buffer = gb.RegisterExternalBuffer(
            gtv->indirect_args_buffer_pooled,
            TEXT("UDLOD.PrepassIndirectArgsBuffer")
        );
        gtv->indirect_args_uav = gb.CreateUAV(gtv->indirect_args_buffer, PF_R32_UINT);
        gtv->prepass_state_storage = gb.RegisterExternalBuffer(
            gtv->prepass_state_storage_pooled,
            TEXT("UDLOD.PrepassStateBuffer")
        );
        gtv->prepass_state_buffer = gb.CreateUAV(gtv->prepass_state_storage);
        gtv->temporary_tiles_storage = gb.RegisterExternalBuffer(
            gtv->temporary_tiles_buffer_pooled,
            TEXT("UDLOD.PrepassTemporaryTilesBuffer")
        );
        gtv->temporary_tiles_buffer = gb.CreateUAV(gtv->temporary_tiles_storage);
        gtv->final_tiles_buffer = gb.RegisterExternalBuffer(
            gtv->final_tiles_buffer_pooled,
            TEXT("UDLOD.FinalTilesBuffer")
        );
        gtv->final_tiles_uav = gb.CreateUAV(gtv->final_tiles_buffer);
        gtv->final_tiles_srv = gb.CreateSRV(gtv->final_tiles_buffer);
        gtv->picking_data_buffer = gb.RegisterExternalBuffer(
            gtv->picking_data_buffer_pooled,
            TEXT("UDLOD.PickingDataBuffer")
        );
        gtv->picking_data_uav = gb.CreateUAV(gtv->picking_data_buffer);

        gtv->prepass_parameters = gb.AllocParameters<Prepass>();
        gtv->prepass_parameters->terrain_view = gtv->terrain_view_buffer;
        gtv->prepass_parameters->state = gtv->prepass_state_buffer;
        gtv->prepass_parameters->indirect_buffer = gtv->indirect_args_uav;
        gtv->prepass_parameters->temporary_tiles = gtv->temporary_tiles_buffer;

        gtv->refine_tiles_parameters = gb.AllocParameters<RefineTiles>();
        gtv->refine_tiles_parameters->terrain_view = gtv->terrain_view_buffer;
        gtv->refine_tiles_parameters->state = gtv->prepass_state_buffer;
        gtv->refine_tiles_parameters->temporary_tiles = gtv->temporary_tiles_buffer;
        gtv->refine_tiles_parameters->final_tiles = gtv->final_tiles_uav;
        gtv->refine_tiles_parameters->IndirectArgs = gtv->indirect_args_buffer;

        gtv->picking_parameters = nullptr;
    }
};

FORCEINLINE uint32 GetTypeHash(const FGpuTerrain& gpu_terrain) {
    return HashCombine(
        HashCombine(
            HashCombine(
                GetTypeHash(gpu_terrain.terrain_buffer),
                GetTypeHash(gpu_terrain.atlas_sampler)
            ),
            GetTypeHash(gpu_terrain.height_attachment_texture)
        ),
        HashCombine(
            GetTypeHash(gpu_terrain.albedo_attachment_texture),
            HashCombine(
                GetTypeHash(
                    reinterpret_cast<UPTRINT>(gpu_terrain.attachments_buffer_pooled.GetReference())
                ),
                HashCombine(
                    GetTypeHash(gpu_terrain.attachments_buffer),
                    GetTypeHash(gpu_terrain.attachments_buffer_srv)
                )
            )
        )
    );
}

FORCEINLINE uint32 GetTypeHash(const FGpuTerrainView& gpu_terrain_view) {
    return HashCombine(
        HashCombine(
            HashCombine(
                HashCombine(
                    GetTypeHash(gpu_terrain_view.order),
                    GetTypeHash(gpu_terrain_view.refinement_count)
                ),
                GetTypeHash(
                    reinterpret_cast<UPTRINT>(
                        gpu_terrain_view.indirect_args_buffer_pooled.GetReference())
                )
            ),
            HashCombine(
                GetTypeHash(gpu_terrain_view.indirect_args_buffer),
                GetTypeHash(gpu_terrain_view.indirect_args_uav)
            )
        ),
        HashCombine(
            HashCombine(
                GetTypeHash(
                    reinterpret_cast<UPTRINT>(
                        gpu_terrain_view.prepass_state_storage_pooled.GetReference())
                ),
                HashCombine(
                    GetTypeHash(gpu_terrain_view.prepass_state_storage),
                    GetTypeHash(gpu_terrain_view.prepass_state_buffer)
                )
            ),
            HashCombine(
                HashCombine(
                    GetTypeHash(
                        reinterpret_cast<UPTRINT>(
                            gpu_terrain_view.temporary_tiles_buffer_pooled.GetReference())
                    ),
                    HashCombine(
                        GetTypeHash(gpu_terrain_view.temporary_tiles_storage),
                        GetTypeHash(gpu_terrain_view.temporary_tiles_buffer)
                    )
                ),
                HashCombine(
                    HashCombine(
                        GetTypeHash(
                            reinterpret_cast<UPTRINT>(
                                gpu_terrain_view.final_tiles_buffer_pooled.GetReference())
                        ),
                        GetTypeHash(gpu_terrain_view.final_tiles_buffer)
                    ),
                    HashCombine(
                        GetTypeHash(gpu_terrain_view.final_tiles_uav),
                        HashCombine(
                            GetTypeHash(gpu_terrain_view.final_tiles_srv),
                            HashCombine(
                                GetTypeHash(
                                    reinterpret_cast<UPTRINT>(
                                        gpu_terrain_view.picking_data_buffer_pooled.
                                        GetReference())
                                ),
                                HashCombine(
                                    GetTypeHash(gpu_terrain_view.picking_data_buffer),
                                    GetTypeHash(gpu_terrain_view.picking_data_uav)
                                )
                            )
                        )
                    )
                )
            )
        )
    );
}
