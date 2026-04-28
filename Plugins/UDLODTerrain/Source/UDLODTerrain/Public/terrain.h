#pragma once

#include "terrain_config.h"
#include "terrain_render_state.h"
#include "terrain_scene_proxy.h"
#include "terrain_settings.h"
#include "terrain_tile_atlas.h"
#include "Components/MeshComponent.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SceneComponent.h"

#include "terrain.generated.h"

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

#pragma region UActorComponent_interface
    virtual void OnRegister() override;
    virtual void OnUnregister() override;
#pragma endregion

    virtual void SetMaterial(int32 ElementIndex, UMaterialInterface* Material) override;
    virtual void UpdateBounds() override;
    // ReSharper disable once CppHidingFunction
    void MarkRenderStateDirty();

    void set_object_data(
        const FTerrainConfig& in_config,
        const FTerrainSettings& in_settings,
        UMaterialInterface* mat
    );

    FTerrainConfig config;

    UPROPERTY()
    FTerrainSettings settings;

    TOptional<FTileAtlas> atlas = NullOpt;
    TSharedPtr<FTerrainRenderResources> render_resources;

    UPROPERTY()
    UMaterialInterface* material;

    bool bDebugWireframe = false;
    bool bDrawDebug;

    friend class FTerrainSceneProxy;
};
