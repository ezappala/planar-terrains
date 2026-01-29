#include "UDLODTerrainRenderer.h"

#include "PixelShaderUtils.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "RHIStaticStates.h"
#include "SceneView.h"
#include "UDLODTerrainShaders.h"
#include "UDLODTerrainViewExtension.h"
#include "Engine/GameViewportClient.h"
#include "Logging/StructuredLog.h"

///
/// @brief Given a [`FUDLODTerrainSceneProxy`] that has a zero-value grid resolution, initialize its settings and resources
/// @param proxy the proxy to ensure resources for
/// @param cmd a reference to an [`FRHIComputeCommandList`] which is passed down to [`create_buffer`] which initializes
///     the proxy's resources vertex and pixel buffers.
///
void FUDLODTerrainRenderer::ensure_resources(const FUDLODTerrainSceneProxy& proxy, FRHIComputeCommandList& cmd) {
    if (proxy.resources().grid_resolution == 0) {
        const FUDLODTerrainSettingsRT settings_rt = proxy.settings();
        proxy.resources().init(settings_rt.grid_resolution, settings_rt.max_tiles, cmd);
    }
}

///
/// @brief Adds a tile pass for just the root tile: the tile which the camera is currently in.
/// @param graph_builder a reference to a [`FRDGBuilder`], should be the same one used across all UDLOD compute and
///     raster passes
/// @param working_tiles_uav buffer of tiles currently being processed by the compute shader
/// @param working_count_uav length of the [`working_tiles_uav`] buffer, will always be 1 for the root tile pass
/// @param final_count_uav output parameter of the final tiel count
/// @remark Root tile pass is a 1-thread compute shader which performs the following:
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

    p->root_working_tiles = working_tiles_uav;
    p->root_working_count = working_count_uav;
    p->root_final_count = final_count_uav;

    FComputeShaderUtils::AddPass(
        graph_builder,
        RDG_EVENT_NAME("UDLOD.RootTile"),
        ERDGPassFlags::Compute,
        cs,
        p,
        FIntVector(1, 1, 1)
    );
}

///
/// @brief Create the uniform buffer of Frustum Planes so that the compute shader understands what the camera is
///     "looking at".
/// @param view Reference the representation of the current [`FSceneView`]
/// @param out_pre_view_translation Translation component of the world space camera transform, output param
/// @param out_focal_len_px The focal length of the main camera in pixels, output param
/// @param out_frustum_planes_tws Frustum planes of the main camera in translated world space, output param.
///
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

