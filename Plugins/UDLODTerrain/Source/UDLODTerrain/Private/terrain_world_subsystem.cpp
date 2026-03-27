#include "terrain_world_subsystem.h"

#include "EngineUtils.h"
#include "terrain_scene_view_extension.h"
#include "Engine/World.h"

namespace {
TArray<ATerrainParentActor*> find_terrain_roots(const UWorld* world) {
    TArray<ATerrainParentActor*> parent_actors;
    // Note: a TActorIterator cannot be used in a non-game thread.
    if (world == nullptr || !IsInGameThread()) { return parent_actors; }

    for (TActorIterator<ATerrainParentActor> actor_it(world); actor_it; ++actor_it) {
        if (!IsValid(*actor_it) || actor_it->IsActorBeingDestroyed()) { continue; }
        parent_actors.Add(*actor_it);
    }

    return parent_actors;
}

ATerrainParentActor* choose_preferred_root(
    const TArray<ATerrainParentActor*>& parent_actors,
    ATerrainParentActor* current_root
) {
    if (IsValid(current_root) &&
        parent_actors.ContainsByPredicate(
            [current_root](const ATerrainParentActor* actor) {
                return actor == current_root && !actor->is_auto_spawned_by_world_subsystem();
            })) { return current_root; }

    if (ATerrainParentActor* const* manual_root = parent_actors.FindByPredicate(
        [](const ATerrainParentActor* actor) {
            return IsValid(actor) && !actor->is_auto_spawned_by_world_subsystem();
        }); manual_root != nullptr && IsValid(*manual_root)) { return *manual_root; }

    if (IsValid(current_root) &&
        parent_actors.ContainsByPredicate(
            [current_root](const ATerrainParentActor* actor) { return actor == current_root; })) {
        return current_root;
    }

    return parent_actors.IsEmpty() ? nullptr : parent_actors[0];
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
            world,
            resolve_terrain_root(true));
    }

    verify_terrain_root_state(false);
}

void UTerrainWorldSubsystem::PostInitialize() {
    Super::PostInitialize();
    verify_terrain_root_state(false);
}

void UTerrainWorldSubsystem::OnWorldBeginPlay(UWorld& in_world) {
    Super::OnWorldBeginPlay(in_world);
    verify_terrain_root_state(true);
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
    if (!IsValid(in_terrain_root)) { return; }

    in_terrain_root->set_auto_spawned_by_world_subsystem(in_auto_spawned);

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
    const UWorld* world = GetWorld();
    if (world == nullptr) { return nullptr; }

    if (!IsValid(terrain_root)) {
        terrain_root = nullptr;
        terrain_root_was_auto_spawned = false;
    }

    TArray<ATerrainParentActor*> parent_actors = find_terrain_roots(world);
    if (parent_actors.Num() > 0) {
        ATerrainParentActor* preferred_root = choose_preferred_root(parent_actors, terrain_root);
        if (IsValid(preferred_root)) {
            set_terrain_root(
                preferred_root,
                preferred_root->is_auto_spawned_by_world_subsystem()
            );
        }

        int32 manual_root_count = 0;
        for (ATerrainParentActor* candidate_root : parent_actors) {
            if (!IsValid(candidate_root) || candidate_root == terrain_root) { continue; }

            if (candidate_root->is_auto_spawned_by_world_subsystem()) {
                UE_LOGFMT(
                    LogTemp,
                    Log,
                    "Destroying stale auto-spawned terrain root actor: {n}",
                    candidate_root->GetName()
                );
                candidate_root->Destroy();
                continue;
            }

            ++manual_root_count;
        }

        if (manual_root_count > 0) {
            UE_LOGFMT(
                LogTemp,
                Warning,
                "[UDLODTerrainSubsystem] Multiple manually managed terrain parent actors exist; keeping {PreferredRoot} as the active root and leaving the others intact.",
                IsValid(terrain_root) ? terrain_root->GetName() : TEXT("None")
            );
        }

        return terrain_root;
    }

    UE_LOGFMT(LogTemp, Log, "No terrain parent actor found in the world.");
    if (!spawn_if_missing) { return terrain_root; }

    UE_LOGFMT(LogTemp, Log, "No terrain root actor found, spawning one...");
    return spawn_terrain_root(ATerrainParentActor::StaticClass(), FTransform::Identity, true);
}

void UTerrainWorldSubsystem::verify_terrain_root_state(const bool spawn_if_missing) {
    if (ATerrainParentActor* root_actor = resolve_terrain_root(spawn_if_missing)) {
        root_actor->VerifyInitializationState(false);
    }
}

ATerrainParentActor* UTerrainWorldSubsystem::spawn_terrain_root(
    const TSubclassOf<ATerrainParentActor> terrain_root_class,
    const FTransform& spawn_transform,
    const bool in_auto_spawned,
    ULevel* override_level
) {
    UWorld* world = GetWorld();
    if (world == nullptr) { return nullptr; }

    UClass* resolved_class = terrain_root_class.Get();
    if (resolved_class == nullptr) { resolved_class = ATerrainParentActor::StaticClass(); }

    FActorSpawnParameters spawn_info;
    spawn_info.OverrideLevel = override_level;
    spawn_info.SpawnCollisionHandlingOverride = ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    spawn_info.bDeferConstruction = true;

    ATerrainParentActor* spawned_root = Cast<ATerrainParentActor>(
        world->SpawnActor(resolved_class, &spawn_transform, spawn_info)
    );
    if (!IsValid(spawned_root)) { return nullptr; }

    spawned_root->set_auto_spawned_by_world_subsystem(in_auto_spawned);
    spawned_root->FinishSpawning(spawn_transform);
    set_terrain_root(spawned_root, in_auto_spawned);
    return spawned_root;
}

ATerrainParentActor* UTerrainWorldSubsystem::respawn_terrain_root(
    ATerrainParentActor* actor_to_replace,
    const TSubclassOf<ATerrainParentActor> terrain_root_class,
    const bool in_auto_spawned
) {
    if (!IsValid(actor_to_replace)) { return nullptr; }

    ATerrainParentActor* new_root = spawn_terrain_root(
        terrain_root_class,
        actor_to_replace->GetActorTransform(),
        in_auto_spawned,
        actor_to_replace->GetLevel()
    );
    if (!IsValid(new_root)) { return nullptr; }

    if (actor_to_replace != new_root && !actor_to_replace->IsActorBeingDestroyed()) {
        actor_to_replace->Destroy();
    }

    return new_root;
}
