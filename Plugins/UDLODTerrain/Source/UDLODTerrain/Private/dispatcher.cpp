#include "dispatcher.h"
#include "CommonRenderResources.h"
#include "EngineModule.h"
#include "GlobalShader.h"
#include "PipelineStateCache.h"
#include "RenderGraphBuilder.h"
#include "RenderGraphUtils.h"
#include "RenderTargetPool.h"
#include "render_shaders.h"
#include "RHICommandList.h"
#include "RHIStaticStates.h"
#include "Shader.h"
#include "ShaderParameterStruct.h"
#include "TextureResource.h"
#include "tile.h"
#include "UDLODTerrain.h"
#include "Logging/StructuredLog.h"
#include "Runtime/Renderer/Private/SceneRendering.h"

static constexpr uint32 GSubdivisions = 64;
static constexpr uint32 GThreadsPerGroup = 128;

static uint32 vertices_per_tile(const uint32 n) { return n * (2 * (n + 1) + 2); }

FCSDispatcher::FCSDispatcher() : heightmap{nullptr}, heightmap_rdg{nullptr} {}

void FCSDispatcher::begin_pass() {
#if defined(BASIC_MODE) && BASIC_MODE
#else
    UE_LOGFMT(LogUDLODTerrain, Log, "[FCSDispatcher] Registering post-opaque render delegate.");
    if (on_post_opaque_render_delegate.IsValid()) {
        UE_LOGFMT(LogUDLODTerrain, Log, "[FCSDispatcher] Post-opaque render delegate already registered, skipping.");
        return;
    }

    on_post_opaque_render_delegate = GetRendererModule().RegisterPostOpaqueRenderDelegate(
        FPostOpaqueRenderDelegate::CreateRaw(this, &FCSDispatcher::add_compute_pass));
    UE_LOGFMT(LogUDLODTerrain, Log, "[FCSDispatcher] Post-opaque render delegate registered successfully.");
#endif
}

void FCSDispatcher::end_pass() const {
#if defined(BASIC_MODE) && BASIC_MODE
#else
    UE_LOGFMT(LogUDLODTerrain, Log, "[FCSDispatcher] Unregistering post-opaque render delegate.");
    GetRendererModule().RemovePostOpaqueRenderDelegate(on_post_opaque_render_delegate);
    UE_LOGFMT(LogUDLODTerrain, Log, "[FCSDispatcher] Post-opaque render delegate unregistered successfully.");
#endif
}

#if defined(BASIC_MODE) && BASIC_MODE
void FCSDispatcher::add_basic_draw_pass(FPostOpaqueRenderParameters& PostOpaqueRenderParameters) {
        UE_LOGFMT(LogUDLODTerrain, Log, "[FCSDispatcher] Adding basic draw pass to render graph.");
        FRDGBuilder& gb = *PostOpaqueRenderParameters.GraphBuilder;
        const FViewInfo& view_info = *PostOpaqueRenderParameters.View;
        FRDGTexture* scene_color = PostOpaqueRenderParameters.ColorTexture;
        FRDGTexture* scene_depth = PostOpaqueRenderParameters.DepthTexture;

        checkf(scene_color != nullptr, TEXT("Scene color texture is null. Cannot proceed with basic draw pass."));
        checkf(scene_depth != nullptr, TEXT("Scene depth texture is null. Cannot proceed with basic draw pass."));
        UE_LOGFMT(LogUDLODTerrain, Log, "[FCSDispatcher] Scene textures are valid. Color: {cdx}x{cdy}, Depth: {ddx}x{ddy}",
            scene_color->Desc.Extent.X, scene_color->Desc.Extent.Y,
            scene_depth->Desc.Extent.X, scene_depth->Desc.Extent.Y);

        const TShaderMapRef<FBasicPS> PS(view_info.ShaderMap);
        auto* pass_params = gb.AllocParameters<FBasicPS::FParameters>();
        // pass_params->in_heightmap = heightmap_rdg;
        // pass_params->in_scene_color = scene_color;
        // pass_params->in_scene_depth = scene_depth;

    AddDrawScreenPass(gb, RDG_EVENT_NAME("UDLDO.BasicDraw"), view_info, )
}
#endif

