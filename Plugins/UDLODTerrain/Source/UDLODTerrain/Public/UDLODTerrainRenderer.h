#pragma once
#include "UDLODTerrainResources.h"
#include "UDLODTerrainSceneProxy.h"

struct FUDLODTerrainRenderer {
    static void ensure_resources(const FUDLODTerrainSceneProxy& proxy, FRHIComputeCommandList& cmd);

    static void add_root_tile_pass(
        FRDGBuilder& graph_builder,
        FRDGBufferUAVRef working_tiles_uav,
        FRDGBufferUAVRef working_count_uav,
        FRDGBufferUAVRef final_count_uav
    );

    static void make_view_uniforms(
        const FSceneView& view,
        FVector3f& out_pre_view_translation,
        float& out_focal_len_px,
        FVector4f out_frustum_planes_tws[]
    );

    static void add_udlod_refine_passes(
        FRDGBuilder& graph_builder,
        const FSceneView& view,
        const FUDLODTerrainSettingsRT& s,
        const FTextureRHIRef& height_texture_rhi,
        const FSamplerStateRHIRef& height_sampler_rhi,
        FRDGBufferRef working_tiles_a, FRDGBufferRef working_count_a,
        FRDGBufferRef working_tiles_b, FRDGBufferRef working_count_b,
        FRDGBufferRef final_tiles, FRDGBufferRef final_count
    );

    static void add_udlod_draw_args_pass(
        FRDGBuilder& graph_builder,
        uint32 index_count_per_instance,
        FRDGBufferRef final_count,
        FRDGBufferUAVRef draw_args_uav
    );

    static void add_udlod_draw_pass(
        FRDGBuilder& graph_builder,
        const FSceneView& view,
        const FUDLODTerrainSettingsRT& s,
        const FTextureRHIRef& height_texture_rhi,
        const FSamplerStateRHIRef& height_sampler_rhi,
        const FUDLODTerrainResources& r,
        FRDGBufferSRVRef final_tiles_srv,
        FRDGBufferRef draw_args,
        FRDGTextureRef scene_color,
        FRDGTextureRef scene_depth
    );

    // void render(FRDGBuilder& graph_builder, const FSceneView& view, const FUDLODTerrainResources& res);
};
