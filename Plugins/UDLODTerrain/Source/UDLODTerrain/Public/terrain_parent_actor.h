#pragma once
#include "gpu_terrain.h"
#include "gpu_terrain_atlas_buffer_info.h"
#include "terrain_config.h"
// #include "terrain_picking.h"
#include "terrain_settings.h"
#include "terrain_tile_atlas.h"
#include "terrain_typedefs.h"
#include "terrain_view_config.h"
#include "GameFramework/Actor.h"

#include "terrain_parent_actor.generated.h"

class UTexture2D;

struct FTerrains {
    FTerrainConfig terrain_config;
    FTerrainViewConfig terrain_view_config;
    TObjectPtr<UMaterialInterface> material_instance;
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

    // TSharedPtr<FPickingData> picking_data = nullptr;

    UFUNCTION(Exec, CallInEditor, BlueprintCallable, Category = "UDLOD")
    void PreprocessTerrain();

    FTerrainPreprocessSettings terrain_preprocess_settings;
    FPrimaryTerrainSettings terrain_settings;
    TArray<FTerrains> terrains;
    TArray<TObjectPtr<UMaterialInterface>> materials;
    TArray<FTerrainConfig> configs;
    TMap<TObjectPtr<UTerrain>, FTileTree> view_components;
    FTerrainSettings settings;
    TMap<TObjectPtr<UTerrain>, FTileAtlas> tile_atlases;
    TMap<TObjectPtr<UTerrain>, FGpuTileAtlas> gpu_tile_atlases;
    TMap<TObjectPtr<UTerrain>, FGpuTerrain> gpu_terrains;
    TMap<TObjectPtr<UTerrain>, FGpuTerrainView> gpu_terrain_views;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UDLOD|Fallback")
    bool bEnableTessellationFallback = false;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category="UDLOD|Fallback",
        meta=(ClampMin="1", ClampMax="128", EditCondition="bEnableTessellationFallback",
            EditConditionHides))
    int32 tessellation_fallback_factor = 32;

private:
    void rebuild_terrains();
    void clear_spawned_terrains();

    UPROPERTY(Transient)
    TArray<TObjectPtr<UTerrain>> spawned_terrains;

    UPROPERTY(Transient)
    TArray<TObjectPtr<USceneComponent>> spawned_fallback_components;

    UPROPERTY(Transient)
    TArray<TObjectPtr<UTexture2D>> fallback_height_textures;
};
