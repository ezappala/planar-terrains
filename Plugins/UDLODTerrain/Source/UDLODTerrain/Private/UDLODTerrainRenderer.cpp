#include "UDLODTerrainRenderer.h"

#include <any>

#include "PixelShaderUtils.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "RHIStaticStates.h"
#include "SceneView.h"
#include "UDLODTerrainShaders.h"
#include "UDLODTerrainViewExtension.h"
#include "Engine/GameViewportClient.h"
#include "Logging/StructuredLog.h"

void FUDLODTerrainRenderer::ensure_resources(const FUDLODTerrainSceneProxy& proxy, FRHIComputeCommandList& cmd) {
    if (proxy.resources().grid_resolution == 0) {
        const FUDLODTerrainSettingsRT settings_rt = proxy.settings();
        proxy.resources().init(settings_rt.grid_resolution, settings_rt.max_tiles, cmd);
    }
}

/// @details root tile pass is a 1-thread compute shader which performs the following:
/// - writes WorkingTiles[0] = {Lod=0, X=0, Y=0, ...}
/// - writes WorkingCount = 1
/// - clears FinalCount = 0
void FUDLODTerrainRenderer::add_root_tile_pass(
    FRDGBuilder& graph_builder,
    const FRDGBufferUAVRef working_tiles_uav, const FRDGBufferUAVRef working_count_uav,
    const FRDGBufferUAVRef final_count_uav
) {
    const TShaderMapRef<FUDLOD_RootTileCS> cs(GetGlobalShaderMap(GMaxRHIFeatureLevel));
    check(cs.IsValid());
    auto* p = graph_builder.AllocParameters<FUDLOD_RootTileCS::FParameters>();

    p->working_tiles_uav = working_tiles_uav;
    p->working_count_uav = working_count_uav;
    p->final_count_uav = final_count_uav;

    FComputeShaderUtils::AddPass(
        graph_builder,
        RDG_EVENT_NAME("UDLOD.RootTile"),
        ERDGPassFlags::Compute,
        cs,
        p,
        FIntVector(1, 1, 1)
    );
}

void FUDLODTerrainRenderer::make_view_uniforms(
    const FSceneView& view,
    FVector3f& out_pre_view_translation,
    float& out_focal_len_px,
    FVector4f out_frustum_planes_tws[6]
) {
    out_pre_view_translation = static_cast<FVector3f>(view.ViewMatrices.GetPreViewTranslation());
    for (int i = 0; i < 6; ++i) { out_frustum_planes_tws[i] = FVector4f(0, 0, 0, 0); }

    const float width = static_cast<float>(view.UnscaledViewRect.Width());
    const float height = static_cast<float>(view.UnscaledViewRect.Height());
    const FMatrix44f proj{view.ViewMatrices.GetProjectionMatrix()};

    // Focal length in pixels - (width/2) * proj[0][0] (perspective)
    const float fx = 0.5 * width * proj.M[0][0];
    const float fy = 0.5 * height * proj.M[1][1];
    out_focal_len_px = FMath::Min(fx, fy);

    // Frustum planes in world; conversion to TWS by adjusting plane w
    // plane: dot(n, x) + d = 0, with x' = x + t => d' d - dot(n, t)

    // BUG: should there be 6 planes on a pyramidal view frustum? (near, far, and the 4 connecting sides)
    const FConvexVolume& fv = view.GetCullingFrustum();
    const int32 N = FMath::Min(6, fv.Planes.Num());
    for (int32 i = 0; i < N; ++i) {
        const FPlane& p = fv.Planes[i];
        const FVector n{p.X, p.Y, p.Z};
        const float d = p.W;
        const float dTWS = d - FVector::DotProduct(n, static_cast<FVector>(out_pre_view_translation));
        out_frustum_planes_tws[i] = FVector4f(
            static_cast<float>(p.X),
            static_cast<float>(p.Y),
            static_cast<float>(p.Z),
            dTWS
        );
    }
}

