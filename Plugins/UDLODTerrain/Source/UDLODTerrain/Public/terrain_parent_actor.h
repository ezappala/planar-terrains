#pragma once
#include "gpu_terrain.h"
#include "gpu_terrain_atlas_buffer_info.h"
#include "terrain_config.h"
// #include "terrain_picking.h"
#include "terrain_settings.h"
#include "terrain_tile_atlas.h"
#include "terrain_tile_tree.h"
#include "terrain_view_config.h"
#include "GameFramework/Actor.h"

#include "terrain_parent_actor.generated.h"

class UTexture2D;

struct FTerrains {
    FTerrainConfig terrain_config;
    FTerrainViewConfig terrain_view_config;
    UMaterialInterface* material_interface;
    FView view;
};

UCLASS(
    Blueprintable,
    ClassGroup = (Rendering),
    hidecategories = (Object, LOD, Physics, Collision))
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

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UDLOD")
    FTerrainPreprocessSettings terrain_preprocess_settings;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "UDLOD")
    FPrimaryTerrainSettings terrain_settings;

    TOptional<FTerrains> terrain;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UDLOD")
    UMaterialInterface* material;

    FTerrainSettings settings;

    TOptional<FTerrainConfig> config = NullOpt;
    TOptional<FTileTree> view_component = NullOpt;
    TOptional<FTileAtlas> tile_atlas = NullOpt;
    TOptional<FGpuTileAtlas> gpu_tile_atlas = NullOpt;
    TOptional<FGpuTerrain> gpu_terrain = NullOpt;
    TOptional<FGpuTerrainView> gpu_terrain_view = NullOpt;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UDLOD|Fallback")
    bool bEnableTessellationFallback = false;

    UPROPERTY(
        EditAnywhere,
        BlueprintReadWrite,
        Category="UDLOD|Fallback",
        meta=(ClampMin="1", ClampMax="128", EditCondition="bEnableTessellationFallback",
            EditConditionHides))
    int32 tessellation_fallback_factor = 32;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UDLOD")
    UTerrain* spawned_terrain;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UDLOD")
    USceneComponent* spawned_fallback_component;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category = "UDLOD")
    UTexture2D* fallback_height_texture;

private:
    void rebuild_terrains();
    void clear_spawned_terrains();
};
