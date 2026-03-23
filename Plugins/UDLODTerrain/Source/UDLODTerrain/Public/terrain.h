#pragma once
#include "terrain_config.h"
#include "terrain_settings.h"
#include "terrain_tile_atlas.h"
#include "terrain_typedefs.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Logging/StructuredLog.h"
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
class UTerrain : public UPrimitiveComponent {
    GENERATED_BODY()

public:
    UTerrain();

    virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
    virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
    virtual void GetUsedMaterials(
        TArray<UMaterialInterface*>& OutMaterials,
        bool bGetDebugMaterials = false
    ) const override;
    virtual int32 GetNumMaterials() const override;

    void set_object_data(
        const FTerrainConfig& in_config,
        const FTerrainSettings& in_settings,
        UMaterialInstance* mat
    ) {
        UE_LOGFMT(LogTemp, Log, "Setting terrain data: config={c}, settings={s}", *in_config.ToString(), *in_settings.ToString());
        this->config = in_config;
        this->settings = in_settings;
        atlas = FTileAtlas(in_config, in_settings);
        material = mat;
        UpdateBounds();
        MarkRenderStateDirty();
    }

    UPROPERTY(VisibleAnywhere)
    FTerrainConfig config;

    UPROPERTY(VisibleAnywhere)
    FTerrainSettings settings;

    UPROPERTY(VisibleAnywhere)
    FTileAtlas atlas;

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
