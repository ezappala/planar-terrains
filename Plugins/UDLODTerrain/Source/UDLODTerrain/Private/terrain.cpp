#include "terrain.h"

#include "terrain_scene_proxy.h"

UTerrain::UTerrain() {
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
    Super::GetUsedMaterials(OutMaterials, bGetDebugMaterials);
    if (material != nullptr) {
        OutMaterials.AddUnique(material);
    }
}

int32 UTerrain::GetNumMaterials() const {
    return material != nullptr ? 1 : 0;
}
