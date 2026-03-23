#pragma once
#include "gpu_terrain.h"
#include "gpu_terrain_atlas_buffer_info.h"
#include "terrain_config.h"
#include "terrain_settings.h"
#include "terrain_tile_atlas.h"
#include "terrain_typedefs.h"
#include "terrain_view_config.h"
#include "GameFramework/Actor.h"

#include "terrain_parent_actor.generated.h"

class ATerrainActor;

USTRUCT()
struct FTerrains {
    GENERATED_BODY()

    UPROPERTY(VisibleAnywhere, Category = "UDLOD")
    FTerrainConfig terrain_config;

    UPROPERTY(VisibleAnywhere, Category = "UDLOD")
    FTerrainViewConfig terrain_view_config;

    UPROPERTY(VisibleAnywhere, Category = "UDLOD")
    TObjectPtr<UMaterialInstance> material_instance;

    UPROPERTY(VisibleAnywhere, Category = "UDLOD")
    FView view;
};

UCLASS()
class UDLODTERRAIN_API ATerrainParentActor : public AActor {
    GENERATED_BODY()

public:
    ATerrainParentActor();
    virtual void OnConstruction(const FTransform& tf) override;
    virtual void BeginPlay() override;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UDLOD")
    USceneComponent* root;

    UFUNCTION(Exec, CallInEditor, BlueprintCallable, Category = "UDLOD")
    void PreprocessTerrain();

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UDLOD")
    FTerrainPreprocessSettings terrain_preprocess_settings;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UDLOD")
    FPrimaryTerrainSettings terrain_settings;

    UPROPERTY(VisibleAnywhere, Category = "UDLOD|Data")
    TArray<FTerrains> terrains;

    UPROPERTY(VisibleAnywhere, Category = "UDLOD|Data")
    TArray<TObjectPtr<UMaterialInstance>> materials;

    UPROPERTY(VisibleAnywhere, Category = "UDLOD|Data")
    TArray<FTerrainConfig> configs;

    UPROPERTY(VisibleAnywhere, Category = "UDLOD|Data")
    TMap<UTerrain*, FTileTree> view_components;

    UPROPERTY(VisibleAnywhere, Category = "UDLOD|Data")
    FTerrainSettings settings;

    UPROPERTY(VisibleAnywhere, Category = "UDLOD|Data")
    TMap<UTerrain*, FTileAtlas> tile_atlases;

    UPROPERTY(VisibleAnywhere, Category = "UDLOD|Data|Gpu")
    TMap<UTerrain*, FGpuTileAtlas> gpu_tile_atlases;

    UPROPERTY(VisibleAnywhere, Category = "UDLOD|Data|Gpu")
    TMap<UTerrain*, FGpuTerrain> gpu_terrains;

    UPROPERTY(VisibleAnywhere, Category = "UDLOD|Data|Gpu")
    TMap<UTerrain*, FGpuTerrainView> gpu_terrain_views;

private:
    void rebuild_terrains();
    void clear_spawned_terrains();

    // UPROPERTY(Transient)
    // TArray<TObjectPtr<ATerrainActor>> spawned_terrain_actors;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UTerrain>> spawned_terrains;
};
