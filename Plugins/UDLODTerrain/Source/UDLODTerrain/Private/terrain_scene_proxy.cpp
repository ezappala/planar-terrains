#include "terrain_scene_proxy.h"

#include "SceneInterface.h"
#include "Runtime/Renderer/Private/ScenePrivate.h"

FTerrainSceneProxy::FTerrainSceneProxy(const UTerrain* component) : FPrimitiveSceneProxy
    {component} {
    bWillEverBeLit = true;
    bCastDynamicShadow = true;
    bCastStaticShadow = false;
    bAffectDynamicIndirectLighting = true;
    bAffectDistanceFieldLighting = true;
}

FTerrainSceneProxy::~FTerrainSceneProxy() {}

SIZE_T FTerrainSceneProxy::GetTypeHash() const {
    static size_t unique_pointer;
    return reinterpret_cast<size_t>(&unique_pointer);
}

void FTerrainSceneProxy::GetDynamicMeshElements(
    const TArray<const FSceneView*>& Views,
    const FSceneViewFamily& ViewFamily,
    uint32 VisibilityMap,
    FMeshElementCollector& Collector) const {
    FPrimitiveSceneProxy::GetDynamicMeshElements(Views, ViewFamily, VisibilityMap, Collector);
}

FPrimitiveViewRelevance FTerrainSceneProxy::GetViewRelevance(
    const FSceneView* view
) const {
    FPrimitiveViewRelevance relevance;
    relevance.bDrawRelevance = IsShown(view);
    relevance.bShadowRelevance = IsShadowCast(view);
    relevance.bDynamicRelevance = true;
    relevance.bRenderInMainPass = ShouldRenderInMainPass();
    relevance.bUsesLightingChannels = GetLightingChannelMask() != GetDefaultLightingChannelMask();
    relevance.bOpaque = true;
    return relevance;
}

uint32 FTerrainSceneProxy::GetMemoryFootprint() const { return sizeof(*this) + GetAllocatedSize(); }

uint32 FTerrainSceneProxy::GetAllocatedSize() const {
    return FPrimitiveSceneProxy::GetAllocatedSize();
}
