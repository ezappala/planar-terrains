#pragma once

#include "gpu_terrain_funcs.h"
#include "RenderGraphFwd.h"
#include "RHIStaticStates.h"
#include "terrain_shaders.h"
#include "terrain_tile_tree.h"

struct FGpuTerrain {
    FGpuTerrain(
        FRDGBuilder& gb,
        const FRDGTextureSRVRef fallback_texture,
        TOptional<FGpuTileAtlas>& gpu_tile_atlas
    ) : atlas_sampler{
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
        height_attachment_texture{
            gpu_tile_atlas.IsSet() && find_attachment_by_label(
                gpu_tile_atlas->attachments,
                TEXT("height")) != nullptr
            ? find_attachment_by_label(gpu_tile_atlas->attachments, TEXT("height"))->atlas_texture
            : fallback_texture
        },
        albedo_attachment_texture{
            gpu_tile_atlas.IsSet() && find_attachment_by_label(
                gpu_tile_atlas->attachments,
                TEXT("albedo")) != nullptr
            ? find_attachment_by_label(gpu_tile_atlas->attachments, TEXT("albedo"))->atlas_texture
            : fallback_texture
        },
        attachment_configs{
            attachment_config_from_gpu_tile_atlas(gpu_tile_atlas, TEXT("height")),
            attachment_config_from_gpu_tile_atlas(gpu_tile_atlas, TEXT("albedo"))
        },
        attachments_buffer{
            gb.CreateSRV(
                CreateStructuredBuffer(
                    gb,
                    TEXT("UDLOD.AttachmentsBuffer"),
                    sizeof(Attachments),
                    1,
                    attachment_configs.GetData(),
                    sizeof(attachment_configs.GetData()) * attachment_configs.Num(),
                    ERDGInitialDataFlags::None
                )
            )
        }
    // height_attachment_buffer{gb.CreateUniformBuffer(&height_attachment_config)},
    // albedo_attachment_buffer{gb.CreateUniformBuffer(&albedo_attachment_config)} {}
    {}

    TUniformBufferRef<Terrain> terrain_buffer;
    FSamplerStateRHIRef atlas_sampler;
    FRDGTextureSRVRef height_attachment_texture;
    FRDGTextureSRVRef albedo_attachment_texture;
    TStaticArray<AttachmentConfig, 2> attachment_configs;
    // Attachments attachments;

    FRDGBufferSRVRef attachments_buffer;
    // FRDGUniformBufferRef height_attachment_buffer;
    // FRDGUniformBufferRef albedo_attachment_buffer;

    friend bool operator ==(const FGpuTerrain& a, const FGpuTerrain& b) {
        return a.terrain_buffer == b.terrain_buffer
            && a.atlas_sampler == b.atlas_sampler
            && a.height_attachment_texture == b.height_attachment_texture
            && a.albedo_attachment_texture == b.albedo_attachment_texture
            && a.attachments_buffer == b.attachments_buffer;
    }

    friend bool operator !=(const FGpuTerrain& a, const FGpuTerrain& b) { return !(a == b); }

    static void initialize(
        FRDGBuilder& gb,
        const FRDGTextureSRVRef fallback_texture,
        TOptional<FGpuTerrain>& gpu_terrains,
        TOptional<FGpuTileAtlas>& gpu_tile_atlas
    ) {
        gpu_terrains.Reset();
        gpu_terrains.Emplace(FGpuTerrain(gb, fallback_texture, gpu_tile_atlas));
    }

    //TODO: if doing so in the constructor, is this needed?
    //
    // static void prepare(
    //     FRDGBuilder& gb,
    //     TMap<TObjectPtr<UTerrain>, FGpuTerrain>& gpu_terrains
    // ) {
    //     for (auto& [terrain, gpu_terrain] : gpu_terrains) {
    //         const auto terrain_buffer = gpu_terrain.terrain_buffer;
    //         const auto attachments_buffer = gpu_terrain.attachments_buffer;
    //         const auto height_attachment_config = gpu_terrain.attachment_configs[0];
    //         const auto albedo_attachment_config = gpu_terrain.attachment_configs[1];
    //
    //         gb.AddPass(
    //             RDG_EVENT_NAME("UDLOD.PrepareGpuTerrain"),
    //             ERDGPassFlags::NeverCull | ERDGPassFlags::Raster,
    //             [ terrain_buffer, attachments_buffer, height_attachment_config, albedo_attachment_config ](
    //             FRHICommandList& cmd
    //         ) {
    //                 cmd.UpdateUniformBuffer(terrain_buffer->GetRHI(), &terrain_buffer);
    //                 cmd.UpdateUniformBuffer(
    //                     height_attachment_buffer->GetRHI(),
    //                     &height_attachment_config);
    //                 cmd.UpdateUniformBuffer(
    //                     albedo_attachment_buffer->GetRHI(),
    //                     &albedo_attachment_config);
    //             });
    //     }
    // }
};

struct FGpuTerrainView {
    FGpuTerrainView(FRDGBuilder& gb, const FTileTree* tile_tree) : order{tile_tree->order},
        refinement_count{tile_tree->refinement_count},
        terrain_view_buffer{
            gb.CreateSRV(tile_tree->terrain_view_buffer)
        },
        indirect_args_buffer{
            gb.CreateBuffer(
                FRDGBufferDesc::CreateIndirectDesc(4),
                TEXT("UDLOD.PrepassIndirectArgsBuffer")
            )
        },
        indirect_args_uav{
            gb.CreateUAV(indirect_args_buffer, PF_R32_UINT)
        },
        prepass_state_buffer{
            gb.CreateUAV(
                gb.CreateBuffer(
                    FRDGBufferDesc::CreateStructuredDesc<PrepassState>(1),
                    TEXT("UDLOD.PrepassStateBuffer")
                ))
        },
        temporary_tiles_buffer{
            gb.CreateUAV(
                gb.CreateBuffer(
                    FRDGBufferDesc::CreateStructuredDesc<TileCoordinate>(
                        tile_tree->geometry_tile_count),
                    TEXT("UDLOD.PrepassTemporaryTilesBuffer")
                ))
        },
        final_tiles_buffer{
            gb.CreateBuffer(
                FRDGBufferDesc::CreateStructuredDesc<GeometryTile>(
                    tile_tree->geometry_tile_count),
                TEXT("UDLOD.FinalTilesBuffer")
            )
        },
        final_tiles_uav{
            gb.CreateUAV(final_tiles_buffer)
        },
        final_tiles_srv{
            gb.CreateSRV(final_tiles_buffer)
        },
        prepass_parameters{nullptr},
        refine_tiles_parameters{nullptr} {}

