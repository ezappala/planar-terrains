#pragma once
#include "terrain_parent_actor.h"
#include "Subsystems/WorldSubsystem.h"

#include "terrain_world_subsystem.generated.h"

UCLASS()
class UTerrainWorldSubsystem : public UWorldSubsystem {
    GENERATED_BODY()

public:
    virtual void Deinitialize() override;
    virtual void Initialize(FSubsystemCollectionBase& collection) override;
    virtual void PostInitialize() override;
    virtual void OnWorldBeginPlay(UWorld& in_world) override;

    TNotNull<ATerrainParentActor*> get_terrain_root_checked();
    void set_terrain_root(ATerrainParentActor* in_terrain_root, bool in_auto_spawned = false);
    ATerrainParentActor* resolve_terrain_root(bool spawn_if_missing);

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UDLOD")
    ATerrainParentActor* terrain_root = nullptr;

    bool terrain_root_was_auto_spawned = false;
};