// ReSharper disable once CppParameterMayBeConstPtrOrRef, reason: delegates can't be made with const parameters
void FCSDispatcher::add_compute_pass(FPostOpaqueRenderParameters& PostOpaqueRenderParameters) {
    UE_LOGFMT(LogUDLODTerrain, Log, "[FCSDispatcher] add_compute_pass called. Setting up compute passes for UDLOD terrain.");
    checkf(heightmap != nullptr, TEXT("Height texture input is null. Ensure it is set before rendering."));
    UE_LOGFMT(LogUDLODTerrain, Log, "[FCSDispatcher] Heightmap texture is valid. Size: {x}x{y}, Format: {fmt}",
        heightmap->GetSizeX(), heightmap->GetSizeY(), heightmap->GetPixelFormat());

    FRDGBuilder& gb = *PostOpaqueRenderParameters.GraphBuilder;
    const FViewInfo& view_info = *PostOpaqueRenderParameters.View;
    FRDGTexture* scene_color = PostOpaqueRenderParameters.ColorTexture;
    FRDGTexture* scene_depth = PostOpaqueRenderParameters.DepthTexture;

    checkf(scene_color != nullptr, TEXT("Scene color texture is null. Cannot proceed with UDLOD passes."));
    checkf(scene_depth != nullptr, TEXT("Scene depth texture is null. Cannot proceed with UDLOD passes."));
    UE_LOGFMT(LogUDLODTerrain, Log, "[FCSDispatcher] Scene textures are valid. Color: {cdx}x{cdy}, Depth: {ddx}x{ddy}",
        scene_color->Desc.Extent.X, scene_color->Desc.Extent.Y,
        scene_depth->Desc.Extent.X, scene_depth->Desc.Extent.Y);

    FSceneRenderTargetItem TextureItem;
    TextureItem.TargetableTexture = heightmap->GetResource()->GetTexture2DRHI();
    TextureItem.ShaderResourceTexture = heightmap->GetResource()->GetTexture2DRHI();
    const FPooledRenderTargetDesc RenderTargetDesc = FPooledRenderTargetDesc::Create2DDesc(
        FIntPoint(heightmap->GetSizeX(), heightmap->GetSizeY()),
        heightmap->GetPixelFormat(),
        FClearValueBinding::None,
        TexCreate_None,
        TexCreate_ShaderResource | TexCreate_UAV,
        false
    );

    TRefCountPtr<IPooledRenderTarget> PooledRenderTarget;
    GRenderTargetPool.CreateUntrackedElement(RenderTargetDesc, PooledRenderTarget, TextureItem);
    heightmap_rdg = gb.RegisterExternalTexture(PooledRenderTarget, TEXT("HeightmapTexture"));

    UE_LOGFMT(LogUDLODTerrain, Log,
        "[FCSDispatcher] Heightmap registered as RDG texture successfully. Proceeding to add UDLOD passes.");

    add_udlod_passes(gb, view_info, scene_color, scene_depth);
}