    uint32 order;
    uint32 refinement_count;

    FRDGBufferSRVRef terrain_view_buffer;
    FRDGBufferRef indirect_args_buffer;
    FRDGBufferUAVRef indirect_args_uav;
    FRDGBufferUAVRef prepass_state_buffer;
    FRDGBufferUAVRef temporary_tiles_buffer;
    FRDGBufferRef final_tiles_buffer;
    FRDGBufferUAVRef final_tiles_uav;
    FRDGBufferSRVRef final_tiles_srv;
    Prepass* prepass_parameters;
    RefineTiles* refine_tiles_parameters;

    friend bool operator==(const FGpuTerrainView& a, const FGpuTerrainView& b) {
        return a.order == b.order
            && a.refinement_count == b.refinement_count
            && a.indirect_args_buffer == b.indirect_args_buffer
            && a.indirect_args_uav == b.indirect_args_uav
            && a.prepass_state_buffer == b.prepass_state_buffer
            && a.temporary_tiles_buffer == b.temporary_tiles_buffer
            && a.final_tiles_buffer == b.final_tiles_buffer
            && a.final_tiles_uav == b.final_tiles_uav
            && a.final_tiles_srv == b.final_tiles_srv;
    }

    static void initialize(
        FRDGBuilder& gb,
        TOptional<FGpuTerrainView>& gpu_terrain_views,
        const FTileTree* tile_tree
    ) {
        gpu_terrain_views.Reset();
        gpu_terrain_views.Emplace(FGpuTerrainView(gb, tile_tree));
    }

    static void prepare(
        FRDGBuilder& gb,
        TOptional<FGpuTerrainView>& gtv
    ) {
        if (!gtv.IsSet()) {
            UE_LOGFMT(LogTemp, Error, "Attempted to prepare a GPU terrain view that was not set");
            return;
        }
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
            GetTypeHash(gpu_terrain.attachments_buffer)
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
                GetTypeHash(gpu_terrain_view.indirect_args_buffer)
            ),
            GetTypeHash(gpu_terrain_view.indirect_args_uav)
        ),
        HashCombine(
            GetTypeHash(gpu_terrain_view.prepass_state_buffer),
            HashCombine(
                HashCombine(
                    GetTypeHash(gpu_terrain_view.temporary_tiles_buffer),
                    GetTypeHash(gpu_terrain_view.final_tiles_buffer)
                ),
                HashCombine(
                    GetTypeHash(gpu_terrain_view.final_tiles_uav),
                    GetTypeHash(gpu_terrain_view.final_tiles_srv)
                )
            )
        )
    );
}
