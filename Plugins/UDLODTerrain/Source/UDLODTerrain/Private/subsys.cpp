#include "subsys.h"

#include <SceneViewExtension.h>

void UTerrainSubsystem::Initialize(FSubsystemCollectionBase& collection) {
    Super::Initialize(collection);

    terrain_render_pass = NewObject<UTerrainRenderPass>(this, "TerrainRenderPass");
    terrain_view = FSceneViewExtensions::NewExtension<FTerrainSceneViewExtension>(terrain_render_pass);
}

void UTerrainSubsystem::Deinitialize() {
    Super::Deinitialize();

    terrain_view.Reset();
    terrain_view = nullptr;
}
