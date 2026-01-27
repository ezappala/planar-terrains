#include "UDLODTerrainSceneProxy.h"

#include "RenderGraphBuilder.h"
#include "RHIGPUReadback.h"
#include "RHIStaticStates.h"
#include "TextureResource.h"
#include "UDLODTerrainRenderer.h"
#include "UDLODTerrainViewExtension.h"
#include "Engine/Texture2D.h"
#include "Logging/StructuredLog.h"

FUDLODTerrainSceneProxy::FUDLODTerrainSceneProxy(const UUDLODTerrainComponent* comp) : FPrimitiveSceneProxy(comp) {
    settings_rt.world_size_xy_ws = FVector2f{0., 0.};
    settings_rt.world_size_xy_ws = {
        static_cast<float>(comp->world_size_xy_ws.X),
        static_cast<float>(comp->world_size_xy_ws.Y)
    };
    settings_rt.height_min_ws = comp->height_min_ws;
    settings_rt.height_max_ws = comp->height_max_ws;
    settings_rt.grid_resolution = static_cast<uint32>(comp->grid_resolution);
    settings_rt.max_lod = static_cast<uint32>(comp->max_lod);
    settings_rt.max_tiles = static_cast<uint32>(comp->max_tiles);
    settings_rt.error_pixels = comp->error_pixels;
    settings_rt.height_error_0ws = comp->height_error_0ws;
    settings_rt.morph_start_ratio = comp->morph_start_ratio;

    if (comp->heightmap != nullptr && comp->heightmap->GetResource() != nullptr) {
        height_texture = comp->heightmap->GetResource()->TextureRHI;
    }

    height_sampler = TStaticSamplerState<SF_Trilinear>::GetRHI();
}

FUDLODTerrainSceneProxy::~FUDLODTerrainSceneProxy() {
    ensureMsgf(resources_rt.grid_resolution == 0,
        TEXT("UDLOD proxy destroyed but resources still appear initialized. "
            "Did DestroyRenderThreadResources override run?"));
}

void FUDLODTerrainSceneProxy::CreateRenderThreadResources(FRHICommandListBase& cmd) {
    UE_LOGFMT(LogTemp, Log, "[FUDLODTerrainSceneProxy] Called CreateRenderThreadResources");
    FPrimitiveSceneProxy::CreateRenderThreadResources(cmd);

    check(IsInRenderingThread());
    FUDLODTerrainViewExtension::Get().RegisterProxy_RenderThread(this);
    if (resources_rt.grid_resolution == 0) {
        resources_rt.init(settings_rt.grid_resolution, settings_rt.max_tiles, cmd.GetAsImmediate());
    }
}

void FUDLODTerrainSceneProxy::DestroyRenderThreadResources() {
    UE_LOGFMT(LogTemp, Log, "[FUDLODTerrainSceneProxy] Called DestroyRenderThreadResources");
    check(IsInRenderingThread())
    FUDLODTerrainViewExtension::Get().UnregisterProxy_RenderThread(this);

    resources_rt.release();
    height_texture.SafeRelease();
    height_sampler->Release();
    //height_sampler.SafeRelease();

    FPrimitiveSceneProxy::DestroyRenderThreadResources();
}

FPrimitiveViewRelevance FUDLODTerrainSceneProxy::GetViewRelevance(const FSceneView* View) const {
    FPrimitiveViewRelevance r;
    r.bDrawRelevance = IsShown(View);
    r.bDynamicRelevance = true;
    r.bRenderInMainPass = true;
    r.bShadowRelevance = false;
    return r;
}

/// @remaks Intentionally left empty as rendering will happen via an RDG pass (gpu-driven indirect).
void FUDLODTerrainSceneProxy::GetDynamicMeshElements(
    const TArray<const FSceneView*>& Views,
    const FSceneViewFamily& ViewFamily,
    uint32 VisibilityMap,
    FMeshElementCollector& Collector
) const {}

#if WITH_EDITOR
//static FRHIGPUBufferReadback GFinalCountReadback{TEXT("UDLOD_FinalCountReadback")};
#endif

