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

FPrimitiveViewRelevance FTerrainSceneProxy::GetViewRelevance(const FSceneView* view) const {
    FPrimitiveViewRelevance relevance;
    relevance.bDrawRelevance = IsShown(view);
    relevance.bShadowRelevance = IsShadowCast(view);
    relevance.bDynamicRelevance = true;
    relevance.bRenderInMainPass = ShouldRenderInMainPass();
    relevance.bUsesLightingChannels = GetLightingChannelMask() != GetDefaultLightingChannelMask();
    relevance.bRenderCustomDepth = ShouldRenderCustomDepth();
    relevance.bTranslucentSelfShadow = bCastVolumetricTranslucentShadow;

    CombinedMaterialRelevance.SetPrimitiveViewRelevance(relevance);

    return relevance;
}

uint32 FTerrainSceneProxy::GetMemoryFootprint() const { return sizeof(*this) + GetAllocatedSize(); }
