#include "terrain_scene_proxy.h"

#include "SceneInterface.h"
#include "Engine/World.h"

#include "terrain_scene_view_extension.h"
#include "Runtime/Renderer/Private/ScenePrivate.h"

FTerrainSceneProxy::FTerrainSceneProxy(const UTerrain* component) : FPrimitiveSceneProxy
    {component},
    last_camera_position{FVector::ZeroVector} {
    if (!FTerrainSceneViewExtension::instance.IsValid()) {
        FTerrainSceneViewExtension::instance = FSceneViewExtensions::NewExtension<
            FTerrainSceneViewExtension>();
    }

    FVector camera_position = FVector::ZeroVector;
    if (const UWorld* world = component->GetWorld()) {
        if (const APlayerController* pc = world->GetFirstPlayerController()) {
            if (pc->PlayerCameraManager) {
                camera_position = pc->PlayerCameraManager->GetCameraLocation();
            }
        }
    }

    if (camera_position.IsZero() || camera_position.ContainsNaN()) {
        const FVector component_location = component->GetComponentLocation();
        camera_position = component_location + FVector(0, 0, 2000.0f);
    }

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
