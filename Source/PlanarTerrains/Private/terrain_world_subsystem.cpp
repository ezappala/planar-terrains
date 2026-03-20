#include "terrain_world_subsystem.h"

#include "EngineUtils.h"

void UTerrainWorldSubsystem::Deinitialize() {
    UE_LOGFMT(LogTemp, Log, "Shutting down UDLOD Terrain Subsystem...");
    Super::Deinitialize();
}

void UTerrainWorldSubsystem::Initialize(FSubsystemCollectionBase& Collection) {
    Super::Initialize(Collection);

    UE_LOGFMT(LogTemp, Log, "Initializing UDLOD Terrain Subsystem...");
}

void UTerrainWorldSubsystem::PostInitialize() {
    Super::PostInitialize();

    auto* world = GetWorld();
    if (world == nullptr) { return; }

    // Find a parent actor, if one doesn't exist, spawn one and attach all terrain actors to it
    TActorIterator<ATerrainParentActor> actor_it(world);
    TArray<ATerrainParentActor*> parent_actors;
    for (; actor_it; ++actor_it) { parent_actors.Add(*actor_it); }

    if (parent_actors.Num() == 0) {
        UE_LOGFMT(LogTemp, Log, "No terrain parent actor found in the world.");
    } else {
        UE_LOGFMT(LogTemp, Log, "Found {n} terrain parent actors in the world.", parent_actors.Num());
    }

    if (parent_actors.Num() > 0) {
        terrain_root = parent_actors[0];
        if (parent_actors.Num() > 1) {
            UE_LOGFMT(LogTemp, Warning, "Multiple terrain root actors found, using the first one found: {n}", terrain_root->GetName());

            for (int32 i = 1; i < parent_actors.Num(); i++) {
                UE_LOGFMT(LogTemp, Warning, "Destroying extra terrain root actor: {n}", parent_actors[i]->GetName());
                parent_actors[i]->Destroy();
            }
        }
    } else {
        UE_LOGFMT(LogTemp, Log, "No terrain root actor found, spawning one...");
        terrain_root =
            world->SpawnActor<ATerrainParentActor>(
                ATerrainParentActor::StaticClass(),
                FTransform::Identity);
    }

    if (terrain_root == nullptr) {
        UE_LOGFMT(LogTemp, Fatal, "[UDLODTerrainSubsystem] Failed to spawn terrain root actor in");
    }
}
