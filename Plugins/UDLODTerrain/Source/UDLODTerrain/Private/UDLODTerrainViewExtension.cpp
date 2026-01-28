#include "UDLODTerrainViewExtension.h"

#include <functional>

#include "SceneView.h"
#include "SceneViewExtension.h"
#include "Logging/StructuredLog.h"
#include "Runtime/Renderer/Internal/PostProcess/PostProcessInputs.h"

static TSharedPtr<FUDLODTerrainViewExtension> GUDLODViewExt = nullptr;

TSharedPtr<FUDLODTerrainViewExtension> FUDLODTerrainViewExtension::Get() {
    if (!GUDLODViewExt.IsValid()) { GUDLODViewExt = FSceneViewExtensions::NewExtension<FUDLODTerrainViewExtension>(); }

    return GUDLODViewExt;
}

FUDLODTerrainViewExtension::FUDLODTerrainViewExtension(const FAutoRegister& auto_reg)
    : FSceneViewExtensionBase{auto_reg} {}

void FUDLODTerrainViewExtension::RegisterProxy_RenderThread(const FUDLODTerrainSceneProxy* proxy) {
    check(IsInAnyRenderingThread());
    proxies_rt.Add(proxy);
}

void FUDLODTerrainViewExtension::UnregisterProxy_RenderThread(const FUDLODTerrainSceneProxy* proxy) {
    check(IsInAnyRenderingThread());
    proxies_rt.Remove(proxy);
}

void FUDLODTerrainViewExtension::PrePostProcessPass_RenderThread(
    FRDGBuilder& graph_builder, const FSceneView& in_view, const FPostProcessingInputs& inputs
) {
#if WITH_EDITOR
    if (!IsInAnyRenderingThread()) {
        UE_LOGFMT(LogTemp, Warning, "Called PrePostProcess_RenderThread from a non-render thread!");
        return;
    }
#endif

    const FSceneTextureUniformParameters* contents = inputs.SceneTextures->GetContents();
    const FRDGTextureRef scene_color = contents->SceneColorTexture;
    const FRDGTextureRef scene_depth = contents->SceneDepthTexture;

    TArray<const FUDLODTerrainSceneProxy*> snapshot;
    {
        [[maybe_unused]] FScopeLock lock(&mutex);
        snapshot = proxies_rt.Array();
    }

    for (const FUDLODTerrainSceneProxy* proxy : snapshot) {
        if (proxy == nullptr) { continue; }

        proxy->render_udlod(graph_builder, in_view, scene_color, scene_depth);
    }
}