void FUDLODTerrainSceneProxy::render_udlod(
    FRDGBuilder& graph_builder,
    const FSceneView& view,
    const FRDGTextureRef scene_color,
    const FRDGTextureRef scene_depth
) const {
    if (!height_texture || !height_sampler) { return; }

    FUDLODTerrainRenderer::ensure_resources(
        *this,
        graph_builder.RHICmdList
    );
    const FRDGBufferRef working_tiles_a = graph_builder.RegisterExternalBuffer(resources_rt.working_tiles_a,
        TEXT("UDLOD_WorkingTilesA"), ERDGBufferFlags::None);
    const FRDGBufferRef working_tiles_b = graph_builder.RegisterExternalBuffer(resources_rt.working_tiles_b,
        TEXT("UDLOD_WorkingTilesB"), ERDGBufferFlags::None);
    const FRDGBufferRef final_tiles = graph_builder.RegisterExternalBuffer(resources_rt.final_tiles,
        TEXT("UDLOD_FinalTiles"), ERDGBufferFlags::None);

    const FRDGBufferRef working_count_a = graph_builder.RegisterExternalBuffer(resources_rt.working_count_a,
        TEXT("UDLOD_WorkingCountA"), ERDGBufferFlags::None);
    const FRDGBufferRef working_count_b = graph_builder.RegisterExternalBuffer(resources_rt.working_count_b,
        TEXT("UDLOD_WorkingCountB"), ERDGBufferFlags::None);
    const FRDGBufferRef final_count = graph_builder.RegisterExternalBuffer(resources_rt.final_count,
        TEXT("UDLOD_FinalCount"), ERDGBufferFlags::None);

    const FRDGBufferRef draw_args = graph_builder.RegisterExternalBuffer(resources_rt.draw_args,
        TEXT("UDLOD_DrawArgs"), ERDGBufferFlags::None);

    const FRDGBufferUAVRef working_tiles_a_uav = graph_builder.CreateUAV(working_tiles_a);
    const FRDGBufferUAVRef working_count_a_uav = graph_builder.CreateUAV(working_count_a, PF_R32_UINT);
    const FRDGBufferUAVRef final_count_uav = graph_builder.CreateUAV(final_count, PF_R32_UINT);

    // Root Pass
    FUDLODTerrainRenderer::add_root_tile_pass(
        graph_builder, working_tiles_a_uav,
        working_count_a_uav, final_count_uav
    );

    // Refine passes (compute shader)
    FUDLODTerrainRenderer::add_udlod_refine_passes(
        graph_builder, view, settings_rt,
        height_texture, height_sampler,
        working_tiles_a, working_count_a,
        working_tiles_b, working_count_b,
        final_tiles, final_count
    );

#if WITH_EDITOR
    // constexpr uint32 fc_readback_num_bytes = 4;
    // graph_builder.AddPass(
    //     RDG_EVENT_NAME("UDLOD.ReadbackFinalCount"),
    //     ERDGPassFlags::Readback,
    //     [&](FRHICommandListImmediate&) {
            // if (final_count) {
            //     GFinalCountReadback.EnqueueCopy(cmd, final_count->GetRHI(), fc_readback_num_bytes);
            // }
    // });
#endif

    // Build draw args
    const FRDGBufferUAVRef draw_args_uav = graph_builder.CreateUAV(draw_args, PF_R32_UINT);
    FUDLODTerrainRenderer::add_udlod_draw_args_pass(
        graph_builder, resources_rt.index_count, final_count, draw_args_uav);

    // Draw pass
    const FRDGBufferSRVRef final_tiles_srv = graph_builder.CreateSRV(FRDGBufferSRVDesc{final_tiles});
    FUDLODTerrainRenderer::add_udlod_draw_pass(
        graph_builder, view, settings_rt,
        height_texture, height_sampler_rhi(),
        resources_rt, final_tiles_srv, draw_args,
        scene_color, scene_depth
    );

#if WITH_EDITOR
    // if (GFinalCountReadback.IsReady()) {
    //     uint32* cnt = static_cast<uint32*>(GFinalCountReadback.Lock(fc_readback_num_bytes));
    //     UE_LOGFMT(LogTemp, Log, "UDLOD final_count = {fc}", *cnt);
    //     GFinalCountReadback.Unlock();
    // }
#endif
}
