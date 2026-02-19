#pragma once
#include <Subsystems/EngineSubsystem.h>

#include "scene_view.h"
#include "subsys.generated.h"

UCLASS()
class UTerrainSubsystem : public UEngineSubsystem {
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& collection) override;
    virtual void Deinitialize() override;

private:
    TSharedPtr<FTerrainSceneViewExtension> terrain_view;

    UPROPERTY()
    UTerrainRenderPass* terrain_render_pass;
};