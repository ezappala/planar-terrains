#include "terrain.h"
#include "Components/MeshComponent.h"

#include "terrain_scene_proxy.h"

UTerrain::UTerrain(const FObjectInitializer& ObjectInitializer) : Super(ObjectInitializer) {
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = true;

#if WITH_EDITOR
    bTickInEditor = true;
#endif

    Bounds = FBoxSphereBounds(FBox(FVector(-500, -500, -100), FVector(500, 500, 100)));

    bCastDynamicShadow = true;
    bCastStaticShadow = false;
    bAffectDynamicIndirectLighting = true;
    bAffectDistanceFieldLighting = true;
}

FPrimitiveSceneProxy* UTerrain::CreateSceneProxy() {
    return new FTerrainSceneProxy(this);
}

FBoxSphereBounds UTerrain::CalcBounds(const FTransform& LocalToWorld) const {
    const double half_side = config.side_length > 0.0 ? config.side_length * 0.5 : 500.0;
    const double max_abs_height = FMath::Max(
        FMath::Abs(static_cast<double>(config.min_height)),
        FMath::Abs(static_cast<double>(config.max_height))
    );
    const double half_height = FMath::Max(max_abs_height, 100.0);

    const FBox local_box(
        FVector(-half_side, -half_side, -half_height),
        FVector(half_side, half_side, half_height)
    );
    return FBoxSphereBounds(local_box).TransformBy(LocalToWorld);
}

void UTerrain::GetUsedMaterials(
    TArray<UMaterialInterface*>& OutMaterials,
    const bool bGetDebugMaterials
) const {
    if (material != nullptr) {
        OutMaterials.AddUnique(material);
    }
}

int32 UTerrain::GetNumMaterials() const {
    return material != nullptr ? 1 : 0;
}

UMaterialInterface* UTerrain::GetMaterial(const int32 ElementIndex) const {
    return ElementIndex == 0 ? material : nullptr;
}

void UTerrain::TickComponent(
    float DeltaTime,
    ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction) {
    Super::TickComponent(DeltaTime, TickType, ThisTickFunction);
}

void UTerrain::OnRegister() {
    Super::OnRegister();
    UpdateBounds();
}

void UTerrain::OnUnregister() { Super::OnUnregister(); }

#if WITH_EDITOR
void UTerrain::PostEditChangeProperty(FPropertyChangedEvent& PropertyChangedEvent) {
    Super::PostEditChangeProperty(PropertyChangedEvent);

    if (PropertyChangedEvent.Property != nullptr) {
        const FName PropertyName = PropertyChangedEvent.Property->GetFName();
        if (PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UTerrain, config) ||
            PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UTerrain, settings) ||
            PropertyName == GET_MEMBER_NAME_STRING_CHECKED(UTerrain, material)) {
            UpdateBounds();
            MarkRenderStateDirty();
        }
    }
}

void UTerrain::SetMaterial(const int32 ElementIndex, UMaterialInterface* Material) {
    if (ElementIndex == 0) {
        material = Material;
        MarkRenderStateDirty();
    }
}

void UTerrain::SendRenderDynamicData_Concurrent() { Super::SendRenderDynamicData_Concurrent(); }
#endif

