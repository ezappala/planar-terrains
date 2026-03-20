#pragma once
#include "terrain_parent_actor.h"
#include "Subsystems/WorldSubsystem.h"

#include "terrain_world_subsystem.generated.h"

UCLASS()
class UTerrainWorldSubsystem : public UWorldSubsystem {
    GENERATED_BODY()

public:
    virtual void Deinitialize() override;
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void PostInitialize() override;

    TNotNull<ATerrainParentActor*> get_terrain_root_checked() const { return terrain_root; }

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UDLOD")
    ATerrainParentActor* terrain_root;
};