///
/// @brief Add passes to refine the tiles computed in [`make_view_uniforms`]. This function specifically
/// @param graph_builder
/// @param view
/// @param s
/// @param height_texture_rhi
/// @param height_sampler_rhi
/// @param working_tiles_a
/// @param working_count_a
/// @param working_tiles_b
/// @param working_count_b
/// @param final_tiles
/// @param final_count
///
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
        AddClearUAVPass(graph_builder, graph_builder.CreateUAV(out_count), 0);

        // build indirect dispatch args from in_count
        constexpr uint32 group_size = 64;
        const FRDGBufferRef indirect_args = FComputeShaderUtils::AddIndirectArgsSetupCsPass1D(graph_builder,
            GMaxRHIFeatureLevel, in_count, TEXT("IndirectArgsBuffer"), group_size);
        const FRDGBufferUAVRef indirect_args_uav = graph_builder.CreateUAV(indirect_args);

        // setup args: [ceil(in_count/group_size), 1, 1]
        TShaderMapRef<FUDLOD_MakeDispatchArgsCS> args_cs(GetGlobalShaderMap(GMaxRHIFeatureLevel));
        check(args_cs.IsValid())
        auto* args_p = graph_builder.AllocParameters<FUDLOD_MakeDispatchArgsCS::FParameters>();
        auto* uniform_args_p = graph_builder.AllocParameters<UDLOD_DispatchCB>();
        uniform_args_p->group_size = group_size;

        auto* uniform_buffer_args_p = graph_builder.CreateUniformBuffer<UDLOD_DispatchCB>(uniform_args_p);

        args_p->dispatch_in_count = graph_builder.CreateSRV(in_count);
        args_p->dispatch_out_args = indirect_args_uav;
        args_p->DispatchCB = uniform_buffer_args_p;
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
        auto* uniform_p = graph_builder.AllocParameters<UDLOD_RefineCB>();
        uniform_p->world_min_xy_ws = s.world_min_xy_ws;
        uniform_p->world_size_xy_ws = s.world_size_xy_ws;
        uniform_p->height_minmax_ws = FVector2f(s.height_min_ws, s.height_max_ws);

        uniform_p->grid_resolution = s.grid_resolution;
        uniform_p->max_lod = s.max_lod;
        uniform_p->error_pixels = s.error_pixels;
        uniform_p->height_error_0ws = s.height_error_0ws;
        uniform_p->focal_len_px = focal_len_px;
        uniform_p->morph_start_ratio = s.morph_start_ratio;
        uniform_p->pre_view_translation = pre_view_translation;

        p->in_tiles = graph_builder.CreateSRV(FRDGBufferSRVDesc(in_tiles));
        p->in_count = graph_builder.CreateSRV(in_count);
        p->out_tiles = graph_builder.CreateUAV(out_tiles);
        p->out_count = graph_builder.CreateUAV(out_count);
        p->final_tiles = graph_builder.CreateUAV(final_tiles);
        p->final_count = graph_builder.CreateUAV(final_count);

        p->height_texture = height_texture_rhi;
        p->height_sampler = height_sampler_rhi;

        // ReSharper disable once CppLocalVariableMayBeConst, reason idk compilers are dumb. this should be constexpr
        int32 N = FMath::Min(6, uniform_p->frustum_planes_tws.Num());
        for (int32 i = 0; i < N; ++i) { uniform_p->frustum_planes_tws[i] = frustum_planes_tws[i]; }

        auto* uniform_buffer_p = graph_builder.CreateUniformBuffer<UDLOD_RefineCB>(uniform_p);
        p->RefineCB = uniform_buffer_p;

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
    auto* uniform_p = graph_builder.AllocParameters<UDLOD_DrawArgsCB>();
    uniform_p->index_count_per_instance = index_count_per_instance;

    auto* uniform_buffer_p = graph_builder.CreateUniformBuffer<UDLOD_DrawArgsCB>(uniform_p);

    p->draw_in_final_count = graph_builder.CreateSRV(final_count);
    p->draw_out_args = draw_args_uav;
    p->DrawArgsCB = uniform_buffer_p;

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
    FRDGBuilder& graph_builder,
    const FSceneView& view,
    const FUDLODTerrainSettingsRT& s,
    const FTextureRHIRef& height_texture_rhi,
    const FSamplerStateRHIRef& height_sampler_rhi,
    const FUDLODTerrainResources& r,
    const FRDGBufferSRVRef final_tiles_srv,
    const FRDGBufferRef draw_args,
    const FRDGTextureRef scene_color,
    const FRDGTextureRef scene_depth
) {
    auto* pass_params = graph_builder.AllocParameters<FUDLOD_TerrainDrawPassParameters>();
    auto* uniform_pass_params = graph_builder.AllocParameters<UDLOD_VSParamsCB>();

    uniform_pass_params->vs_world_min_xy_ws = s.world_min_xy_ws;
    uniform_pass_params->vs_world_size_xy_ws = s.world_size_xy_ws;
    uniform_pass_params->vs_height_minmax_ws = {s.height_min_ws, s.height_max_ws};
    uniform_pass_params->vs_grid_resolution = s.grid_resolution;

    auto* uniform_buffer_pass_params = graph_builder.CreateUniformBuffer<UDLOD_VSParamsCB>(uniform_pass_params);

    const auto* global_shader_map = GetGlobalShaderMap(GMaxRHIFeatureLevel);
    const TShaderMapRef<FUDLOD_TerrainVS> vs{global_shader_map};
    check(vs.IsValid())
    const TShaderMapRef<FUDLOD_TerrainPS> ps{global_shader_map};
    check(ps.IsValid())

    pass_params->shader.view = view.ViewUniformBuffer;
    pass_params->shader.vs_final_tiles = final_tiles_srv;
    pass_params->shader.vs_height_texture = height_texture_rhi;
    pass_params->shader.vs_height_sampler = height_sampler_rhi;
    pass_params->shader.RenderTargets[0] = {scene_color, ERenderTargetLoadAction::ELoad};
    pass_params->shader.RenderTargets.DepthStencil = {
        scene_depth,
        ERenderTargetLoadAction::ELoad, ERenderTargetLoadAction::ELoad, FExclusiveDepthStencil::DepthRead_StencilWrite
    };
    pass_params->shader.VSParamsCB = uniform_buffer_pass_params;

    pass_params->indirect_args = draw_args;

    graph_builder.AddPass(
        RDG_EVENT_NAME("UDLOD.Draw"),
        pass_params,
        ERDGPassFlags::Raster,
        [&view, &r, ps, vs, pass_params](FRHICommandList& cmd) {
            check(r.vertex_decl.IsValid());
            check(r.grid_vb.IsValid());
            check(r.grid_ib.IsValid());
            check(ps.IsValid());
            check(vs.IsValid());

            const FIntRect vr = view.CameraConstrainedViewRect;
            cmd.SetViewport(vr.Min.X, vr.Min.Y, 0., vr.Max.X, vr.Max.Y, 0.);

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
            SetShaderParameters(cmd, vs, vs.GetVertexShader(), pass_params->shader);
            SetShaderParameters(cmd, ps, ps.GetPixelShader(), pass_params->shader);

            cmd.SetStreamSource(0, r.grid_vb, 0);
            cmd.DrawIndexedPrimitiveIndirect(r.grid_ib, pass_params->indirect_args->GetRHI(), 0);
        }
    );
}
