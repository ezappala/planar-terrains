#pragma once

#include "SceneViewExtension.h"
#include "UDLODTerrainSceneProxy.h"

struct FUDLODTerrainViewExtension final : FSceneViewExtensionBase {
    static FUDLODTerrainViewExtension& Get();
    FUDLODTerrainViewExtension(const FAutoRegister& auto_reg);

    void RegisterProxy_RenderThread(const FUDLODTerrainSceneProxy* proxy);
    void UnregisterProxy_RenderThread(const FUDLODTerrainSceneProxy* proxy);
    // void RegisterProxy(const FUDLODTerrainSceneProxy* proxy);
    // void UnregisterProxy(const FUDLODTerrainSceneProxy* proxy);

    virtual void PrePostProcessPass_RenderThread(
        FRDGBuilder& graph_builder,
        const FSceneView& in_view,
        const FPostProcessingInputs& inputs
    ) override;

private:
    TSet<const FUDLODTerrainSceneProxy*> proxies_rt;
    FCriticalSection mutex;
};