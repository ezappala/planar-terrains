#pragma once
#include "gpu_terrain_atlas_buffer_info.h"
#include "gpu_terrain.h"
#include "terrain_config.h"
#include "terrain_settings.h"
#include "terrain_view_config.h"
#include "terrain_tile_atlas.h"
#include "terrain_typedefs.h"
#include "GameFramework/Actor.h"

#include "terrain_parent_actor.generated.h"

UCLASS()
class UDLODTERRAIN_API ATerrainParentActor : public AActor {
    GENERATED_BODY()

public:
    ATerrainParentActor();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UDLOD")
    USceneComponent* root;

    UFUNCTION(Exec, CallInEditor, BlueprintCallable, Category = "UDLOD")
    void PreprocessTerrain();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UDLOD")
    FTerrainPreprocessSettings terrain_preprocess_settings;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UDLOD")
    FPrimaryTerrainSettings terrain_settings;

    using FTerrains = TTuple<
        FTerrainConfig,
        FTerrainViewConfig,
        FMaterialInstancePtr,
        FView
    >;

    TUniquePtr<TArray<FTerrains>> terrains;
    TUniquePtr<TArray<FMaterialInstancePtr>> materials;
    TUniquePtr<TArray<FTerrainConfig>> configs;
    TUniquePtr<TMap<UTerrain*, FTileTree>> view_components;
    TUniquePtr<FTerrainSettings> settings;
    TUniquePtr<TMap<UTerrain*, FTileAtlas>> tile_atlases;

    TUniquePtr<TMap<UTerrain*, FGpuTileAtlas>> gpu_tile_atlases;
    TUniquePtr<TMap<UTerrain*, FGpuTerrain>> gpu_terrains;
    TUniquePtr<TMap<UTerrain*, FGpuTerrainView>> gpu_terrain_views;
};