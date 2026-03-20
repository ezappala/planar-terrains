#pragma once

#include "gpu_terrain_funcs.h"
#include "RenderGraphFwd.h"
#include "RHIStaticStates.h"
#include "terrain_shader_helpers.h"
#include "terrain_tile_atlas.h"

struct FGpuTerrain {
    FGpuTerrain(
        FRDGBuilder& gb,
        const FRDGTextureSRVRef fallback_texture,
        const FTileAtlas& tile_atlas,
        const FGpuTileAtlas& gpu_tile_atlas
    ) : terrain_buffer{tile_atlas.terrain_buffer},
        atlas_sampler{
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
            gpu_tile_atlas.attachments.Contains("Height")
            ? gpu_tile_atlas.attachments["Height"].atlas_texture
            : fallback_texture
        },
        albedo_attachment_texture{
            gpu_tile_atlas.attachments.Contains("Albedo")
            ? gpu_tile_atlas.attachments["Albedo"].atlas_texture
            : fallback_texture
        },
        attachment_configs{
            attachment_config_from_gpu_tile_atlas(gpu_tile_atlas, "Height"),
            attachment_config_from_gpu_tile_atlas(gpu_tile_atlas, "Albedo")
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

    FRDGUniformBufferRef terrain_buffer;
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
        TMap<UTerrain*, FGpuTerrain>& gpu_terrains,
        TMap<UTerrain*, FGpuTileAtlas>& gpu_tile_atlases,
        TMap<UTerrain*, FTileAtlas>& tile_atlases
    ) {
        for (auto& [terrain, tile_atlas] : tile_atlases) {
            const auto gpu_tile_atlas = gpu_tile_atlases[terrain];
            gpu_terrains.Add(
                terrain,
                FGpuTerrain(gb, fallback_texture, tile_atlas, gpu_tile_atlas));
        }
    }

    //TODO: if doing so in the constructor, is this needed?
    //
    // static void prepare(
    //     FRDGBuilder& gb,
    //     TMap<UTerrain*, FGpuTerrain>& gpu_terrains
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
    FGpuTerrainView(FRDGBuilder& gb, const FTileTree& tile_tree) : order{tile_tree.order},
        refinement_count{tile_tree.refinement_count},
        terrain_view_buffer{
            gb.CreateSRV(tile_tree.terrain_view_buffer)
        },
        indirect_buffer{
            gb.CreateUAV(
                gb.CreateBuffer(
                    FRDGBufferDesc::CreateStructuredDesc<IndirectBuffer>(1),
                    TEXT("UDLOD.PrepassIndirectBuffer")
                ))
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
                        tile_tree.geometry_tile_count),
                    TEXT("UDLOD.PrepassTemporaryTilesBuffer")
                ))
        },
        final_tiles_buffer{
            gb.CreateUAV(
                gb.CreateBuffer(
                    FRDGBufferDesc::CreateStructuredDesc<TileCoordinate>(
                        tile_tree.geometry_tile_count),
                    TEXT("UDLOD.FinalTilesBuffer")
                ))
        },
        prepass_parameters{nullptr},
        refine_tiles_parameters{nullptr} {}

    uint32 order;
    uint32 refinement_count;
    FRDGBufferSRVRef terrain_view_buffer;
    FRDGBufferUAVRef indirect_buffer;
    FRDGBufferUAVRef prepass_state_buffer;
    FRDGBufferUAVRef temporary_tiles_buffer;
    FRDGBufferUAVRef final_tiles_buffer;
    Prepass* prepass_parameters;
    RefineTiles* refine_tiles_parameters;

    friend bool operator==(const FGpuTerrainView& a, const FGpuTerrainView& b) {
        return a.order == b.order
            && a.refinement_count == b.refinement_count
            && a.indirect_buffer == b.indirect_buffer
            && a.prepass_state_buffer == b.prepass_state_buffer
            && a.temporary_tiles_buffer == b.temporary_tiles_buffer
            && a.final_tiles_buffer == b.final_tiles_buffer;
    }

    static void initialize(
        FRDGBuilder& gb,
        TMap<UTerrain*, FGpuTerrainView>& gpu_terrain_views,
        TMap<UTerrain*, FTileTree>& tile_trees
    ) {
        for (auto& [terrain, gpu_terrain_view] : gpu_terrain_views) {
            const auto tile_tree = tile_trees[terrain];
            gpu_terrain_view = FGpuTerrainView(gb, tile_tree);
        }
    }

    static void prepare(
        FRDGBuilder& gb,
        TMap<UTerrain*, FGpuTerrainView>& gpu_terrain_views
    ) {
        for (auto& [terrain, gpu_terrain_view] : gpu_terrain_views) {
            gpu_terrain_view.prepass_parameters = gb.AllocParameters<Prepass>();
            gpu_terrain_view.prepass_parameters->terrain_view = gpu_terrain_view.terrain_view_buffer;
            gpu_terrain_view.prepass_parameters->state = gpu_terrain_view.prepass_state_buffer;
            gpu_terrain_view.prepass_parameters->indirect_buffer = gpu_terrain_view.indirect_buffer;
            gpu_terrain_view.prepass_parameters->temporary_tiles = gpu_terrain_view.
                temporary_tiles_buffer;

            gpu_terrain_view.refine_tiles_parameters = gb.AllocParameters<RefineTiles>();
            gpu_terrain_view.refine_tiles_parameters->terrain_view = gpu_terrain_view.terrain_view_buffer;
            gpu_terrain_view.refine_tiles_parameters->state = gpu_terrain_view.prepass_state_buffer;
            gpu_terrain_view.refine_tiles_parameters->indirect_buffer = gpu_terrain_view.
                indirect_buffer;
            gpu_terrain_view.refine_tiles_parameters->temporary_tiles = gpu_terrain_view.
                temporary_tiles_buffer;
            gpu_terrain_view.refine_tiles_parameters->final_tiles = gpu_terrain_view.
                final_tiles_buffer;
        }
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
                GetTypeHash(gpu_terrain_view.indirect_buffer)
            ),
            GetTypeHash(gpu_terrain_view.prepass_state_buffer)
        ),
        HashCombine(
            GetTypeHash(gpu_terrain_view.temporary_tiles_buffer),
            GetTypeHash(gpu_terrain_view.final_tiles_buffer)
        )
    );
}
