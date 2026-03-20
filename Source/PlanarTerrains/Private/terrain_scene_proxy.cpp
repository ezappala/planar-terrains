#include "terrain_scene_proxy.h"

#include "SceneInterface.h"
#include "Engine/World.h"

#include "terrain_scene_view_extension.h"
#include "Runtime/Renderer/Private/ScenePrivate.h"

FTerrainSceneProxy::FTerrainSceneProxy(UTerrainComponent* component) : FPrimitiveSceneProxy
    {component},
    settings{MakeShared<FPrimaryTerrainSettings>(component->terrain_settings)},
    preproc_settings{
        MakeShared<FTerrainPreprocessSettings>(component->terrain_preprocess_settings)
    },
    last_camera_position{FVector::ZeroVector} {
    FTerrainSceneViewExtension::Init(
        preproc_settings->heightmap_src_path,
        preproc_settings->albedo_src_path,
        settings
    );

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

    // ENQUEUE_RENDER_COMMAND(RenderTerrain)([
    //         this,
    //         heightmap = component->terrain_preprocess_settings.heightmap_src
    //     ](FRHICommandListImmediate& cmd) {});

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
    const TArray<const FSceneView*>& views,
    const FSceneViewFamily& view_family,
    uint32 visibility_map,
    FMeshElementCollector& collector
) const {
    FPrimitiveSceneProxy::GetDynamicMeshElements(
        views,
        view_family,
        visibility_map,
        collector);
}

FPrimitiveViewRelevance FTerrainSceneProxy::GetViewRelevance(
    const FSceneView* view
) const { return FPrimitiveSceneProxy::GetViewRelevance(view); }

uint32 FTerrainSceneProxy::GetMemoryFootprint() const { return sizeof(*this) + GetAllocatedSize(); }
