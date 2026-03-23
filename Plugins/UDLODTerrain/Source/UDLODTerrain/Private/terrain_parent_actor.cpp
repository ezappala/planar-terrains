#include "terrain_parent_actor.h"

#include "ext_logging.h"
#include "terrain_preprocess_runner.h"
#include "terrain_world_subsystem.h"
#include "Engine/World.h"

namespace {
TOptional<FTerrains> load_default_terrain_descriptor(
    const FTerrainPreprocessSettings& terrain_preprocess_settings
) {
    const FString config_path = FPaths::Combine(
        terrain_preprocess_settings.terrain_path,
        TEXT("config.json")
    );
    const TOptional<FTerrainConfig> config = FTerrainConfig::from_file(config_path);
    if (!config.IsSet()) {
        return NullOpt;
    }

    return FTerrains{
        config.GetValue(),
        FTerrainViewConfig{},
        nullptr,
        FView{}
    };
}
}

ATerrainParentActor::ATerrainParentActor() {
    root = CreateDefaultSubobject<USceneComponent>(TEXT("RootComp"));
    SetRootComponent(root);
    check(IsValid(root));

    terrains = TArray<FTerrains>();
    materials = TArray<TObjectPtr<UMaterialInstance>>();
    configs = TArray<FTerrainConfig>();
    view_components = TMap<UTerrain*, FTileTree>();
    settings = FTerrainSettings();
    tile_atlases = TMap<UTerrain*, FTileAtlas>();
}

void ATerrainParentActor::OnConstruction(const FTransform& tf) {
    Super::OnConstruction(tf);
    UE_LOGFMT(LogTemp, Log, "Constructing terrain parent actor: {n}", GetName());
    rebuild_terrains();
}

void ATerrainParentActor::BeginPlay() {
    Super::BeginPlay();
    rebuild_terrains();
}

void ATerrainParentActor::PreprocessTerrain() {
    using ext::logging::log_time;
    const FDateTime start_preprocessing = FDateTime::UtcNow();
    UE_LOGFMT(LogTemp, Log, "{t}: Starting terrain preprocessing.", start_preprocessing.ToString());

    const auto result = run_preprocess(
        terrain_preprocess_settings,
        preprocess::FPreprocessRunOptions{
            preprocess::EPreprocessProgressMode::EditorDialog,
            0.25f
        });
    if (!result.has_value()) {
        UE_LOGFMT(LogTemp, Error, "Terrain preprocessing failed: {error}", result.error().ToString());
        return;
    }

    log_time(start_preprocessing, "Finished terrain preprocessing.");

    terrains.Emplace(
        FTerrains{
            FTerrainConfig::from_file(
                FPaths::Combine(terrain_preprocess_settings.terrain_path, TEXT("config.json"))).
            Get(FTerrainConfig{}),
            FTerrainViewConfig{},
            nullptr,
            FView{}
        });

    rebuild_terrains();
}

void ATerrainParentActor::rebuild_terrains() {
    UE_LOGFMT(LogTemp, Log, "Rebuilding terrains for terrain parent actor: {n}", GetName());
    if (IsTemplate()) {
        UE_LOGFMT(LogTemp, Warning, "Skipping terrain rebuild for terrain parent actor template: {n}", GetName());
        return;
    }

    const UWorld* world = GetWorld();
    if (world == nullptr) {
        UE_LOGFMT(LogTemp, Warning, "Failed to get world for terrain parent actor: {n}", GetName());
        return;
    }

    if (UTerrainWorldSubsystem* Subsystem = world->GetSubsystem<UTerrainWorldSubsystem>()) {
        Subsystem->set_terrain_root(this);
    }

    clear_spawned_terrains();

    TArray<FTerrains> terrains_to_spawn = terrains;
    if (terrains_to_spawn.IsEmpty()) {
        if (const TOptional<FTerrains> default_terrain = load_default_terrain_descriptor(
            terrain_preprocess_settings
        )) {
            terrains_to_spawn.Add(default_terrain.GetValue());
        }
    }

    UE_LOGFMT(LogTemp, Log, "Spawning terrain actors for terrain parent actor: {n}", GetName());
    TArray<ext::iter::Enumeration<FTerrains>> iter = ext::iter::enumerate(terrains_to_spawn);
    for (const auto& [i, out_terrains] : iter) {
        const auto& [terrain_config, terrain_view_config, material_instance, view] = out_terrains;
        const FTerrainConfig& config = terrain_config;
        const FTerrainViewConfig& view_config = terrain_view_config;
        UMaterialInstance* material = material_instance;
        FString terrain_comp_name = FString::Printf(TEXT("TerrainComp_%llu"), i);
        auto* terrain = NewObject<UTerrain>(this, *terrain_comp_name, RF_Transient);
        terrain->RegisterComponent();
        terrain->AttachToComponent(root, FAttachmentTransformRules::KeepRelativeTransform);

        if (!IsValid(terrain)) {
            UE_LOGFMT(
                LogTemp,
                Warning,
                "Failed to create terrain component for terrain parent actor: {n}, skipping terrain index: {i}",
                GetName(),
                i);
            continue;
        }

        UE_LOGFMT(
            LogTemp,
            Log,
            "Created terrain component: {n} for terrain parent actor: {parent_n}, terrain index: {i}",
            terrain->GetName(),
            GetName(),
            i);
        terrain->set_object_data(config, settings, material);
        terrain->UpdateBounds();
        terrain->MarkRenderStateDirty();

        spawned_terrains.Add(terrain);
        materials.Add(material);
        configs.Add(config);
        view_components.Emplace(terrain, FTileTree{config, view_config});

        tile_atlases.Emplace(terrain, terrain->atlas);
        UE_LOGFMT(
            LogTemp,
            Log,
            "Added tile atlas for terrain component: {n} in terrain parent actor: {parent_n}, terrain index: {i}",
            terrain->GetName(),
            GetName(),
            i);
    }
}

void ATerrainParentActor::clear_spawned_terrains() {
    UE_LOGFMT(LogTemp, Log, "Clearing spawned terrains for terrain parent actor: {n}", GetName());
    for (TObjectPtr<UTerrain>& terrain : spawned_terrains) {
        if (IsValid(terrain)) {
            UE_LOGFMT(LogTemp, Log, "Destroying terrain actor: {n}", terrain->GetName());
            terrain->DestroyComponent();
        } else {
            UE_LOGFMT(
                LogTemp,
                Warning,
                "Invalid terrain actor found while clearing spawned terrains for terrain parent actor: {n}",
                GetName());
        }
    }

    // spawned_terrain_actors.Reset();
    spawned_terrains.Reset();
    materials.Reset();
    configs.Reset();
    view_components.Reset();
    tile_atlases.Reset();
    gpu_tile_atlases.Reset();
    gpu_terrains.Reset();
    gpu_terrain_views.Reset();
}
