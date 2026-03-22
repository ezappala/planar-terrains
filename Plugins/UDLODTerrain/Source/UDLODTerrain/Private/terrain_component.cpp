#include "terrain_component.h"

#include "terrain_preprocess_runner.h"

#if WITH_EDITOR
#include "Editor.h"
#include "EditorViewportClient.h"
#endif

UTerrainComponent::UTerrainComponent(
    const FObjectInitializer& ObjectInitializer
) : Super(ObjectInitializer) {
    PrimaryComponentTick.bCanEverTick = true;
    PrimaryComponentTick.bStartWithTickEnabled = true;

#if WITH_EDITOR
    bTickInEditor = true;
#endif
    Bounds = FBoxSphereBounds(
        FBox(
            FVector(-500, -500, -100),
            FVector(500, 500, 100)));

    bCastDynamicShadow = true;
    bCastStaticShadow = false;
    bAffectDynamicIndirectLighting = true;
    bAffectDistanceFieldLighting = true;
}

FPrimitiveSceneProxy* UTerrainComponent::CreateSceneProxy() {
    // TODO: Add terain scene proxy
    return nullptr;
}

FBoxSphereBounds UTerrainComponent::CalcBounds(
    const FTransform& LocalToWorld
) const {
    const float half_size_x = terrain_settings.heightmap_texture_size * 0.5f;
    const float half_size_y = terrain_settings.heightmap_texture_size * 0.5f;
    const float max_displacement = terrain_settings.max_height;

    // Check for zero or near-zero scale which would make bounds invalid
    const FVector Scale3D = LocalToWorld.GetScale3D();
    constexpr float MinScale = 0.001f;
    if (FMath::IsNearlyZero(Scale3D.X, MinScale) ||
        FMath::IsNearlyZero(Scale3D.Y, MinScale) ||
        FMath::IsNearlyZero(Scale3D.Z, MinScale)) {
        // // This is an error condition - always log as Warning
        // if (bEnableDebugLogging) {
        //     UE_LOG(LogTemp, Warning,
        //         TEXT("GPUTessellation: CalcBounds - ZERO OR NEAR-ZERO SCALE DETECTED: %s - Using identity scale"),
        //         *Scale3D.ToString());
        // }
        // Use a transform with identity scale
        FTransform FixedTransform = LocalToWorld;
        FixedTransform.SetScale3D(FVector::OneVector);

        const FBox LocalBox(
            FVector(-half_size_x, -max_displacement, -half_size_y),
            FVector(half_size_x, max_displacement, half_size_y)
        );

        return FBoxSphereBounds(LocalBox).TransformBy(FixedTransform);
    }

    const FBox LocalBox(
        FVector(-half_size_x, -max_displacement, -half_size_y),
        FVector(half_size_x, max_displacement, half_size_y)
    );

    const FBoxSphereBounds Result = FBoxSphereBounds(LocalBox).TransformBy(
        LocalToWorld);

    // Throttled logging (max once every 2 seconds)
    // if (bEnableDebugLogging) {
    //     const double CurrentTime = FPlatformTime::Seconds();
    //     if (CurrentTime - LastLogTime >= 2.0) {
    //         LastLogTime = CurrentTime;
    //         UE_LOG(LogTemp, Log,
    //             TEXT("GPUTessellation: CalcBounds - PlaneSizeX:%.1f PlaneSizeZ:%.1f MaxDisp:%.1f Scale:%s Result:%s"),
    //             TessellationSettings.PlaneSizeX, TessellationSettings.PlaneSizeY, MaxDisplacement,
    //             *Scale3D.ToString(), *Result.ToString());
    //     }
    // }

    return Result;
}

void UTerrainComponent::GetUsedMaterials(
    TArray<UMaterialInterface*>& OutMaterials,
    bool bGetDebugMaterials
) const {
    // if (material) { OutMaterials.AddUnique(material); }
}

int32 UTerrainComponent::GetNumMaterials() const {
    // return material ? 1 : 0;
    return 0;
}

// UMaterialInterface* UTerrainComponent::GetMaterial(
//     const int32 ElementIndex
// ) const {
//     // return ElementIndex == 0 ? material : nullptr;
// }

void UTerrainComponent::TickComponent(
    float DeltaTime,
    ELevelTick TickType,
    FActorComponentTickFunction* ThisTickFunction
) {
    // Super::TickComponent(DeltaTime, TickType, ThisTickFunction);

    // if (!auto_update) { return; }

    // TODO: Implement UDLOD update logic on the gpu
}

void UTerrainComponent::OnRegister() {
    // Super::OnRegister();
    // UpdateBounds();
    // if (auto_update) {
    //     update_terrain_mesh();
    // }
}

void UTerrainComponent::OnUnregister() { Super::OnUnregister(); }

#if WITH_EDITOR
void UTerrainComponent::PostEditChangeProperty(
    FPropertyChangedEvent& PropertyChangedEvent
) {
    Super::PostEditChangeProperty(PropertyChangedEvent);
    if (PropertyChangedEvent.Property != nullptr) { mark_render_state_dirty(); }
}

void UTerrainComponent::preprocess_terrain(
    bool& out_success,
    FString& out_message
) {
    const auto result = run_preprocess(
        terrain_preprocess_settings,
        preprocess::FPreprocessRunOptions{
            preprocess::EPreprocessProgressMode::EditorDialog,
            0.25f
        });
    out_success = result.has_value();
    out_message = out_success
        ? FString::Printf(
            TEXT("Preprocessing completed in %.2f seconds."),
            result.value().duration.GetTotalSeconds())
        : result.error().ToString();
}
#endif

void UTerrainComponent::update_terrain_mesh() { mark_render_state_dirty(); }

void UTerrainComponent::set_material(
    const int32 element_idx,
    UMaterialInterface* in_material
) {
    // if (element_idx == 0) {
    //     material = in_material;
    //     mark_render_state_dirty();
    // }
}

void UTerrainComponent::mark_render_state_dirty() { MarkRenderStateDirty(); }