void FUDLODTerrainRenderer::add_udlod_refine_passes(
    FRDGBuilder& graph_builder, const FSceneView& view,
    const FUDLODTerrainSettingsRT& s,
    const FTextureRHIRef& height_texture_rhi, const FSamplerStateRHIRef& height_sampler_rhi,
    const FRDGBufferRef working_tiles_a, const FRDGBufferRef working_count_a,
    const FRDGBufferRef working_tiles_b, const FRDGBufferRef working_count_b,
    const FRDGBufferRef final_tiles, const FRDGBufferRef final_count
) {
    FVector3f pre_view_translation;
    float focal_len_px = 0.;
    FVector4f frustum_planes_tws[6];
    make_view_uniforms(view, pre_view_translation, focal_len_px, frustum_planes_tws);

    FRDGBufferRef in_tiles = working_tiles_a;
    FRDGBufferRef in_count = working_count_a;
    FRDGBufferRef out_tiles = working_tiles_b;
    FRDGBufferRef out_count = working_count_b;

    for (uint32 pass = 0; pass < s.max_lod; ++pass) {
        // clear out_count (1 uint)
        AddClearUAVPass(graph_builder, graph_builder.CreateUAV(out_count, PF_R32_UINT), 0);

        // build indirect dispatch args from in_count
        constexpr uint32 group_size = 64;
        const FRDGBufferRef indirect_args = FComputeShaderUtils::AddIndirectArgsSetupCsPass1D(graph_builder,
            GMaxRHIFeatureLevel, in_count, TEXT("IndirectArgsBuffer"), group_size);
        const FRDGBufferUAVRef indirect_args_uav = graph_builder.CreateUAV(indirect_args, PF_R32_UINT);

        // setup args: [ceil(in_count/group_size), 1, 1]
        TShaderMapRef<FUDLOD_MakeDispatchArgsCS> args_cs(GetGlobalShaderMap(GMaxRHIFeatureLevel));
        check(args_cs.IsValid())
        auto* args_p = graph_builder.AllocParameters<FUDLOD_MakeDispatchArgsCS::FParameters>();
        args_p->in_count_srv = graph_builder.CreateSRV(in_count, PF_R32_UINT);
        args_p->out_args_uav = indirect_args_uav;
        args_p->group_size = group_size;
        FComputeShaderUtils::AddPass(
            graph_builder,
            RDG_EVENT_NAME("UDLOD.MakeDispatchArgs"),
            ERDGPassFlags::Compute,
            args_cs,
            args_p,
            FIntVector(1, 1, 1)
        );

        // refine pass
        TShaderMapRef<FUDLOD_RefineCS> cs(GetGlobalShaderMap(GMaxRHIFeatureLevel));
        check(cs.IsValid())
        auto* p = graph_builder.AllocParameters<FUDLOD_RefineCS::FParameters>();

        p->in_tiles_srv = graph_builder.CreateSRV(FRDGBufferSRVDesc(in_tiles));
        p->in_count_srv = graph_builder.CreateSRV(in_count, PF_R32_UINT);
        p->out_tiles_uav = graph_builder.CreateUAV(out_tiles);
        p->out_count_uav = graph_builder.CreateUAV(out_count, PF_R32_UINT);
        p->final_tiles_uav = graph_builder.CreateUAV(final_tiles);
        p->final_count_uav = graph_builder.CreateUAV(final_count, PF_R32_UINT);

        p->height_texture = height_texture_rhi;
        p->height_sampler = height_sampler_rhi;

        p->world_min_xy_ws = s.world_min_xy_ws;
        p->world_size_xy_ws = s.world_size_xy_ws;
        p->height_minmax_ws = FVector2f(s.height_min_ws, s.height_max_ws);

        p->grid_resolution = s.grid_resolution;
        p->max_lod = s.max_lod;
        p->error_pixels = s.error_pixels;
        p->height_error_0ws = s.height_error_0ws;
        p->focal_len_px = focal_len_px;
        p->morph_start_ratio = s.morph_start_ratio;
        p->pre_view_translation = pre_view_translation;

        // ReSharper disable once CppLocalVariableMayBeConst, reason idk compilers are dumb. this should be constexpr
        int32 N = FMath::Min(6, p->frustum_planes_tws.Num());
        for (int32 i = 0; i < N; ++i) { p->frustum_planes_tws[i] = frustum_planes_tws[i]; }

        check(p != nullptr);
        check(indirect_args != nullptr);

        constexpr uint32 indirect_args_offset = 0;

        FComputeShaderUtils::AddPass(
            graph_builder,
            RDG_EVENT_NAME("UDLOD.Refine_%u", pass),
            ERDGPassFlags::Compute, cs, p, indirect_args, indirect_args_offset);

        Swap(in_tiles, out_tiles);
        Swap(in_count, out_count);

        // early out: if in_count == 0, no more tiles to refine
        // note: this is done on the gpu
    }
}

