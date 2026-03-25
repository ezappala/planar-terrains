#pragma once
#include "terrain_config.h"
#include "terrain_settings.h"
#include "terrain_tile_atlas.h"
#include "Components/MeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"
#include "Logging/StructuredLog.h"
#include "Math/Transform.h"

#include "terrain.generated.h"

using CellCoord = FIntVector3;
struct FView {
    FTransform tile_world_transform = FTransform::Identity;
    // CellCoord cell_coord;
};

UCLASS(
    ClassGroup = (Rendering),
    meta = (BlueprintSpawnableComponent),
    hidecategories = (Object, LOD, Physics, Collision)
)
class UDLODTERRAIN_API UTerrain : public UMeshComponent {
    GENERATED_BODY()

public:
    UTerrain(const FObjectInitializer& ObjectInitializer);

#pragma region UPrimitiveComponent_interface
    virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
    virtual FBoxSphereBounds CalcBounds(const FTransform& LocalToWorld) const override;
    virtual void GetUsedMaterials(
        TArray<UMaterialInterface*>& OutMaterials,
        bool bGetDebugMaterials = false
    ) const override;
    virtual int32 GetNumMaterials() const override;
    virtual UMaterialInterface* GetMaterial(int32 ElementIndex) const override;
#pragma endregion

#pragma region USceneComponent_interface
    virtual void TickComponent(
        float DeltaTime,
        ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction) override;
#pragma endregion

#pragma region UActorComponent_interface
    virtual void OnRegister() override;
    virtual void OnUnregister() override;
#pragma endregion

#if WITH_EDITOR
#pragma region UObject_interface
    virtual void PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) override;
#pragma endregion
#endif

    virtual void SetMaterial(int32 ElementIndex, UMaterialInterface* Material) override;

    void set_object_data(
        const FTerrainConfig& in_config,
        const FTerrainSettings& in_settings,
        UMaterialInterface* mat
    ) {
        UE_LOGFMT(LogTemp, Log, "Setting terrain data: config={c}, settings={s}", *in_config.ToString(), *in_settings.ToString());
        this->config = in_config;
        this->settings = in_settings;
        atlas = FTileAtlas(in_config, in_settings);
        material = mat;
        SetMaterial(0, material);
        UpdateBounds();
        MarkRenderStateDirty();
    }

    FTerrainConfig config;
    FTerrainSettings settings;
    TOptional<FTileAtlas> atlas = NullOpt;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UDLOD|Terrain")
    UMaterialInterface* material;

    friend class FTerrainSceneProxy;
};
