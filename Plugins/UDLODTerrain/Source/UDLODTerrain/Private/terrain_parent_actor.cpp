#include "terrain_parent_actor.h"

#include "preprocess_exec.h"
#include "ext_logging.h"

ATerrainParentActor::ATerrainParentActor() {
    root = CreateDefaultSubobject<USceneComponent>(TEXT("RootComp"));
    SetRootComponent(root);
    check(IsValid(root));

    terrains = MakeUnique<TArray<FTerrains>>();
    materials = MakeUnique<TArray<FMaterialInstancePtr>>();
    view_components = MakeUnique<TMap<UTerrain*, FTileTree>>();
    settings = MakeUnique<FTerrainSettings>();
    tile_atlases = MakeUnique<TMap<UTerrain*, FTileAtlas>>();
}

void ATerrainParentActor::PreprocessTerrain() {
    using ext::logging::time_elapsed_to_string, ext::logging::log_time;
    const FDateTime start_preprocessing = FDateTime::UtcNow();
    UE_LOGFMT(LogTemp, Log, "{t}: Starting terrain preprocessing.", start_preprocessing.ToString());

    log_time(start_preprocessing, "Initializing preprocessing for terrain.");
    auto init = preprocess::initialize(terrain_preprocess_settings);
    auto& [heightmap_dataset, heightmap_context] = init.Get<0>();
    auto& [albedo_dataset, albedo_context] = init.Get<1>();

    log_time(start_preprocessing, "Preprocessing heightmap");
    preprocess::preprocess(MoveTemp(heightmap_dataset), heightmap_context);

    log_time(start_preprocessing, "Preprocessing albedo");
    preprocess::preprocess(MoveTemp(albedo_dataset), albedo_context);

    log_time(start_preprocessing, "Finished terrain preprocessing.");

    terrains->Emplace(
        FTerrains{
            FTerrainConfig::from_file(
                FPaths::ProjectContentDir() / "terrains/earth/config.json").
            Get(FTerrainConfig{}),
            FTerrainViewConfig{},
            nullptr,
            FView{}
        });
}