void FUDLODTerrainRenderer::add_udlod_draw_args_pass(
    FRDGBuilder& graph_builder, const uint32 index_count_per_instance, const FRDGBufferRef final_count,
    const FRDGBufferUAVRef draw_args_uav
) {
    const TShaderMapRef<FUDLOD_DrawArgsCS> cs(GetGlobalShaderMap(GMaxRHIFeatureLevel));
    check(cs.IsValid());
    auto* p = graph_builder.AllocParameters<FUDLOD_DrawArgsCS::FParameters>();

    p->final_count_srv = graph_builder.CreateSRV(final_count, PF_R32_UINT);
    p->draw_args_uav = draw_args_uav;
    p->index_count_per_instance = index_count_per_instance;

    FComputeShaderUtils::AddPass(
        graph_builder,
        RDG_EVENT_NAME("UDLOD.DrawArgs"),
        ERDGPassFlags::Compute,
        cs,
        p,
        FIntVector(1, 1, 1)
    );
}

void FUDLODTerrainRenderer::add_udlod_draw_pass(
    FRDGBuilder& graph_builder, const FSceneView& view,
    const FUDLODTerrainSettingsRT& s,
    const FTextureRHIRef& height_texture_rhi, const FSamplerStateRHIRef& height_sampler_rhi,
    const FUDLODTerrainResources& r, const FRDGBufferSRVRef final_tiles_srv,
    const FRDGBufferRef draw_args, const FRDGTextureRef scene_color, const FRDGTextureRef scene_depth
) {
    const auto* global_shader_map = GetGlobalShaderMap(GMaxRHIFeatureLevel);
    const TShaderMapRef<FUDLOD_TerrainVS> vs{global_shader_map};
    check(vs.IsValid())
    const TShaderMapRef<FUDLOD_TerrainPS> ps{global_shader_map};
    check(ps.IsValid())

    auto* pass_params = graph_builder.AllocParameters<FUDLOD_TerrainPassParameters>();
    pass_params->view = view.ViewUniformBuffer;
    pass_params->final_tiles = final_tiles_srv;
    pass_params->height_texture = height_texture_rhi;
    pass_params->height_sampler = height_sampler_rhi;
    pass_params->world_min_xy_ws = s.world_min_xy_ws;
    pass_params->world_size_xy_ws = s.world_size_xy_ws;
    pass_params->height_minmax_ws = FVector2f(s.height_min_ws, s.height_max_ws);
    pass_params->grid_resolution = s.grid_resolution;
    pass_params->RenderTargets[0] = FRenderTargetBinding(scene_color, ERenderTargetLoadAction::ELoad);
    pass_params->RenderTargets.DepthStencil = FDepthStencilBinding(
        scene_depth,
        ERenderTargetLoadAction::ELoad, ERenderTargetLoadAction::ELoad, FExclusiveDepthStencil::DepthRead_StencilNop
    );

    graph_builder.AddPass(
        RDG_EVENT_NAME("UDLOD.Draw"),
        pass_params,
        ERDGPassFlags::Raster,
        [&](FRHICommandList& cmd) {
            check(r.vertex_decl.IsValid());
            check(r.grid_vb.IsValid());
            check(r.grid_ib.IsValid());
            check(ps.IsValid());
            check(vs.IsValid());

            FGraphicsPipelineStateInitializer pso{};
            cmd.ApplyCachedRenderTargets(pso);

            pso.PrimitiveType = PT_TriangleStrip;
            pso.BlendState = TStaticBlendState<>::GetRHI();
            pso.RasterizerState = TStaticRasterizerState<>::GetRHI();
            pso.DepthStencilState = TStaticDepthStencilState<true, CF_LessEqual>::GetRHI();

            pso.BoundShaderState.VertexDeclarationRHI = r.vertex_decl;
            pso.BoundShaderState.VertexShaderRHI = vs.GetVertexShader();
            pso.BoundShaderState.PixelShaderRHI = ps.GetPixelShader();

            SetGraphicsPipelineState(cmd, pso, 0);
            SetShaderParameters(cmd, vs, vs.GetVertexShader(), *pass_params);
            SetShaderParameters(cmd, ps, ps.GetPixelShader(), *pass_params);

            cmd.SetStreamSource(0, r.grid_vb, 0);
            cmd.DrawIndexedPrimitiveIndirect(r.grid_ib, draw_args->GetRHI(), 0);
        }
    );
}
