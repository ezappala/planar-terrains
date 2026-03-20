#pragma once

#include "ShaderParameterMacros.h"
#include "terrain_settings.h"
#include "Components/MeshComponent.h"
#include "Logging/StructuredLog.h"
#include "UObject/ObjectMacros.h"

#include "terrain_component.generated.h"

UCLASS(
    ClassGroup = (Rendering),
    meta = (BlueprintSpawnableComponent),
    hidecategories = (Object, LOD, Physics, Collision))
class UDLODTERRAIN_API UTerrainComponent : public UMeshComponent {
    GENERATED_BODY()

public:
    UTerrainComponent(const FObjectInitializer& ObjectInitializer);

    virtual FPrimitiveSceneProxy* CreateSceneProxy() override;
    virtual FBoxSphereBounds CalcBounds(
        const FTransform& LocalToWorld
    ) const override;

    virtual void GetUsedMaterials(
        TArray<UMaterialInterface*>& OutMaterials,
        bool bGetDebugMaterials = false
    ) const override;

    virtual int32 GetNumMaterials() const override;
    // virtual UMaterialInterface* GetMaterial(int32 ElementIndex) const override;

    virtual void TickComponent(
        float DeltaTime,
        ELevelTick TickType,
        FActorComponentTickFunction* ThisTickFunction
    ) override;

    virtual void OnRegister() override;
    virtual void OnUnregister() override;

    void update_terrain_mesh();
    void set_material(int32 element_idx, UMaterialInterface* in_material);

#if WITH_EDITOR
    virtual void PostEditChangeProperty(
        FPropertyChangedEvent& PropertyChangedEvent
    ) override;

    UFUNCTION(CallInEditor, Category="UDLOD|Preprocess")
    void preprocess_terrain(bool& out_success, FString& out_message);
#endif
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UDLOD|Preprocess")
    FTerrainPreprocessSettings terrain_preprocess_settings;

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category="UDLOD|Process")
    FPrimaryTerrainSettings terrain_settings;

private:
    void mark_render_state_dirty();
    // FIntPoint calculate_grid_resolution() const;
    // virtual void SendRenderDynamicData_Concurrent() override;
};
