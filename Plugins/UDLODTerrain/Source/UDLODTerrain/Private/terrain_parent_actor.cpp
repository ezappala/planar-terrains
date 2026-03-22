#include "terrain_parent_actor.h"

#include "ext_logging.h"
#include "terrain_preprocess_runner.h"

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

    terrains->Emplace(
        FTerrains{
            FTerrainConfig::from_file(
                FPaths::Combine(terrain_preprocess_settings.terrain_path, TEXT("config.json"))).
            Get(FTerrainConfig{}),
            FTerrainViewConfig{},
            nullptr,
            FView{}
        });
}
