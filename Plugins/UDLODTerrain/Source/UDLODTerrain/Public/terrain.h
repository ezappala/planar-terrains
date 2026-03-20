#pragma once
#include "terrain_config.h"
#include "terrain_settings.h"
#include "terrain_tile_atlas.h"
#include "terrain_typedefs.h"
#include "Components/SceneComponent.h"
#include "Math/Transform.h"

#include "terrain.generated.h"

using CellCoord = FIntVector3;
USTRUCT(Blueprintable)
struct FView {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FTransform tile_world_transform = FTransform::Identity;
    // CellCoord cell_coord;
};

UCLASS(Blueprintable)
class UTerrain : public USceneComponent {
    GENERATED_BODY()

public:
    UTerrain() {}

    void set_object_data(
        const FTerrainConfig& config,
        const FTerrainSettings& settings,
        UMaterialInstance* mat
    ) {
        atlas = MakeUnique<FTileAtlas>(config, settings);
        material = mat;
    }

    TUniquePtr<FTileAtlas> atlas;

    UPROPERTY()
    TObjectPtr<UMaterialInstance> material;
};

USTRUCT(Blueprintable)
struct FTerrainViewKey {
    GENERATED_BODY()
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    UTerrain* terrain;
    UPROPERTY(EditAnywhere, BlueprintReadWrite)
    FView view;
};
