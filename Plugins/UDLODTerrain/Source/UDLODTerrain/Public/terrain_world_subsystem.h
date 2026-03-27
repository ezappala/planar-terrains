#pragma once

#include "terrain_parent_actor.h"
#include "Subsystems/WorldSubsystem.h"
#include "Templates/SharedPointer.h"

#include "terrain_world_subsystem.generated.h"

class FTerrainSceneViewExtension;

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
    void verify_terrain_root_state(bool spawn_if_missing);
    ATerrainParentActor* spawn_terrain_root(
        TSubclassOf<ATerrainParentActor> terrain_root_class,
        const FTransform& spawn_transform,
        bool in_auto_spawned,
        ULevel* override_level = nullptr
    );
    ATerrainParentActor* respawn_terrain_root(
        ATerrainParentActor* actor_to_replace,
        TSubclassOf<ATerrainParentActor> terrain_root_class,
        bool in_auto_spawned
    );

protected:
    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UDLOD")
    ATerrainParentActor* terrain_root = nullptr;

    bool terrain_root_was_auto_spawned = false;
    TSharedPtr<FTerrainSceneViewExtension> terrain_view_extension;
};
