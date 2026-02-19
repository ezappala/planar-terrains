#include "render_pass.h"

#include "scene_view.h"

// IMPLEMENT_GLOBAL_SHADER(FColorExtractPS, "/Plugins/UDLODTerrain/r_fragment.usf", "MainPS", SF_Pixel);
void UTerrainRenderPass::ReInitializeAllViews() {
    auto* viewData = &PerViewData;
    ENQUEUE_RENDER_COMMAND(QueueGoLReInitialization)([viewData](FRHICommandListImmediate& cmds) {
        viewData->ForEachView([&](int viewID, FTerrainView& data, ERHIFeatureLevel::Type featureLevel) {
            data.ReinitializeViews = true;
        });
    });
}

TSharedRef<F_EGP_RenderPassSceneViewExtension> UTerrainRenderPass::InitThisPass_GameThread(UWorld& thisWorld) {
    return FSceneViewExtensions::NewExtension<FTerrainSceneViewExtension>(this);
}

void UTerrainRenderPass::Tick_GameThread(UWorld& thisWorld, float deltaSeconds) {
    Super::Tick_GameThread(thisWorld, deltaSeconds);

    //Update render-thread copies of our parameters.
    auto* matIn = EffectMaterial;
    auto* matOut = &effectMaterial_RenderThread;
    ENQUEUE_RENDER_COMMAND(UpdateGoLParams)([matIn, matOut](FRHICommandList& cmds) { *matOut = matIn; });
}

void UTerrainRenderPass::Tick_RenderThread(const FSceneInterface& thisScene, float gameThreadDeltaSeconds) {
    if (effectMaterial_RenderThread->IsValidLowLevel()) {
        EnsureCompiledSimulateShader(
            thisScene.GetFeatureLevel(),
            effectMaterial_RenderThread
        );
    }

    Super::Tick_RenderThread(thisScene, gameThreadDeltaSeconds);
    PerViewData.Tick();

    //Update the delta-time for each viewport's next tick.
    PerViewData.ForEachView([&](int viewID, FTerrainView& view, ERHIFeatureLevel::Type featureLevel) {
        view.NextTickTime += gameThreadDeltaSeconds;
    });
}
