#include "terrain_world_subsystem.h"

#include "EngineUtils.h"
#include "terrain_scene_view_extension.h"
#include "Engine/World.h"

namespace {
TArray<ATerrainParentActor*> find_terrain_roots(const UWorld* world) {
    TArray<ATerrainParentActor*> parent_actors;
    // Note: a TActorIterator cannot be used in a non-game thread.
    if (world == nullptr || !IsInGameThread()) {
        return parent_actors;
    }

    for (TActorIterator<ATerrainParentActor> actor_it(world); actor_it; ++actor_it) {
        parent_actors.Add(*actor_it);
    }

    return parent_actors;
}
}

void UTerrainWorldSubsystem::Deinitialize() {
    UE_LOGFMT(LogTemp, Log, "Shutting down UDLOD Terrain Subsystem...");
    terrain_view_extension.Reset();
    terrain_root = nullptr;
    terrain_root_was_auto_spawned = false;
    Super::Deinitialize();
}

void UTerrainWorldSubsystem::Initialize(FSubsystemCollectionBase& collection) {
    Super::Initialize(collection);

    UE_LOGFMT(LogTemp, Log, "Initializing UDLOD Terrain Subsystem...");
    if (UWorld* world = GetWorld(); world != nullptr) {
        terrain_view_extension = FSceneViewExtensions::NewExtension<FTerrainSceneViewExtension>(
            world);
    }
}

void UTerrainWorldSubsystem::PostInitialize() {
    Super::PostInitialize();
    resolve_terrain_root(false);
}

void UTerrainWorldSubsystem::OnWorldBeginPlay(UWorld& in_world) {
    Super::OnWorldBeginPlay(in_world);
    resolve_terrain_root(true);
}

TNotNull<ATerrainParentActor*> UTerrainWorldSubsystem::get_terrain_root_checked() {
    ATerrainParentActor* resolved_root = resolve_terrain_root(true);
    checkf(
        IsValid(resolved_root),
        TEXT("[UDLODTerrainSubsystem] Failed to resolve or spawn a terrain root actor")
    );
    return resolved_root;
}

void UTerrainWorldSubsystem::set_terrain_root(
    ATerrainParentActor* in_terrain_root,
    const bool in_auto_spawned
) {
    if (!IsValid(in_terrain_root)) {
        return;
    }

    if (terrain_root == in_terrain_root) {
        terrain_root_was_auto_spawned = in_auto_spawned;
        return;
    }

    if (IsValid(terrain_root) && terrain_root_was_auto_spawned) {
        UE_LOGFMT(
            LogTemp,
            Log,
            "Replacing auto-spawned terrain root actor: {n}",
            terrain_root->GetName()
        );
        terrain_root->Destroy();
    }

    terrain_root = in_terrain_root;
    terrain_root_was_auto_spawned = in_auto_spawned;
}

ATerrainParentActor* UTerrainWorldSubsystem::resolve_terrain_root(const bool spawn_if_missing) {
    UWorld* world = GetWorld();
    if (world == nullptr) {
        return nullptr;
    }

    if (!IsValid(terrain_root)) {
        terrain_root = nullptr;
        terrain_root_was_auto_spawned = false;
    }

    TArray<ATerrainParentActor*> parent_actors = find_terrain_roots(world);
    if (parent_actors.Num() > 0) {
        UE_LOGFMT(LogTemp, Log, "Found {n} terrain parent actors in the world.", parent_actors.Num());

        set_terrain_root(parent_actors[0], false);
        for (int32 i = 1; i < parent_actors.Num(); ++i) {
            if (parent_actors[i] == terrain_root) {
                continue;
            }

            UE_LOGFMT(
                LogTemp,
                Warning,
                "Destroying extra terrain root actor: {n}",
                parent_actors[i]->GetName()
            );
            parent_actors[i]->Destroy();
        }

        return terrain_root;
    }

    UE_LOGFMT(LogTemp, Log, "No terrain parent actor found in the world.");
    if (!spawn_if_missing) {
        return terrain_root;
    }

    UE_LOGFMT(LogTemp, Log, "No terrain root actor found, spawning one...");
    ATerrainParentActor* spawned_root = world->SpawnActor<ATerrainParentActor>(
        ATerrainParentActor::StaticClass(),
        FTransform::Identity
    );
    set_terrain_root(spawned_root, true);
    return terrain_root;
}