void FCSDispatcher::add_udlod_passes(
    FRDGBuilder& gb, const FViewInfo& view,
    FRDGTexture* scene_color, FRDGTexture* scene_depth
) const {
    UE_LOGFMT(LogUDLODTerrain, Log, "[FCSDispatcher] Adding UDLOD compute passes to the render graph.");
    checkf(heightmap_rdg != nullptr,
        TEXT("Heightmap RDG texture is null. Ensure heightmap is set and properly registered."));
    UE_LOGFMT(LogUDLODTerrain, Log, "[FCSDispatcher] Heightmap RDG texture is valid. Size: {X}x{Y}, Format: {d}",
        heightmap_rdg->Desc.Extent.X, heightmap_rdg->Desc.Extent.Y, heightmap_rdg->Desc.Format);

    constexpr uint32 max_tiles = 1u << 2;
    //Tile buffers
    //TODO:(RDG will pool/reuse. Docs recommend for hard persistence use RegisterExternalBuffer
    const FRDGBufferRef tile_a = gb.CreateBuffer(
        FRDGBufferDesc::CreateStructuredDesc(sizeof(FTile), max_tiles), TEXT("UDLOD.TileA"));
    const FRDGBufferRef tile_b = gb.CreateBuffer(
        FRDGBufferDesc::CreateStructuredDesc(sizeof(FTile), max_tiles), TEXT("UDLOD.TileB"));
    const FRDGBufferRef final_tiles = gb.CreateBuffer(
        FRDGBufferDesc::CreateStructuredDesc(sizeof(FTile), max_tiles), TEXT("UDLOD.FinalTiles"));
    auto MakeCountBuffer = [&](const TCHAR* Name) {
        return gb.CreateBuffer(FRDGBufferDesc::CreateStructuredDesc<FCount>(1), Name);
    };

    const FRDGBufferRef count_cur = MakeCountBuffer(TEXT("UDLOD.CountCur"));
    const FRDGBufferRef count_next = MakeCountBuffer(TEXT("UDLOD.CountNext"));
    const FRDGBufferRef count_final = MakeCountBuffer(TEXT("UDLOD.CountFinal"));

    UE_LOGFMT(LogUDLODTerrain, Log,
        "[FCSDispatcher] Buffers for tiles and counts created successfully. Max tiles: {mt}", max_tiles);

    // Draw args buffer (must be usable for indirect draw)
    FRDGBufferRef DrawArgs = gb.CreateBuffer(FRDGBufferDesc::CreateIndirectDesc(4), TEXT("UDLOD.DrawArgs"));

    constexpr int terrain_scale_1d = 1024 * 1024;
    const FVector2f root_origin(0, 0);
    // Init (seed root, zero counters)
    {
        UE_LOGFMT(LogUDLODTerrain, Log,
            "[FCSDispatcher] Adding initialization compute pass to seed root tile and zero counters.");
        const TShaderMapRef<FInitCS> CS(view.ShaderMap);
        auto* pass_params = gb.AllocParameters<FInitCS::FParameters>();
        pass_params->tile_list = gb.CreateUAV(tile_a);
        pass_params->count_current = gb.CreateUAV(count_cur);
        pass_params->count_next = gb.CreateUAV(count_next);
        pass_params->count_final = gb.CreateUAV(count_final);
        pass_params->root_origin = root_origin;

        // TODO: Parameterize
        constexpr int terrain_world_size_in_world_units = terrain_scale_1d;
        pass_params->root_size = terrain_world_size_in_world_units;
        FComputeShaderUtils::AddPass(
            gb,
            RDG_EVENT_NAME("UDLOD.Init"),
            ERDGPassFlags::Compute | ERDGPassFlags::NeverCull,
            CS,
            pass_params,
            FIntVector(1, 1, 1)
        );
    }

    //  Refinement passes (fixed count, but GPU-sized work via indirect dispatch)
    FRDGBufferRef cur_tiles = tile_a;
    FRDGBufferRef next_tiles = tile_b;
    FRDGBufferRef cur_cnt_buf = count_cur;
    FRDGBufferRef next_cnt_buf = count_next;
    // TODO: Parameterize
    constexpr uint32 max_depth = 2;
    constexpr TAlignedTypedef<float, 4>::Type log_distance_scale = 64.f;
    constexpr uint32 input_count_offset = 0;
    constexpr uint32 mult = 1;
    constexpr uint32 divisor = GThreadsPerGroup;
    constexpr TAlignedTypedef<float, 4>::Type view_radius = 200000.f;
    constexpr int indirect_args_offset = 0;

    UE_LOGFMT(LogUDLODTerrain, Log,
        "[FCSDispatcher] Adding refinement passes. Max depth: {md}, Max tiles: {mt}", max_depth, max_tiles);
    for (uint32 Depth = 0; Depth < max_depth; ++Depth) {
        // Clear NextCount to 0 (no CPU readback)
        AddClearUAVPass(gb, gb.CreateUAV(next_cnt_buf), 0, ERDGPassFlags::Compute);
        // Build indirect dispatch args from CurCount[0]
        FRDGBufferRef DispatchArgs = FComputeShaderUtils::AddIndirectArgsSetupCsPass1D(
            gb,
            view.GetFeatureLevel(),
            cur_cnt_buf,
            TEXT("UDLOD.RefineDispatchArgs"),
            divisor,
            input_count_offset,
            mult
        );

        UE_LOGFMT(LogUDLODTerrain, Log,
            "[FCSDispatcher] Added refinement pass for depth {d}. "
            "Dispatch args will be built on GPU with count from current tile buffer.", Depth);

        // Refine (indirect dispatch)
        TShaderMapRef<FRefineCS> CS(view.ShaderMap);
        auto* pass_params = gb.AllocParameters<FRefineCS::FParameters>();
        pass_params->tile_list_in = gb.CreateSRV(cur_tiles);
        pass_params->tile_list_out_next = gb.CreateUAV(next_tiles);
        pass_params->tile_list_out_final = gb.CreateUAV(final_tiles);
        pass_params->count_current = gb.CreateUAV(cur_cnt_buf);
        pass_params->count_next = gb.CreateUAV(next_cnt_buf);
        pass_params->count_final = gb.CreateUAV(count_final);
        pass_params->view_world_pos = FVector3f(view.ViewLocation);
        pass_params->lod_distance_scale = log_distance_scale;
        pass_params->max_depth = max_depth;
        pass_params->max_tiles = max_tiles;
        pass_params->view_radius = view_radius;

        gb.AddPass(
            RDG_EVENT_NAME("UDLOD.Refine Depth=%u", Depth),
            pass_params, ERDGPassFlags::Compute | ERDGPassFlags::NeverCull,
            [CS, pass_params, DispatchArgs](FRHIComputeCommandList& RHICmdList) {
                FComputeShaderUtils::DispatchIndirect(RHICmdList, CS, *pass_params, DispatchArgs, indirect_args_offset);
            });

        // swap frontier for next depth (CPU swaps references only; no per-tile work)
        Swap(cur_tiles, next_tiles);
        Swap(cur_cnt_buf, next_cnt_buf);
    }
    //  Build DrawIndirect args on GPU
    {
        const TShaderMapRef<FDrawArgsCS> cs(view.ShaderMap);
        auto* pass_params = gb.AllocParameters<FDrawArgsCS::FParameters>();
        pass_params->count_final = gb.CreateUAV(count_final);
        pass_params->draw_args_uav = gb.CreateUAV(DrawArgs);
        pass_params->vertex_count_per_instance = vertices_per_tile(GSubdivisions);
        FComputeShaderUtils::AddPass(
            gb,
            RDG_EVENT_NAME("UDLOD.BuildDrawArgs"),
            ERDGPassFlags::Compute | ERDGPassFlags::NeverCull,
            cs,
            pass_params,
            FIntVector(1, 1, 1)
        );
    }
    // Raster pass: one indirect draw, instanced tiles
    {
        constexpr TAlignedTypedef<float, 4>::Type height_scale = 3000.f;
        TShaderMapRef<FTerrainVS> VS(view.ShaderMap);
        TShaderMapRef<FTerrainPS> PS(view.ShaderMap);
        auto* pass_params = gb.AllocParameters<FTerrainVS::FParameters>();
        // pass_params->View = View.ViewUniformBuffer;
        pass_params->final_tiles = gb.CreateSRV(final_tiles);
        pass_params->subdivisions = GSubdivisions;
        pass_params->height_scale = height_scale;
        pass_params->height_tex = heightmap_rdg;
        pass_params->height_samp = TStaticSamplerState<SF_Trilinear>::GetRHI();
        pass_params->terrain_origin_ws = root_origin;
        pass_params->terrain_scale_ws = FVector2f(terrain_scale_1d, terrain_scale_1d);
        pass_params->RenderTargets[0] = FRenderTargetBinding(scene_color, ERenderTargetLoadAction::ELoad);
        pass_params->RenderTargets.DepthStencil = FDepthStencilBinding(scene_depth, ERenderTargetLoadAction::ELoad,
            ERenderTargetLoadAction::ELoad, FExclusiveDepthStencil::DepthWrite_StencilNop);

        gb.AddPass(
            RDG_EVENT_NAME("UDLOD.Draw"),
            pass_params,
            ERDGPassFlags::Raster | ERDGPassFlags::NeverCull,
            [pass_params, VS, PS, DrawArgs, ViewRect = view.ViewRect](FRHICommandList& RHICmdList) {
                constexpr float min_z = 0.0f;
                constexpr float max_z = 1.0f;

                RHICmdList.SetViewport(ViewRect.Min.X, ViewRect.Min.Y, min_z, ViewRect.Max.X, ViewRect.Max.Y, max_z);
                FGraphicsPipelineStateInitializer PSO;
                RHICmdList.ApplyCachedRenderTargets(PSO);
                PSO.PrimitiveType = PT_TriangleStrip;
                PSO.BoundShaderState.VertexDeclarationRHI = GEmptyVertexDeclaration.VertexDeclarationRHI;
                PSO.BoundShaderState.VertexShaderRHI = VS.GetVertexShader();
                PSO.BoundShaderState.PixelShaderRHI = PS.GetPixelShader();
                PSO.DepthStencilState = TStaticDepthStencilState<true, CF_LessEqual>::GetRHI();
                PSO.BlendState = TStaticBlendState<>::GetRHI();
                PSO.RasterizerState = TStaticRasterizerState<>::GetRHI();
                SetGraphicsPipelineState(RHICmdList, PSO, 0);
                SetShaderParameters(RHICmdList, VS, VS.GetVertexShader(), *pass_params);
                SetShaderParameters(RHICmdList, PS, PS.GetPixelShader(), *pass_params);
                RHICmdList.DrawPrimitiveIndirect(DrawArgs->GetRHI(), 0);
            }
        );
    }
}
